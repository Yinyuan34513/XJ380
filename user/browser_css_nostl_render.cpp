#include "./xapi/include/x3api.h"
#include "browser_css_nostl_internal.h"

/*
 * 布局阶段会把页面展开成一组扁平的绘制原语。
 * 这里这些辅助函数的职责，就是把最终可绘制的数据直接追加到 document 自己的数组里，
 * 这样绘制阶段就只需要顺序遍历，不再回头解析 DOM。
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

static int browsercss_add_image(browser_css_document *doc,
                                int x,
                                int y,
                                int width,
                                int height,
                                int source_width,
                                int source_height,
                                XCOLORA *pixels,
                                int link_index)
{
    browsercss_Image *image;

    if (doc->image_count >= BCSS_MAX_IMAGES || width <= 0 || height <= 0 ||
        source_width <= 0 || source_height <= 0 || pixels == NULL) {
        return -1;
    }

    image = &doc->images[doc->image_count];
    image->x = x;
    image->y = y;
    image->width = width;
    image->height = height;
    image->source_width = source_width;
    image->source_height = source_height;
    image->pixels = pixels;
    image->link_index = link_index;
    return doc->image_count++;
}

static int browsercss_parse_positive_attr(const char *text)
{
    int value = 0;

    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    while (*text != '\0' && *text >= '0' && *text <= '9') {
        value = value * 10 + (*text - '0');
        ++text;
    }
    return value > 0 ? value : 0;
}

/* 父级 flex 居中时，当前块里已经生成过的文字、背景、链接、图片都必须整体平移。 */
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
    for (i = ctx->first_image_index; i < doc->image_count; ++i) {
        doc->images[i].x += dx;
        doc->images[i].y += dy;
    }
}

/*
 * 行内内容是先暂存、后收束的。
 * finalize_line 在光标真正下移前，统一处理 text-align 和垂直居中调整。
 */
static void browsercss_finalize_line(browser_css_document *doc, browsercss_Context *ctx)
{
    int i;

    if ((ctx->line_run_start < 0 && ctx->line_image_start < 0) || ctx->line_height <= 0) {
        ctx->line_x = ctx->content_x;
        ctx->line_y = ctx->cursor_y;
        ctx->line_height = 0;
        ctx->line_run_start = -1;
        ctx->line_box_start = -1;
        ctx->line_link_start = -1;
        ctx->line_image_start = -1;
        return;
    }

    if (ctx->style.text_align != BCSS_TEXT_ALIGN_LEFT) {
        int line_width = 0;
        int dx = 0;

        if (ctx->line_run_start >= 0) {
            for (i = ctx->line_run_start; i < doc->run_count; ++i) {
                int run_end = (doc->runs[i].x + doc->runs[i].width) - ctx->content_x;
                if (run_end > line_width) {
                    line_width = run_end;
                }
            }
        }
        if (ctx->line_image_start >= 0) {
            for (i = ctx->line_image_start; i < doc->image_count; ++i) {
                int image_end = (doc->images[i].x + doc->images[i].width) - ctx->content_x;
                if (image_end > line_width) {
                    line_width = image_end;
                }
            }
        }

        if (ctx->style.text_align == BCSS_TEXT_ALIGN_CENTER) {
            dx = (ctx->content_width - line_width) / 2;
        } else if (ctx->style.text_align == BCSS_TEXT_ALIGN_RIGHT) {
            dx = ctx->content_width - line_width;
        }

        if (dx > 0) {
            if (ctx->line_run_start >= 0) {
                for (i = ctx->line_run_start; i < doc->run_count; ++i) {
                    doc->runs[i].x += dx;
                }
            }
            if (ctx->line_box_start >= 0) {
                for (i = ctx->line_box_start; i < doc->box_count; ++i) {
                    doc->boxes[i].x += dx;
                }
            }
            if (ctx->line_link_start >= 0) {
                for (i = ctx->line_link_start; i < doc->link_count; ++i) {
                    doc->links[i].x1 += dx;
                    doc->links[i].x2 += dx;
                }
            }
            if (ctx->line_image_start >= 0) {
                for (i = ctx->line_image_start; i < doc->image_count; ++i) {
                    doc->images[i].x += dx;
                }
            }
        }
    }

    if (ctx->line_run_start >= 0) {
        for (i = ctx->line_run_start; i < doc->run_count; ++i) {
            int adjust = ctx->line_height - doc->runs[i].height;
            if (adjust > 0) {
                doc->runs[i].y += adjust / 2;
            }
        }
    }

    ctx->cursor_y = ctx->line_y + ctx->line_height;
    ctx->line_x = ctx->content_x;
    ctx->line_y = ctx->cursor_y;
    ctx->line_height = 0;
    ctx->line_run_start = -1;
    ctx->line_box_start = -1;
    ctx->line_link_start = -1;
    ctx->line_image_start = -1;
}

