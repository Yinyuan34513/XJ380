#include "./xapi/include/x3api.h"
#include "browser_css_nostl_internal.h"

/*
 * Layout emits a flat list of colored boxes and text runs. These helpers append
 * the final draw primitives directly into the document-owned arrays.
 */
static int browsercss_add_box(browser_css_document *doc,
                              int x,
                              int y,
                              int width,
                              int height,
                              unsigned int color)
{
    browsercss_Box *box;

    if (doc->box_count >= BCSS_MAX_BOXES || width <= 0 || height <= 0) {
        return -1;
    }

    box = &doc->boxes[doc->box_count];
    box->x = x;
    box->y = y;
    box->width = width;
    box->height = height;
    box->color = color;
    return doc->box_count++;
}

static int browsercss_add_link(browser_css_document *doc,
                               int x1,
                               int y1,
                               int x2,
                               int y2,
                               const char *href,
                               const char *base_url)
{
    browsercss_Link *link;

    if (doc->link_count >= BCSS_MAX_LINKS || href == NULL || href[0] == '\0') {
        return -1;
    }

    link = &doc->links[doc->link_count];
    link->x1 = x1;
    link->y1 = y1;
    link->x2 = x2;
    link->y2 = y2;
    browsercss_copy(link->href, sizeof(link->href), href);
    browsercss_copy(link->base_url, sizeof(link->base_url), base_url);
    return doc->link_count++;
}

/* When flex centering repositions a block, every generated primitive must move with it. */
static void browsercss_shift_context_range(browser_css_document *doc,
                                           const browsercss_Context *ctx,
                                           int dx,
                                           int dy)
{
    int i;

    if (dx == 0 && dy == 0) {
        return;
    }

    for (i = ctx->first_run_index; i < doc->run_count; ++i) {
        doc->runs[i].x += dx;
        doc->runs[i].y += dy;
    }
    for (i = ctx->first_box_index; i < doc->box_count; ++i) {
        doc->boxes[i].x += dx;
        doc->boxes[i].y += dy;
    }
    for (i = ctx->first_link_index; i < doc->link_count; ++i) {
        doc->links[i].x1 += dx;
        doc->links[i].y1 += dy;
        doc->links[i].x2 += dx;
        doc->links[i].y2 += dy;
    }
}

/*
 * A line is buffered as runs are emitted. Finalizing the line applies text
 * alignment and vertical centering before the cursor advances.
 */
static void browsercss_finalize_line(browser_css_document *doc, browsercss_Context *ctx)
{
    int i;

    if (ctx->line_run_start < 0 || ctx->line_height <= 0) {
        ctx->line_x = ctx->content_x;
        ctx->line_y = ctx->cursor_y;
        ctx->line_height = 0;
        ctx->line_run_start = -1;
        return;
    }

    if (ctx->style.text_align != BCSS_TEXT_ALIGN_LEFT) {
        int line_width = 0;
        int dx = 0;

        for (i = ctx->line_run_start; i < doc->run_count; ++i) {
            int run_end = (doc->runs[i].x + doc->runs[i].width) - ctx->content_x;
            if (run_end > line_width) {
                line_width = run_end;
            }
        }

        if (ctx->style.text_align == BCSS_TEXT_ALIGN_CENTER) {
            dx = (ctx->content_width - line_width) / 2;
        } else if (ctx->style.text_align == BCSS_TEXT_ALIGN_RIGHT) {
            dx = ctx->content_width - line_width;
        }

        if (dx > 0) {
            for (i = ctx->line_run_start; i < doc->run_count; ++i) {
                int link_index = doc->runs[i].link_index;

                doc->runs[i].x += dx;
                if (link_index >= 0 && link_index < doc->link_count) {
                    doc->links[link_index].x1 += dx;
                    doc->links[link_index].x2 += dx;
                }
            }
        }
    }

    for (i = ctx->line_run_start; i < doc->run_count; ++i) {
        int adjust = ctx->line_height - doc->runs[i].height;
        if (adjust > 0) {
            doc->runs[i].y += adjust / 2;
        }
    }

    ctx->cursor_y = ctx->line_y + ctx->line_height;
    ctx->line_x = ctx->content_x;
    ctx->line_y = ctx->cursor_y;
    ctx->line_height = 0;
    ctx->line_run_start = -1;
}

static void browsercss_line_break(browser_css_document *doc, browsercss_Context *ctx)
{
    if (ctx->line_run_start >= 0) {
        browsercss_finalize_line(doc, ctx);
    } else {
        ctx->cursor_y += ctx->style.font_px + 4;
        ctx->line_x = ctx->content_x;
        ctx->line_y = ctx->cursor_y;
    }
}

/* Each visible text fragment becomes a run that draw time can replay directly. */
static void browsercss_append_run(browser_css_document *doc,
                                  browsercss_Context *ctx,
                                  const browsercss_Frame *frame,
                                  const char *text)
{
    browsercss_Run *run;
    int width;
    int height;
    int link_index = -1;

    if (text == NULL || text[0] == '\0' || frame->hidden || doc->run_count >= BCSS_MAX_RUNS) {
        return;
    }

    width = (int)xapi_CalcTextWidth((char *)text, (unsigned int)frame->style.font_px);
    if (ctx->content_width <= 0) {
        return;
    }
    if (ctx->line_run_start >= 0 && ctx->line_x > ctx->content_x &&
        ctx->line_x + width > ctx->content_x + ctx->content_width) {
        browsercss_finalize_line(doc, ctx);
    }

    height = frame->style.font_px + 6;
    run = &doc->runs[doc->run_count];
    run->x = ctx->line_x;
    run->y = ctx->line_y;
    run->width = width;
    run->height = height;
    run->font_px = frame->style.font_px;
    run->color = frame->style.color;
    run->bold = frame->style.bold;
    run->underline = frame->style.underline;
    browsercss_copy(run->text, sizeof(run->text), text);

    if (frame->href[0] != '\0') {
        link_index = browsercss_add_link(doc,
                                         run->x,
                                         run->y,
                                         run->x + run->width,
                                         run->y + run->height,
                                         frame->href,
                                         frame->base_url);
    }
    run->link_index = link_index;

    if (ctx->line_run_start < 0) {
        ctx->line_run_start = doc->run_count;
    }
    if (height > ctx->line_height) {
        ctx->line_height = height;
    }
    ctx->line_x += width;
    ++doc->run_count;
}

/* Only a tiny HTML entity subset is needed for the current text pipeline. */
static int browsercss_decode_entity(const char **text)
{
    if (strncmp(*text, "&amp;", 5) == 0) {
        *text += 5;
        return '&';
    }
    if (strncmp(*text, "&lt;", 4) == 0) {
        *text += 4;
        return '<';
    }
    if (strncmp(*text, "&gt;", 4) == 0) {
        *text += 4;
        return '>';
    }
    if (strncmp(*text, "&quot;", 6) == 0) {
        *text += 6;
        return '"';
    }
    if (strncmp(*text, "&nbsp;", 6) == 0) {
        *text += 6;
        return ' ';
    }
    return -1;
}

/*
 * The renderer collapses whitespace for normal flow and preserves it for
 * preformatted blocks before emitting runs.
 */