static void browsercss_begin_line(browser_css_document *doc, browsercss_Context *ctx)
{
    if (ctx->line_run_start >= 0 || ctx->line_image_start >= 0) {
        return;
    }

    ctx->line_run_start = doc->run_count;
    ctx->line_box_start = doc->box_count;
    ctx->line_link_start = doc->link_count;
    ctx->line_image_start = doc->image_count;
}

static void browsercss_line_break(browser_css_document *doc, browsercss_Context *ctx)
{
    int default_line_height;

    default_line_height = ctx->style.line_height > 0 ? ctx->style.line_height : (ctx->style.font_px + 4);
    if (ctx->line_run_start >= 0 || ctx->line_image_start >= 0) {
        browsercss_finalize_line(doc, ctx);
    } else {
        ctx->cursor_y += default_line_height;
        ctx->line_x = ctx->content_x;
        ctx->line_y = ctx->cursor_y;
    }
}

/* 每个最终可见的文本片段都会变成一个 run，供绘制阶段直接重放。 */
static int browsercss_copy_text_slice(char *dst,
                                      int dst_size,
                                      const char *src,
                                      int start,
                                      int length)
{
    int i;

    if (dst == NULL || dst_size <= 0 || src == NULL || start < 0 || length <= 0) {
        return 0;
    }

    if (length > dst_size - 1) {
        length = dst_size - 1;
    }
    for (i = 0; i < length && src[start + i] != '\0'; ++i) {
        dst[i] = src[start + i];
    }
    dst[i] = '\0';
    return i;
}

static int browsercss_measure_text_slice(const char *text,
                                         int start,
                                         int length,
                                         int font_px)
{
    char buffer[BCSS_MAX_TEXT];
    int copied;

    copied = browsercss_copy_text_slice(buffer, sizeof(buffer), text, start, length);
    if (copied <= 0) {
        return 0;
    }
    return (int)xapi_CalcTextWidth(buffer, (unsigned int)font_px);
}

static int browsercss_find_fitting_text_length(const char *text,
                                               int start,
                                               int max_length,
                                               int max_width,
                                               int font_px)
{
    int low;
    int high;
    int best = 0;

    if (text == NULL || max_length <= 0) {
        return 0;
    }

    /*
     * 这里用二分而不是逐字符试探：
     * - 长文本换行会非常频繁
     * - 二分能把一次“找能放下多少字符”的成本压低
     */
    low = 1;
    high = max_length;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        int width = browsercss_measure_text_slice(text, start, mid, font_px);

        if (width <= max_width) {
            best = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return best;
}

static void browsercss_append_run_raw(browser_css_document *doc,
                                      browsercss_Context *ctx,
                                      const browsercss_Frame *frame,
                                      const char *text)
{
    /*
     * 纯文本最终都会变成 run。
     * run 是绘制阶段最小的文本单元，已经带上了位置、颜色、字号和链接信息。
     *
     * 这里顺便处理两件和现代网页关系很大的事：
     * 1. line-height
     * 2. white-space: nowrap 下禁止自动换行
     */
    browsercss_Run *run;
    int width;
    int height;
    int link_index = -1;
    int line_height;

    if (text == NULL || text[0] == '\0' || frame->hidden || doc->run_count >= BCSS_MAX_RUNS) {
        return;
    }

    width = (int)xapi_CalcTextWidth((char *)text, (unsigned int)frame->style.font_px);
    if (ctx->content_width <= 0) {
        return;
    }
    if (!frame->style.nowrap &&
        (ctx->line_run_start >= 0 || ctx->line_image_start >= 0) && ctx->line_x > ctx->content_x &&
        ctx->line_x + width > ctx->content_x + ctx->content_width) {
        /* 当前行放不下时，先收束上一行，再把这个 run 放到新行。 */
        browsercss_finalize_line(doc, ctx);
    }

    line_height = frame->style.line_height > 0 ? frame->style.line_height : (frame->style.font_px + 6);
    if (line_height < frame->style.font_px + 2) {
        line_height = frame->style.font_px + 2;
    }
    height = line_height;
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
        /* 文本 run 自己就带点击矩形，不需要额外回查 DOM。 */
        link_index = browsercss_add_link(doc,
                                         run->x,
                                         run->y,
                                         run->x + run->width,
                                         run->y + run->height,
                                         frame->href,
                                         frame->base_url);
    }
    run->link_index = link_index;

    browsercss_begin_line(doc, ctx);
    if (height > ctx->line_height) {
        ctx->line_height = height;
    }
    ctx->line_x += width;
    ++doc->run_count;
}

static void browsercss_append_run(browser_css_document *doc,
                                  browsercss_Context *ctx,
                                  const browsercss_Frame *frame,
                                  const char *text)
{
    char segment[BCSS_MAX_TEXT];
    int text_len;
    int start;

    if (text == NULL || text[0] == '\0' || frame == NULL) {
        return;
    }

    text_len = (int)strlen(text);
    if (text_len <= 0) {
        return;
    }

    /*
     * 旧逻辑把整段 token 当成一个 run。
     * 这样一旦遇到超长 URL、连续英文、无空格中文段，就会整段顶出内容区。
     *
     * 这里改成“按当前行剩余宽度切段”：
     * - 能整段放下时，保持原来的 run 粒度
     * - 放不下时，拆成多个 run
     * - 即使页面没写 overflow-wrap，也优先保证不横向炸布局
     */
    start = 0;
    while (start < text_len) {
        int remain_chars = text_len - start;
        int available_width = ctx->content_x + ctx->content_width - ctx->line_x;
        int allow_break_inside_word;
        int chunk_chars;
        int copied;

        if (ctx->content_width <= 0) {
            return;
        }

        if (available_width <= 0) {
            /* 当前行已经没有可用宽度，先真正换到下一行再继续塞剩余文本。 */
            browsercss_finalize_line(doc, ctx);
            available_width = ctx->content_width;
        }

        allow_break_inside_word = !frame->style.nowrap &&
                                  (frame->style.break_word || remain_chars > BCSS_MAX_TEXT - 1 ||
                                   browsercss_measure_text_slice(text, start, remain_chars, frame->style.font_px) > ctx->content_width);

        if (!frame->style.nowrap && allow_break_inside_word) {
            /*
             * 优先尝试把当前行剩余空间吃满，这样长链接和长单词会自然折成多行。
             * 如果这一行连一个字符都放不下，就强制开新行。
             */
            chunk_chars = browsercss_find_fitting_text_length(text,
                                                              start,
                                                              remain_chars > BCSS_MAX_TEXT - 1 ? BCSS_MAX_TEXT - 1 : remain_chars,
                                                              available_width,
                                                              frame->style.font_px);
            if (chunk_chars <= 0) {
                if (ctx->line_x > ctx->content_x) {
                    /* 前面已经有内容时，不在这一行硬塞，避免最后一个字把行高和点击区搞乱。 */
                    browsercss_finalize_line(doc, ctx);
                    continue;
                }
                /* 至少保证前进一步，避免超宽字符/异常字体度量时卡死。 */
                chunk_chars = 1;
            }
        } else {
            /*
             * 普通短文本仍然尽量保持整段提交。
             * 这样可以少生成 run，后面的点击区域和绘制遍历也更轻。
             */
            chunk_chars = remain_chars > BCSS_MAX_TEXT - 1 ? BCSS_MAX_TEXT - 1 : remain_chars;
        }

        copied = browsercss_copy_text_slice(segment, sizeof(segment), text, start, chunk_chars);
        if (copied <= 0) {
            break;
        }

        browsercss_append_run_raw(doc, ctx, frame, segment);
        start += copied;

        if (!frame->style.nowrap && start < text_len) {
            /*
             * 这一段还没写完时，主动收束当前行，让下一段从新行起。
             * 这样不会把第二段又接着塞回同一行尾巴，导致视觉上像“没换行”。
             */
            browsercss_finalize_line(doc, ctx);
        }
    }
}