static void browsercss_emit_text(browser_css_document *doc,
                                 browsercss_Context *ctx,
                                 const browsercss_Frame *frame,
                                 const char *text,
                                 int length)
{
    int i = 0;
    int pending_space = 0;
    char token[BCSS_MAX_TEXT];
    int token_len = 0;

    if (frame->hidden || length <= 0) {
        return;
    }

    if (frame->style.preformatted) {
        while (i < length) {
            const char *ptr = text + i;
            int decoded = *ptr == '&' ? browsercss_decode_entity(&ptr) : -1;
            char ch = decoded >= 0 ? (char)decoded : text[i];
            if (decoded >= 0) {
                i += (int)(ptr - (text + i));
            } else {
                ++i;
            }
            if (ch == '\r') {
                continue;
            }
            if (ch == '\n') {
                if (token_len > 0) {
                    token[token_len] = '\0';
                    browsercss_append_run(doc, ctx, frame, token);
                    token_len = 0;
                }
                browsercss_line_break(doc, ctx);
                continue;
            }
            if (token_len >= BCSS_MAX_TEXT - 2) {
                token[token_len] = '\0';
                browsercss_append_run(doc, ctx, frame, token);
                token_len = 0;
            }
            token[token_len++] = ch;
        }
        if (token_len > 0) {
            token[token_len] = '\0';
            browsercss_append_run(doc, ctx, frame, token);
        }
        return;
    }

    while (i < length) {
        const char *ptr = text + i;
        int decoded = *ptr == '&' ? browsercss_decode_entity(&ptr) : -1;
        char ch = decoded >= 0 ? (char)decoded : text[i];
        if (decoded >= 0) {
            i += (int)(ptr - (text + i));
        } else {
            ++i;
        }

        if (browsercss_is_space(ch)) {
            if (token_len > 0) {
                token[token_len] = '\0';
                browsercss_append_run(doc, ctx, frame, token);
                token_len = 0;
            }
            pending_space = 1;
            continue;
        }

        if (pending_space && ctx->line_x > ctx->content_x) {
            browsercss_append_run(doc, ctx, frame, " ");
        }
        pending_space = 0;

        if (token_len >= BCSS_MAX_TEXT - 2) {
            token[token_len] = '\0';
            browsercss_append_run(doc, ctx, frame, token);
            token_len = 0;
        }
        token[token_len++] = ch;
    }

    if (token_len > 0) {
        token[token_len] = '\0';
        browsercss_append_run(doc, ctx, frame, token);
    }
}

/* Opening a block resolves the element box, inner content area, and box fill. */
static void browsercss_open_block(browser_css_document *doc,
                                  browsercss_Context *parent,
                                  const browsercss_Frame *frame,
                                  browsercss_Context *ctx)
{
    int available_width;
    int border_width;
    int box_x;
    int box_y;
    int box_width;
    int extra_width;
    int auto_left = 0;
    int auto_right = 0;
    int margin_left = frame->style.margin_left;
    int margin_right = frame->style.margin_right;

    browsercss_finalize_line(doc, parent);

    border_width = frame->style.border_width > 0 ? frame->style.border_width : 0;
    extra_width = frame->style.padding_left + frame->style.padding_right + border_width * 2;
    available_width = parent->content_width - margin_left - margin_right;
    if (available_width < 32) {
        available_width = 32;
    }

    box_y = parent->cursor_y + frame->style.margin_top;
    box_width = available_width;
    if (frame->style.width > 0) {
        box_width = frame->style.width + extra_width;
    }
    if (frame->style.max_width > 0 && box_width > frame->style.max_width) {
        box_width = frame->style.max_width;
    }
    if (box_width < 32) {
        box_width = 32;
    }

    auto_left = frame->style.margin_left_auto;
    auto_right = frame->style.margin_right_auto;
    if (auto_left || auto_right) {
        int remain = parent->content_width - box_width - margin_left - margin_right;
        if (remain < 0) {
            remain = 0;
        }
        if (auto_left && auto_right) {
            margin_left += remain / 2;
            margin_right += remain - (remain / 2);
        } else if (auto_left) {
            margin_left += remain;
        } else {
            margin_right += remain;
        }
    }

    box_x = parent->content_x + margin_left;

    ctx->style = frame->style;
    ctx->box_x = box_x;
    ctx->box_y = box_y;
    ctx->box_width = box_width;
    ctx->content_x = box_x + border_width + frame->style.padding_left;
    ctx->content_width = box_width - extra_width;
    if (ctx->content_width < 16) {
        ctx->content_width = 16;
    }
    ctx->content_height = frame->style.height;
    if (ctx->content_height <= 0) {
        ctx->content_height = frame->style.min_height;
    }
    ctx->content_height -= frame->style.padding_top + frame->style.padding_bottom + border_width * 2;
    if (ctx->content_height < 0) {
        ctx->content_height = 0;
    }
    ctx->cursor_y = box_y + border_width + frame->style.padding_top;
    ctx->line_x = ctx->content_x;
    ctx->line_y = ctx->cursor_y;
    ctx->line_height = 0;
    ctx->line_run_start = -1;
    ctx->box_index = -1;
    ctx->first_run_index = doc->run_count;
    ctx->first_box_index = doc->box_count;
    ctx->first_link_index = doc->link_count;

    if ((frame->style.props & BCSS_PROP_BACKGROUND) != 0u && frame->style.background != 0) {
        ctx->box_index = browsercss_add_box(doc, box_x, box_y, box_width, 1, frame->style.background);
    }
}

/* Closing a block finalizes size, flex adjustments, fill, and border primitives. */
static void browsercss_close_block(browser_css_document *doc,
                                   browsercss_Context *parent,
                                   browsercss_Context *ctx)
{
    int border_width;
    int box_height;
    int dx = 0;
    int dy = 0;

    browsercss_finalize_line(doc, ctx);
    border_width = ctx->style.border_width > 0 ? ctx->style.border_width : 0;
    box_height = (ctx->cursor_y - ctx->box_y) + ctx->style.padding_bottom + border_width;
    if (ctx->style.height > 0) {
        int target_height = ctx->style.height + ctx->style.padding_top +
                            ctx->style.padding_bottom + border_width * 2;
        if (box_height < target_height) {
            box_height = target_height;
        }
    }
    if (ctx->style.min_height > 0) {
        int min_height = ctx->style.min_height + ctx->style.padding_top +
                         ctx->style.padding_bottom + border_width * 2;
        if (box_height < min_height) {
            box_height = min_height;
        }
    }
    if (box_height < ctx->style.font_px + 6 + ctx->style.padding_top +
                     ctx->style.padding_bottom + border_width * 2) {
        box_height = ctx->style.font_px + 6 + ctx->style.padding_top +
                     ctx->style.padding_bottom + border_width * 2;
    }

    if (parent->style.display_mode == BCSS_DISPLAY_FLEX) {
        if (parent->style.align_center) {
            int target_x = parent->content_x + (parent->content_width - ctx->box_width) / 2;
            dx = target_x - ctx->box_x;
        }
        if (parent->style.justify_center && parent->content_height > box_height) {
            int target_y = parent->box_y + parent->style.padding_top +
                           (parent->content_height - box_height) / 2 + ctx->style.margin_top;
            dy = target_y - ctx->box_y;
        }
    }

    if (dx != 0 || dy != 0) {
        browsercss_shift_context_range(doc, ctx, dx, dy);
        ctx->box_x += dx;
        ctx->box_y += dy;
        ctx->content_x += dx;
        ctx->cursor_y += dy;
        ctx->line_x += dx;
        ctx->line_y += dy;
    }

    if (ctx->box_index >= 0) {
        doc->boxes[ctx->box_index].x = ctx->box_x;
        doc->boxes[ctx->box_index].y = ctx->box_y;
        doc->boxes[ctx->box_index].width = ctx->box_width;
        doc->boxes[ctx->box_index].height = box_height;
    }

    if (border_width > 0) {
        (void)browsercss_add_box(doc, ctx->box_x, ctx->box_y,
                                 ctx->box_width, border_width, ctx->style.border_color);
        (void)browsercss_add_box(doc, ctx->box_x,
                                 ctx->box_y + box_height - border_width,
                                 ctx->box_width, border_width, ctx->style.border_color);
        if (box_height > border_width * 2) {
            (void)browsercss_add_box(doc, ctx->box_x,
                                     ctx->box_y + border_width,
                                     border_width, box_height - border_width * 2,
                                     ctx->style.border_color);
            (void)browsercss_add_box(doc,
                                     ctx->box_x + ctx->box_width - border_width,
                                     ctx->box_y + border_width,
                                     border_width, box_height - border_width * 2,
                                     ctx->style.border_color);
        }
    }

    parent->cursor_y = ctx->box_y + box_height + ctx->style.margin_bottom;
    parent->line_y = parent->cursor_y;
    parent->line_x = parent->content_x;
}