static void browsercss_append_image(browser_css_document *doc,
                                    browsercss_Context *ctx,
                                    const browsercss_Frame *frame,
                                    const char *src,
                                    int attr_width,
                                    int attr_height)
{
    /*
     * <img> 目前采用“布局时同步抓取 + 同步解码”的最直接实现。
     * 这么做的优点是逻辑简单，图片一旦进了 draw list，绘制阶段就不再依赖网络。
     *
     * 这里同时负责：
     * 1. 解析 HTML width/height
     * 2. 应用 CSS width/height/max-width
     * 3. 处理 box-sizing:border-box
     * 4. 生成背景、边框、链接点击区域
     *
     * 后续如果要做异步图片加载，优先从这个函数拆“请求层”和“布局占位层”。
     */
    char *body = NULL;
    int body_len = 0;
    char final_url[768];
    XCOLORA *pixels = NULL;
    int source_width = 0;
    int source_height = 0;
    int target_width;
    int target_height;
    int border_left;
    int border_right;
    int border_top;
    int border_bottom;
    int total_width;
    int total_height;
    int outer_x;
    int outer_y;
    int image_x;
    int image_y;
    int link_index = -1;

    if (doc == NULL || ctx == NULL || frame == NULL || frame->hidden ||
        src == NULL || src[0] == '\0' || doc->callbacks.fetch_resource == NULL) {
        return;
    }

    final_url[0] = '\0';
    if (!doc->callbacks.fetch_resource(doc->callbacks.user_data,
                                       src,
                                       frame->base_url,
                                       &body,
                                       &body_len,
                                       final_url,
                                       sizeof(final_url)) ||
        body == NULL || body_len <= 0) {
        free(body);
        return;
    }

    if (!browsercss_decode_webp_image(body, body_len, &pixels, &source_width, &source_height)) {
        /* 当前图片管线先只接 WebP，其他格式这里直接失败返回。 */
        free(body);
        return;
    }
    free(body);

    target_width = frame->style.width > 0 ? frame->style.width :
                   (attr_width > 0 ? attr_width : source_width);
    target_height = frame->style.height > 0 ? frame->style.height :
                    (attr_height > 0 ? attr_height :
                     ((target_width != source_width && source_width > 0)
                          ? (source_height * target_width) / source_width
                          : source_height));
    if (target_width <= 0) {
        target_width = source_width;
    }
    if (target_height <= 0) {
        target_height = source_height;
    }
    if (frame->style.max_width > 0 && target_width > frame->style.max_width) {
        /* 限制宽度时同步保持原始宽高比。 */
        target_height = (target_height * frame->style.max_width) / target_width;
        target_width = frame->style.max_width;
    }
    if (target_width <= 0 || target_height <= 0) {
        free(pixels);
        return;
    }

    border_left = frame->style.border_left_width > 0 ? frame->style.border_left_width : frame->style.border_width;
    border_right = frame->style.border_right_width > 0 ? frame->style.border_right_width : frame->style.border_width;
    border_top = frame->style.border_top_width > 0 ? frame->style.border_top_width : frame->style.border_width;
    border_bottom = frame->style.border_bottom_width > 0 ? frame->style.border_bottom_width : frame->style.border_width;
    if (frame->style.box_sizing_border_box) {
        int extra_width = border_left + border_right + frame->style.padding_left + frame->style.padding_right;
        int extra_height = border_top + border_bottom + frame->style.padding_top + frame->style.padding_bottom;
        if (frame->style.width > 0) {
            /* border-box 下 style.width 表示整个盒子宽，所以这里要反推内容宽。 */
            target_width = frame->style.width - extra_width;
            if (target_width < 1) target_width = 1;
        }
        if (frame->style.height > 0) {
            /* 高度逻辑同上。 */
            target_height = frame->style.height - extra_height;
            if (target_height < 1) target_height = 1;
        }
    }
    total_width = frame->style.margin_left + border_left + frame->style.padding_left +
                  target_width + frame->style.padding_right + border_right +
                  frame->style.margin_right;
    total_height = frame->style.margin_top + border_top + frame->style.padding_top +
                   target_height + frame->style.padding_bottom + border_bottom +
                   frame->style.margin_bottom;

    if (frame->style.display_mode == BCSS_DISPLAY_BLOCK) {
        browsercss_finalize_line(doc, ctx);
        /* 块级图片总是从新行开始。 */
        outer_x = ctx->content_x + frame->style.margin_left;
        outer_y = ctx->cursor_y + frame->style.margin_top;
    } else {
        if (!frame->style.nowrap &&
            (ctx->line_run_start >= 0 || ctx->line_image_start >= 0) && ctx->line_x > ctx->content_x &&
            ctx->line_x + total_width > ctx->content_x + ctx->content_width) {
            /* 行内图片放不下时，也和文本一样先换行。 */
            browsercss_finalize_line(doc, ctx);
        }
        outer_x = ctx->line_x + frame->style.margin_left;
        outer_y = ctx->line_y + frame->style.margin_top;
    }

    image_x = outer_x + border_left + frame->style.padding_left;
    image_y = outer_y + border_top + frame->style.padding_top;

    if ((frame->style.props & BCSS_PROP_BACKGROUND) != 0u && frame->style.background != 0) {
        (void)browsercss_add_box(doc, outer_x, outer_y,
                                 total_width - frame->style.margin_left - frame->style.margin_right,
                                 total_height - frame->style.margin_top - frame->style.margin_bottom,
                                 frame->style.background);
    }

    if (frame->href[0] != '\0') {
        link_index = browsercss_add_link(doc,
                                         outer_x,
                                         outer_y,
                                         outer_x + total_width - frame->style.margin_left - frame->style.margin_right,
                                         outer_y + total_height - frame->style.margin_top - frame->style.margin_bottom,
                                         frame->href,
                                         frame->base_url);
    }

    if (border_top > 0) {
        (void)browsercss_add_box(doc, outer_x, outer_y,
                                 target_width + frame->style.padding_left + frame->style.padding_right + border_left + border_right,
                                 border_top,
                                 frame->style.border_color);
    }
    if (border_bottom > 0) {
        (void)browsercss_add_box(doc, outer_x,
                                 outer_y + target_height + frame->style.padding_top + frame->style.padding_bottom + border_top,
                                 target_width + frame->style.padding_left + frame->style.padding_right + border_left + border_right,
                                 border_bottom,
                                 frame->style.border_color);
    }
    if (border_left > 0) {
        (void)browsercss_add_box(doc, outer_x,
                                 outer_y + border_top,
                                 border_left,
                                 target_height + frame->style.padding_top + frame->style.padding_bottom,
                                 frame->style.border_color);
    }
    if (border_right > 0) {
        (void)browsercss_add_box(doc,
                                 outer_x + target_width + frame->style.padding_left + frame->style.padding_right + border_left,
                                 outer_y + border_top,
                                 border_right,
                                 target_height + frame->style.padding_top + frame->style.padding_bottom,
                                 frame->style.border_color);
    }

    if (browsercss_add_image(doc,
                             image_x,
                             image_y,
                             target_width,
                             target_height,
                             source_width,
                             source_height,
                             pixels,
                             link_index) < 0) {
        free(pixels);
        return;
    }

    if (frame->style.display_mode == BCSS_DISPLAY_BLOCK) {
        ctx->cursor_y = outer_y + total_height;
        ctx->line_x = ctx->content_x;
        ctx->line_y = ctx->cursor_y;
    } else {
        browsercss_begin_line(doc, ctx);
        if (total_height > ctx->line_height) {
            ctx->line_height = total_height;
        }
        ctx->line_x += total_width;
    }
}