/*
 * The HTML walker is intentionally simple: it builds a frame stack and a
 * parallel block-context stack, then emits flat draw primitives.
 */
void browsercss_layout_html(browser_css_document *doc, const char *html)
{
    browsercss_Frame frames[BCSS_MAX_STACK];
    browsercss_Context contexts[BCSS_MAX_STACK];
    int frame_top = 0;
    int context_top = 0;
    const char *p = html;

    doc->run_count = 0;
    doc->box_count = 0;
    doc->link_count = 0;

    browsercss_style_reset(&frames[0].style);
    frames[0].style.display_mode = BCSS_DISPLAY_BLOCK;
    browsercss_copy(frames[0].tag, sizeof(frames[0].tag), "body");
    frames[0].href[0] = '\0';
    browsercss_copy(frames[0].base_url, sizeof(frames[0].base_url), doc->base_url);
    frames[0].hidden = 0;
    frames[0].is_block = 1;
    frames[0].has_context = 1;

    memset(&contexts[0], 0, sizeof(contexts[0]));
    contexts[0].style = frames[0].style;
    contexts[0].box_x = 0;
    contexts[0].box_y = 0;
    contexts[0].box_width = doc->viewport_width;
    contexts[0].content_x = 0;
    contexts[0].content_width = doc->viewport_width;
    contexts[0].cursor_y = 0;
    contexts[0].line_x = 0;
    contexts[0].line_y = 0;
    contexts[0].line_height = 0;
    contexts[0].line_run_start = -1;
    contexts[0].box_index = -1;
    contexts[0].content_height = doc->viewport_height;
    contexts[0].first_run_index = 0;
    contexts[0].first_box_index = 0;
    contexts[0].first_link_index = 0;

    while (*p != '\0') {
        if (*p == '<') {
            if (strncmp(p, "<!--", 4) == 0) {
                const char *end = strstr(p + 4, "-->");
                p = end != NULL ? end + 3 : p + 4;
                continue;
            }

            if (p[1] == '/') {
                char tag[BCSS_MAX_TAG];
                const char *end = strchr(p, '>');
                if (end == NULL) {
                    break;
                }
                browsercss_extract_tag_name(p + 2, tag, sizeof(tag));
                while (frame_top > 0 && !browsercss_streq_nocase(frames[frame_top].tag, tag)) {
                    if (frames[frame_top].has_context && context_top > 0) {
                        browsercss_close_block(doc, &contexts[context_top - 1], &contexts[context_top]);
                        --context_top;
                    }
                    --frame_top;
                }
                if (frame_top > 0) {
                    if (frames[frame_top].has_context && context_top > 0) {
                        browsercss_close_block(doc, &contexts[context_top - 1], &contexts[context_top]);
                        --context_top;
                    }
                    --frame_top;
                }
                p = end + 1;
                continue;
            } else {
                char tag_text[512];
                char tag[BCSS_MAX_TAG];
                char class_name[BCSS_MAX_SELECTOR];
                char id[BCSS_MAX_SELECTOR];
                char inline_style[256];
                char href[768];
                int self_close;
                browsercss_Frame frame;
                const char *end = strchr(p, '>');

                if (end == NULL) {
                    break;
                }
                {
                    int len = (int)(end - p - 1);
                    if (len >= (int)sizeof(tag_text)) {
                        len = (int)sizeof(tag_text) - 1;
                    }
                    memcpy(tag_text, p + 1, (size_t)len);
                    tag_text[len] = '\0';
                }
                browsercss_extract_tag_name(tag_text, tag, sizeof(tag));
                if (tag[0] == '\0') {
                    p = end + 1;
                    continue;
                }

                self_close = tag_text[strlen(tag_text) - 1] == '/' ||
                             browsercss_streq_nocase(tag, "br") ||
                             browsercss_streq_nocase(tag, "img") ||
                             browsercss_streq_nocase(tag, "meta") ||
                             browsercss_streq_nocase(tag, "link") ||
                             browsercss_streq_nocase(tag, "base");

                if (browsercss_streq_nocase(tag, "style") || browsercss_streq_nocase(tag, "script") ||
                    browsercss_streq_nocase(tag, "title") || browsercss_streq_nocase(tag, "head")) {
                    const char closing_prefix[] = "</";
                    char closing[32];
                    const char *close;

                    snprintf(closing, sizeof(closing), "%s%s>", closing_prefix, tag);
                    close = browsercss_find_closing(end + 1, closing);
                    p = close != NULL ? close + strlen(closing) : end + 1;
                    continue;
                }

                class_name[0] = '\0';
                id[0] = '\0';
                inline_style[0] = '\0';
                href[0] = '\0';
                browsercss_extract_attr(tag_text, "class", class_name, sizeof(class_name));
                browsercss_extract_attr(tag_text, "id", id, sizeof(id));
                browsercss_extract_attr(tag_text, "style", inline_style, sizeof(inline_style));
                browsercss_extract_attr(tag_text, "href", href, sizeof(href));

                memset(&frame, 0, sizeof(frame));
                browsercss_copy(frame.tag, sizeof(frame.tag), tag);
                browsercss_copy(frame.class_name, sizeof(frame.class_name), class_name);
                browsercss_copy(frame.id, sizeof(frame.id), id);
                browsercss_copy(frame.base_url, sizeof(frame.base_url), frames[frame_top].base_url);
                browsercss_copy(frame.href, sizeof(frame.href), href);
                frame.hidden = frames[frame_top].hidden;
                browsercss_compute_style(doc,
                                         frames,
                                         frame_top,
                                         &frames[frame_top].style,
                                         tag,
                                         class_name,
                                         id,
                                         inline_style,
                                         &frame.style);
                frame.is_block = frame.style.display_mode == BCSS_DISPLAY_BLOCK ||
                                 frame.style.display_mode == BCSS_DISPLAY_FLEX;
                if (frame.style.display_mode == BCSS_DISPLAY_NONE) {
                    frame.hidden = 1;
                }

                if (browsercss_streq_nocase(tag, "a") && frame.href[0] == '\0') {
                    browsercss_copy(frame.href, sizeof(frame.href), frames[frame_top].href);
                }

                if (!frame.hidden && browsercss_streq_nocase(tag, "br")) {
                    browsercss_line_break(doc, &contexts[context_top]);
                } else if (!frame.hidden && frame.is_block) {
                    browsercss_Context *ctx;

                    if (frame_top + 1 < BCSS_MAX_STACK && context_top + 1 < BCSS_MAX_STACK) {
                        ++frame_top;
                        frames[frame_top] = frame;
                        ++context_top;
                        ctx = &contexts[context_top];
                        browsercss_open_block(doc, &contexts[context_top - 1], &frames[frame_top], ctx);
                        frames[frame_top].has_context = 1;
                    }
                } else if (frame_top + 1 < BCSS_MAX_STACK) {
                    ++frame_top;
                    frames[frame_top] = frame;
                    frames[frame_top].has_context = 0;
                }

                if (self_close && frame_top > 0 && browsercss_streq_nocase(frames[frame_top].tag, tag)) {
                    if (frames[frame_top].has_context && context_top > 0) {
                        browsercss_close_block(doc, &contexts[context_top - 1], &contexts[context_top]);
                        --context_top;
                    }
                    --frame_top;
                }
                p = end + 1;
                continue;
            }
        }

        {
            const char *end = strchr(p, '<');
            int len = end != NULL ? (int)(end - p) : (int)strlen(p);
            browsercss_emit_text(doc, &contexts[context_top], &frames[frame_top], p, len);
            if (end == NULL) {
                break;
            }
            p = end;
        }
    }

    while (context_top > 0) {
        browsercss_close_block(doc, &contexts[context_top - 1], &contexts[context_top]);
        --context_top;
    }
    browsercss_finalize_line(doc, &contexts[0]);
    doc->content_height = contexts[0].cursor_y + 12;
    if (doc->content_height < doc->viewport_height) {
        doc->content_height = doc->viewport_height;
    }
}

/*
 * Paint simply replays the flattened primitives. There is no retained DOM or
 * incremental raster stage yet.
 */
extern "C" void browser_css_draw(browser_css_document *document,
                                 unsigned long long window,
                                 int x,
                                 int y,
                                 int scroll_y,
                                 int width,
                                 int height)
{
    int i;

    if (document == NULL) {
        return;
    }

    for (i = 0; i < document->box_count; ++i) {
        const browsercss_Box *box = &document->boxes[i];
        int draw_y = y + box->y - scroll_y;

        if (draw_y + box->height < y || draw_y > y + height) {
            continue;
        }
        xapi_DrawRect((HDLE)window,
                      (UINT32)(x + box->x),
                      (UINT32)draw_y,
                      (UINT32)(x + box->x + box->width),
                      (UINT32)(draw_y + box->height),
                      box->color,
                      true);
    }

    for (i = 0; i < document->run_count; ++i) {
        const browsercss_Run *run = &document->runs[i];
        int draw_y = y + run->y - scroll_y;
        int draw_x = x + run->x;

        if (draw_y + run->height < y || draw_y > y + height) {
            continue;
        }
        xapi_DrawText((HDLE)window,
                      (UINT32)draw_x,
                      (UINT32)draw_y,
                      (char *)run->text,
                      (UINT32)run->font_px,
                      run->color);
        if (run->bold) {
            xapi_DrawText((HDLE)window,
                          (UINT32)(draw_x + 1),
                          (UINT32)draw_y,
                          (char *)run->text,
                          (UINT32)run->font_px,
                          run->color);
        }
        if (run->underline) {
            int line_y = draw_y + run->font_px + 1;
            xapi_DrawLine((HDLE)window,
                          (UINT32)draw_x,
                          (UINT32)line_y,
                          (UINT32)(draw_x + run->width-15),
                          (UINT32)line_y,
                          run->color);
        }
    }
}

extern "C" int browser_css_height(const browser_css_document *document)
{
    return document == NULL ? 0 : document->content_height;
}

extern "C" const char *browser_css_title(const browser_css_document *document)
{
    if (document == NULL) {
        return "";
    }
    return document->title;
}

extern "C" int browser_css_mouse_move(browser_css_document *document, int x, int y, int scroll_y)
{
    (void)document;
    (void)x;
    (void)y;
    (void)scroll_y;
    return 0;
}

extern "C" int browser_css_mouse_leave(browser_css_document *document)
{
    (void)document;
    return 0;
}

/* Link hit testing walks the flattened link rectangles from topmost to bottom. */
extern "C" int browser_css_mouse_click(browser_css_document *document, int x, int y, int scroll_y)
{
    int i;

    if (document == NULL) {
        return 0;
    }

    y += scroll_y;
    for (i = document->link_count - 1; i >= 0; --i) {
        const browsercss_Link *link = &document->links[i];

        if (x >= link->x1 && x <= link->x2 && y >= link->y1 && y <= link->y2) {
            if (document->callbacks.navigate != NULL) {
                document->callbacks.navigate(document->callbacks.user_data, link->href, link->base_url);
            }
            return 1;
        }
    }
    return 0;
}