/* 当前文本管线只先支持少量最常见的 HTML 实体。 */
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
 * 普通流里的连续空白会折叠，pre / pre-wrap 之类的块则保留原始换行和空格。
 * 处理完成后再把文本切成 run。
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

/* 打开块级元素时，计算盒子外框、内容区起点，以及背景填充对应的区域。 */
static void browsercss_open_block(browser_css_document *doc,
                                  browsercss_Context *parent,
                                  const browsercss_Frame *frame,
                                  browsercss_Context *ctx)
{
    /*
     * 打开一个块级盒子时，这里计算：
     * 1. 盒子的外框位置和宽度
     * 2. content 区域起点
     * 3. 当前子上下文的初始光标位置
     *
     * 这里已经兼容两种宽度语义：
     * - content-box: width 只表示内容宽
     * - border-box:  width 表示整个盒子宽
     *
     * 所以后面你如果继续加 min-width / box-sizing 的细化规则，
     * 主要改这里和 close_block 即可。
     */
    int available_width;
    int border_left;
    int border_right;
    int border_top;
    int border_bottom;
    int box_x;
    int box_y;
    int box_width;
    int extra_width;
    int explicit_box_width = 0;
    int auto_left = 0;
    int auto_right = 0;
    int margin_left = frame->style.margin_left;
    int margin_right = frame->style.margin_right;

    browsercss_finalize_line(doc, parent);

    border_left = frame->style.border_left_width > 0 ? frame->style.border_left_width : frame->style.border_width;
    border_right = frame->style.border_right_width > 0 ? frame->style.border_right_width : frame->style.border_width;
    border_top = frame->style.border_top_width > 0 ? frame->style.border_top_width : frame->style.border_width;
    border_bottom = frame->style.border_bottom_width > 0 ? frame->style.border_bottom_width : frame->style.border_width;
    extra_width = frame->style.padding_left + frame->style.padding_right + border_left + border_right;
    available_width = parent->content_width - margin_left - margin_right;
    if (available_width < 32) {
        available_width = 32;
    }

    box_y = parent->cursor_y + frame->style.margin_top;
    box_width = available_width;
    if (frame->style.width > 0) {
        explicit_box_width = frame->style.box_sizing_border_box ?
                             frame->style.width :
                             (frame->style.width + extra_width);
        if (explicit_box_width > 0) {
            /* content-box 和 border-box 的宽度语义在这里统一折成外框宽。 */
            box_width = explicit_box_width;
        }
    }
    if (frame->style.max_width > 0) {
        int max_box_width = frame->style.box_sizing_border_box ?
                            frame->style.max_width :
                            (frame->style.max_width + extra_width);
        if (max_box_width > 0 && box_width > max_box_width) {
            box_width = max_box_width;
        }
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
            /* 左右都 auto 时做水平居中。 */
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
    ctx->content_x = box_x + border_left + frame->style.padding_left;
    ctx->content_width = box_width - extra_width;
    if (ctx->content_width < 16) {
        ctx->content_width = 16;
    }
    ctx->content_height = frame->style.height;
    if (ctx->content_height <= 0) {
        ctx->content_height = frame->style.min_height;
    }
    ctx->content_height -= frame->style.padding_top + frame->style.padding_bottom + border_top + border_bottom;
    if (ctx->content_height < 0) {
        ctx->content_height = 0;
    }
    ctx->cursor_y = box_y + border_top + frame->style.padding_top;
    ctx->line_x = ctx->content_x;
    ctx->line_y = ctx->cursor_y;
    ctx->line_height = 0;
    ctx->line_run_start = -1;
    ctx->line_box_start = -1;
    ctx->line_link_start = -1;
    ctx->line_image_start = -1;
    ctx->box_index = -1;
    ctx->first_run_index = doc->run_count;
    ctx->first_box_index = doc->box_count;
    ctx->first_link_index = doc->link_count;
    ctx->first_image_index = doc->image_count;

    if ((frame->style.props & BCSS_PROP_BACKGROUND) != 0u && frame->style.background != 0) {
        ctx->box_index = browsercss_add_box(doc, box_x, box_y, box_width, 1, frame->style.background);
    }
}

/* 关闭块级元素时，补齐最终高度、父级 flex 调整，并生成边框对应的绘制原语。 */
static void browsercss_close_block(browser_css_document *doc,
                                   browsercss_Context *parent,
                                   browsercss_Context *ctx)
{
    int border_left;
    int border_right;
    int border_top;
    int border_bottom;
    int box_height;
    int dx = 0;
    int dy = 0;

    browsercss_finalize_line(doc, ctx);
    border_left = ctx->style.border_left_width > 0 ? ctx->style.border_left_width : ctx->style.border_width;
    border_right = ctx->style.border_right_width > 0 ? ctx->style.border_right_width : ctx->style.border_width;
    border_top = ctx->style.border_top_width > 0 ? ctx->style.border_top_width : ctx->style.border_width;
    border_bottom = ctx->style.border_bottom_width > 0 ? ctx->style.border_bottom_width : ctx->style.border_width;
    box_height = (ctx->cursor_y - ctx->box_y) + ctx->style.padding_bottom + border_bottom;
    if (ctx->style.height > 0) {
        int target_height = ctx->style.height + ctx->style.padding_top +
                            ctx->style.padding_bottom + border_top + border_bottom;
        if (box_height < target_height) {
            box_height = target_height;
        }
    }
    if (ctx->style.min_height > 0) {
        int min_height = ctx->style.min_height + ctx->style.padding_top +
                         ctx->style.padding_bottom + border_top + border_bottom;
        if (box_height < min_height) {
            box_height = min_height;
        }
    }
    if (box_height < ctx->style.font_px + 6 + ctx->style.padding_top +
                     ctx->style.padding_bottom + border_top + border_bottom) {
        box_height = ctx->style.font_px + 6 + ctx->style.padding_top +
                     ctx->style.padding_bottom + border_top + border_bottom;
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

    if (border_top > 0) {
        (void)browsercss_add_box(doc, ctx->box_x, ctx->box_y,
                                 ctx->box_width, border_top, ctx->style.border_color);
    }
    if (border_bottom > 0) {
        (void)browsercss_add_box(doc, ctx->box_x,
                                 ctx->box_y + box_height - border_bottom,
                                 ctx->box_width, border_bottom, ctx->style.border_color);
    }
    if (box_height > border_top + border_bottom) {
        if (border_left > 0) {
            (void)browsercss_add_box(doc, ctx->box_x,
                                     ctx->box_y + border_top,
                                     border_left, box_height - border_top - border_bottom,
                                     ctx->style.border_color);
        }
        if (border_right > 0) {
            (void)browsercss_add_box(doc,
                                     ctx->box_x + ctx->box_width - border_right,
                                     ctx->box_y + border_top,
                                     border_right, box_height - border_top - border_bottom,
                                     ctx->style.border_color);
        }
    }

    parent->cursor_y = ctx->box_y + box_height + ctx->style.margin_bottom;
    parent->line_y = parent->cursor_y;
    parent->line_x = parent->content_x;
}

/*
 * 这里的 HTML 遍历器故意保持简单：
 * 一边维护标签栈和块级上下文栈，一边直接产出扁平绘制列表。
 * 目标是“先能稳定显示”，而不是实现完整浏览器级 DOM。
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
    contexts[0].line_box_start = -1;
    contexts[0].line_link_start = -1;
    contexts[0].line_image_start = -1;
    contexts[0].box_index = -1;
    contexts[0].content_height = doc->viewport_height;
    contexts[0].first_run_index = 0;
    contexts[0].first_box_index = 0;
    contexts[0].first_link_index = 0;
    contexts[0].first_image_index = 0;

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
                } else if (!frame.hidden && browsercss_streq_nocase(tag, "img")) {
                    char src[768];
                    char width_text[32];
                    char height_text[32];

                    src[0] = '\0';
                    width_text[0] = '\0';
                    height_text[0] = '\0';
                    browsercss_extract_attr(tag_text, "src", src, sizeof(src));
                    browsercss_extract_attr(tag_text, "width", width_text, sizeof(width_text));
                    browsercss_extract_attr(tag_text, "height", height_text, sizeof(height_text));
                    browsercss_append_image(doc,
                                            &contexts[context_top],
                                            &frame,
                                            src,
                                            browsercss_parse_positive_attr(width_text),
                                            browsercss_parse_positive_attr(height_text));
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
 * 绘制阶段只做一件事：顺序重放前面布局阶段展开好的原语。
 * 目前还没有保留式 DOM、分层缓存或增量光栅化。
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
    int clip_bottom;

    if (document == NULL) {
        return;
    }

    clip_bottom = y + height;

    for (i = 0; i < document->box_count; ++i) {
        const browsercss_Box *box = &document->boxes[i];
        int draw_y = y + box->y - scroll_y;

        if (draw_y + box->height < y) {
            continue;
        }
        if (draw_y > clip_bottom) {
            break;
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

        if (draw_y + run->height < y) {
            continue;
        }
        if (draw_y > clip_bottom) {
            break;
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

    for (i = 0; i < document->image_count; ++i) {
        const browsercss_Image *image = &document->images[i];
        int draw_x = x + image->x;
        int draw_y = y + image->y - scroll_y;
        int clip_x1;
        int clip_y1;
        int clip_x2;
        int clip_y2;
        int visible_width;
        int visible_height;
        int dx;
        XCOLORA *buffer;

        if (draw_y + image->height < y) {
            continue;
        }
        if (draw_y > clip_bottom) {
            break;
        }

        clip_x1 = draw_x > x ? draw_x : x;
        clip_y1 = draw_y > y ? draw_y : y;
        clip_x2 = draw_x + image->width < x + width ? draw_x + image->width : x + width;
        clip_y2 = draw_y + image->height < clip_bottom ? draw_y + image->height : clip_bottom;
        visible_width = clip_x2 - clip_x1;
        visible_height = clip_y2 - clip_y1;
        if (visible_width <= 0 || visible_height <= 0) {
            continue;
        }

        buffer = (XCOLORA *)malloc((size_t)visible_width * (size_t)visible_height * sizeof(XCOLORA));
        if (buffer == NULL) {
            continue;
        }

        for (dx = 0; dx < visible_height; ++dx) {
            int src_y = ((clip_y1 - draw_y + dx) * image->source_height) / image->height;
            int sx;

            if (src_y < 0) src_y = 0;
            if (src_y >= image->source_height) src_y = image->source_height - 1;
            for (sx = 0; sx < visible_width; ++sx) {
                int src_x = ((clip_x1 - draw_x + sx) * image->source_width) / image->width;
                if (src_x < 0) src_x = 0;
                if (src_x >= image->source_width) src_x = image->source_width - 1;
                buffer[(size_t)dx * (size_t)visible_width + (size_t)sx] =
                    image->pixels[(size_t)src_y * (size_t)image->source_width + (size_t)src_x];
            }
        }

        xapi_WriteBufferA((HDLE)window,
                          (UINT32)clip_x1,
                          (UINT32)clip_y1,
                          (UINT32)visible_width,
                          (UINT32)visible_height,
                          buffer);
        free(buffer);
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

/* 链接点击命中检测直接遍历扁平链接矩形，按从上到下的顺序判断。 */
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
