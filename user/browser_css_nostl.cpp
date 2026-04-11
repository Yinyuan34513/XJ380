#include "./xapi/include/x3api.h"
#include "browser_css_nostl_internal.h"
#include "lexbor/css/css.h"

int browsercss_is_space(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

static int browsercss_is_digit(char ch)
{
    return ch >= '0' && ch <= '9';
}

static char browsercss_to_lower(char ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        return (char)(ch - 'A' + 'a');
    }
    return ch;
}

int browsercss_streq_nocase(const char *lhs, const char *rhs)
{
    if (lhs == NULL || rhs == NULL) {
        return lhs == rhs;
    }

    while (*lhs != '\0' && *rhs != '\0') {
        if (browsercss_to_lower(*lhs) != browsercss_to_lower(*rhs)) {
            return 0;
        }
        ++lhs;
        ++rhs;
    }
    return *lhs == '\0' && *rhs == '\0';
}

static int browsercss_contains_word_nocase(const char *text, const char *word)
{
    int i;
    int word_len;

    if (text == NULL || word == NULL) {
        return 0;
    }

    word_len = (int)strlen(word);
    if (word_len <= 0) {
        return 0;
    }

    for (i = 0; text[i] != '\0'; ++i) {
        int j = 0;
        while (j < word_len && text[i + j] != '\0' &&
               browsercss_to_lower(text[i + j]) == browsercss_to_lower(word[j])) {
            ++j;
        }
        if (j == word_len) {
            char prev = i > 0 ? text[i - 1] : ' ';
            char next = text[i + j];
            int prev_ok = browsercss_is_space(prev) || prev == '"' || prev == '\'' || prev == ';' || prev == ',';
            int next_ok = browsercss_is_space(next) || next == '\0' || next == '"' || next == '\'' || next == ';' || next == ',';
            if (prev_ok && next_ok) {
                return 1;
            }
        }
    }
    return 0;
}

static int browsercss_starts_with_nocase(const char *text, const char *prefix)
{
    if (text == NULL || prefix == NULL) {
        return 0;
    }
    while (*prefix != '\0') {
        if (*text == '\0' || browsercss_to_lower(*text) != browsercss_to_lower(*prefix)) {
            return 0;
        }
        ++text;
        ++prefix;
    }
    return 1;
}

void browsercss_copy(char *dst, int dst_size, const char *src)
{
    int i = 0;
    if (dst == NULL || dst_size <= 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    while (i < dst_size - 1 && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static void browsercss_trim(char *text)
{
    int len;
    int start = 0;
    int end;
    int i;

    if (text == NULL) {
        return;
    }

    len = (int)strlen(text);
    while (start < len && browsercss_is_space(text[start])) {
        ++start;
    }

    end = len;
    while (end > start && browsercss_is_space(text[end - 1])) {
        --end;
    }

    if (start > 0) {
        for (i = 0; i < end - start; ++i) {
            text[i] = text[start + i];
        }
        text[end - start] = '\0';
    } else {
        text[end] = '\0';
    }
}

static int browsercss_parse_int(const char *text)
{
    int sign = 1;
    int value = 0;

    if (text == NULL) {
        return 0;
    }
    while (browsercss_is_space(*text)) {
        ++text;
    }
    if (*text == '-') {
        sign = -1;
        ++text;
    }
    while (browsercss_is_digit(*text)) {
        value = value * 10 + (*text - '0');
        ++text;
    }
    return value * sign;
}

static int browsercss_parse_length(browser_css_document *doc, const char *text, int font_px)
{
    int value;
    int i;
    char unit[8];

    if (text == NULL || text[0] == '\0') {
        return 0;
    }

    value = browsercss_parse_int(text);
    i = 0;
    while (text[i] != '\0' && (browsercss_is_digit(text[i]) || text[i] == '-' || text[i] == '.')) {
        ++i;
    }
    browsercss_copy(unit, sizeof(unit), text + i);
    browsercss_trim(unit);

    if (unit[0] == '\0' || browsercss_streq_nocase(unit, "px")) {
        return value;
    }
    if (browsercss_streq_nocase(unit, "pt")) {
        return (value * 4) / 3;
    }
    if (browsercss_streq_nocase(unit, "em")) {
        return value * font_px;
    }
    if (browsercss_streq_nocase(unit, "rem")) {
        return value * BCSS_DEFAULT_FONT_PX;
    }
    if (browsercss_streq_nocase(unit, "%")) {
        return (font_px * value) / 100;
    }
    if (browsercss_streq_nocase(unit, "vh")) {
        return (doc->viewport_height * value) / 100;
    }
    if (browsercss_streq_nocase(unit, "vw")) {
        return (doc->viewport_width * value) / 100;
    }
    return value;
}

static unsigned int browsercss_rgba(unsigned int r, unsigned int g, unsigned int b, unsigned int a)
{
    return ((r & 0xffu) << 24) | ((g & 0xffu) << 16) | ((b & 0xffu) << 8) | (a & 0xffu);
}

static int browsercss_hex(char ch)
{
    ch = browsercss_to_lower(ch);
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}

static unsigned int browsercss_named_color(const char *name, int *ok)
{
    *ok = 1;
    if (browsercss_streq_nocase(name, "black")) return browsercss_rgba(0, 0, 0, 255);
    if (browsercss_streq_nocase(name, "white")) return browsercss_rgba(255, 255, 255, 255);
    if (browsercss_streq_nocase(name, "red")) return browsercss_rgba(220, 38, 38, 255);
    if (browsercss_streq_nocase(name, "green")) return browsercss_rgba(22, 163, 74, 255);
    if (browsercss_streq_nocase(name, "blue")) return browsercss_rgba(37, 99, 235, 255);
    if (browsercss_streq_nocase(name, "gray") || browsercss_streq_nocase(name, "grey")) return browsercss_rgba(107, 114, 128, 255);
    if (browsercss_streq_nocase(name, "silver")) return browsercss_rgba(203, 213, 225, 255);
    if (browsercss_streq_nocase(name, "yellow")) return browsercss_rgba(250, 204, 21, 255);
    if (browsercss_streq_nocase(name, "orange")) return browsercss_rgba(249, 115, 22, 255);
    if (browsercss_streq_nocase(name, "purple")) return browsercss_rgba(147, 51, 234, 255);
    if (browsercss_streq_nocase(name, "transparent")) return browsercss_rgba(0, 0, 0, 0);
    *ok = 0;
    return 0;
}

static int browsercss_parse_color(const char *text, unsigned int *color)
{
    char value[64];
    int ok;

    if (text == NULL || color == NULL) {
        return 0;
    }

    browsercss_copy(value, sizeof(value), text);
    browsercss_trim(value);
    if (value[0] == '#') {
        int len = (int)strlen(value);
        if (len == 4) {
            int r = browsercss_hex(value[1]);
            int g = browsercss_hex(value[2]);
            int b = browsercss_hex(value[3]);
            if (r < 0 || g < 0 || b < 0) return 0;
            *color = browsercss_rgba((unsigned int)(r * 17), (unsigned int)(g * 17), (unsigned int)(b * 17), 255);
            return 1;
        }
        if (len == 7) {
            int r1 = browsercss_hex(value[1]);
            int r2 = browsercss_hex(value[2]);
            int g1 = browsercss_hex(value[3]);
            int g2 = browsercss_hex(value[4]);
            int b1 = browsercss_hex(value[5]);
            int b2 = browsercss_hex(value[6]);
            if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0) return 0;
            *color = browsercss_rgba((unsigned int)(r1 * 16 + r2),
                                     (unsigned int)(g1 * 16 + g2),
                                     (unsigned int)(b1 * 16 + b2),
                                     255);
            return 1;
        }
        return 0;
    }

    if (browsercss_contains_word_nocase(value, "rgb(")) {
        const char *p = strchr(value, '(');
        int r, g, b;
        if (p == NULL) return 0;
        ++p;
        r = browsercss_parse_int(p);
        p = strchr(p, ',');
        if (p == NULL) return 0;
        g = browsercss_parse_int(p + 1);
        p = strchr(p + 1, ',');
        if (p == NULL) return 0;
        b = browsercss_parse_int(p + 1);
        if (r < 0) r = 0;
        if (r > 255) r = 255;
        if (g < 0) g = 0;
        if (g > 255) g = 255;
        if (b < 0) b = 0;
        if (b > 255) b = 255;
        *color = browsercss_rgba((unsigned int)r, (unsigned int)g, (unsigned int)b, 255);
        return 1;
    }

    *color = browsercss_named_color(value, &ok);
    return ok;
}

void browsercss_style_reset(browsercss_Style *style)
{
    memset(style, 0, sizeof(*style));
    style->font_px = BCSS_DEFAULT_FONT_PX;
    style->color = BCSS_TEXT_COLOR;
    style->display_mode = BCSS_DISPLAY_INLINE;
    style->text_align = BCSS_TEXT_ALIGN_LEFT;
    style->border_color = BCSS_TEXT_COLOR;
}

static void browsercss_style_inherit(const browsercss_Style *parent, browsercss_Style *style)
{
    browsercss_style_reset(style);
    if (parent != NULL) {
        style->font_px = parent->font_px;
        style->color = parent->color;
        style->bold = parent->bold;
        style->underline = parent->underline;
        style->preformatted = parent->preformatted;
        style->text_align = parent->text_align;
    }
}

static void browsercss_style_apply(browsercss_Style *dst, const browsercss_Style *src)
{
    if ((src->props & BCSS_PROP_COLOR) != 0u) dst->color = src->color;
    if ((src->props & BCSS_PROP_BACKGROUND) != 0u) dst->background = src->background;
    if ((src->props & BCSS_PROP_FONT_SIZE) != 0u) dst->font_px = src->font_px;
    if ((src->props & BCSS_PROP_DISPLAY) != 0u) dst->display_mode = src->display_mode;
    if ((src->props & BCSS_PROP_BOLD) != 0u) dst->bold = src->bold;
    if ((src->props & BCSS_PROP_UNDERLINE) != 0u) dst->underline = src->underline;
    if ((src->props & BCSS_PROP_MARGIN_TOP) != 0u) dst->margin_top = src->margin_top;
    if ((src->props & BCSS_PROP_MARGIN_RIGHT) != 0u) dst->margin_right = src->margin_right;
    if ((src->props & BCSS_PROP_MARGIN_BOTTOM) != 0u) dst->margin_bottom = src->margin_bottom;
    if ((src->props & BCSS_PROP_MARGIN_LEFT) != 0u) dst->margin_left = src->margin_left;
    if ((src->props & BCSS_PROP_PADDING_TOP) != 0u) dst->padding_top = src->padding_top;
    if ((src->props & BCSS_PROP_PADDING_RIGHT) != 0u) dst->padding_right = src->padding_right;
    if ((src->props & BCSS_PROP_PADDING_BOTTOM) != 0u) dst->padding_bottom = src->padding_bottom;
    if ((src->props & BCSS_PROP_PADDING_LEFT) != 0u) dst->padding_left = src->padding_left;
    if ((src->props & BCSS_PROP_PRE) != 0u) dst->preformatted = src->preformatted;
    if ((src->props & BCSS_PROP_MIN_HEIGHT) != 0u) dst->min_height = src->min_height;
    if ((src->props & BCSS_PROP_MAX_WIDTH) != 0u) dst->max_width = src->max_width;
    if ((src->props & BCSS_PROP_WIDTH) != 0u) dst->width = src->width;
    if ((src->props & BCSS_PROP_HEIGHT) != 0u) dst->height = src->height;
    if ((src->props & BCSS_PROP_JUSTIFY) != 0u) dst->justify_center = src->justify_center;
    if ((src->props & BCSS_PROP_ALIGN) != 0u) dst->align_center = src->align_center;
    if ((src->props & BCSS_PROP_TEXT_ALIGN) != 0u) dst->text_align = src->text_align;
    if ((src->props & BCSS_PROP_MARGIN_LEFT) != 0u) dst->margin_left_auto = 0;
    if ((src->props & BCSS_PROP_MARGIN_RIGHT) != 0u) dst->margin_right_auto = 0;
    if ((src->props & BCSS_PROP_MARGIN_LEFT_AUTO) != 0u) {
        dst->margin_left_auto = src->margin_left_auto;
        if (src->margin_left_auto) dst->margin_left = 0;
    }
    if ((src->props & BCSS_PROP_MARGIN_RIGHT_AUTO) != 0u) {
        dst->margin_right_auto = src->margin_right_auto;
        if (src->margin_right_auto) dst->margin_right = 0;
    }
    if ((src->props & BCSS_PROP_BORDER_WIDTH) != 0u) dst->border_width = src->border_width;
    if ((src->props & BCSS_PROP_BORDER_COLOR) != 0u) dst->border_color = src->border_color;
}

static void browsercss_style_apply_tag_defaults(const char *tag, browsercss_Style *style)
{
    if (browsercss_streq_nocase(tag, "body") || browsercss_streq_nocase(tag, "div") ||
        browsercss_streq_nocase(tag, "section") || browsercss_streq_nocase(tag, "article") ||
        browsercss_streq_nocase(tag, "main") || browsercss_streq_nocase(tag, "header") ||
        browsercss_streq_nocase(tag, "footer") || browsercss_streq_nocase(tag, "nav") ||
        browsercss_streq_nocase(tag, "p") || browsercss_streq_nocase(tag, "ul") ||
        browsercss_streq_nocase(tag, "ol") || browsercss_streq_nocase(tag, "li") ||
        browsercss_streq_nocase(tag, "pre") || browsercss_streq_nocase(tag, "blockquote")) {
        style->display_mode = BCSS_DISPLAY_BLOCK;
    }

    if (browsercss_streq_nocase(tag, "h1")) {
        style->display_mode = BCSS_DISPLAY_BLOCK;
        style->font_px = 30;
        style->bold = 1;
        style->margin_top = 14;
        style->margin_bottom = 10;
    } else if (browsercss_streq_nocase(tag, "h2")) {
        style->display_mode = BCSS_DISPLAY_BLOCK;
        style->font_px = 26;
        style->bold = 1;
        style->margin_top = 12;
        style->margin_bottom = 8;
    } else if (browsercss_streq_nocase(tag, "h3")) {
        style->display_mode = BCSS_DISPLAY_BLOCK;
        style->font_px = 22;
        style->bold = 1;
        style->margin_top = 10;
        style->margin_bottom = 6;
    } else if (browsercss_streq_nocase(tag, "h4") || browsercss_streq_nocase(tag, "h5") || browsercss_streq_nocase(tag, "h6")) {
        style->display_mode = BCSS_DISPLAY_BLOCK;
        style->font_px = 18;
        style->bold = 1;
        style->margin_top = 8;
        style->margin_bottom = 6;
    } else if (browsercss_streq_nocase(tag, "a")) {
        style->color = BCSS_LINK_COLOR;
        style->underline = 1;
    } else if (browsercss_streq_nocase(tag, "strong") || browsercss_streq_nocase(tag, "b")) {
        style->bold = 1;
    } else if (browsercss_streq_nocase(tag, "code")) {
        if (style->font_px > 12) style->font_px -= 1;
    } else if (browsercss_streq_nocase(tag, "pre")) {
        style->preformatted = 1;
        style->padding_top = 8;
        style->padding_right = 8;
        style->padding_bottom = 8;
        style->padding_left = 8;
        style->background = browsercss_rgba(241, 245, 249, 255);
    }
}

static int browsercss_is_selector_ident_char(char ch)
{
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') ||
           ch == '_' || ch == '-';
}

static int browsercss_parse_selector_part(browsercss_SelectorPart *part, const char *text)
{
    int i;
    int tag_pos = 0;
    int id_pos = 0;
    int mode = 0;
    int class_pos = 0;

    if (part == NULL || text == NULL || text[0] == '\0') {
        return 0;
    }

    memset(part, 0, sizeof(*part));

    for (i = 0; text[i] != '\0'; ++i) {
        char ch = text[i];
        if (!browsercss_is_selector_ident_char(ch) && ch != '.' && ch != '#' && ch != '*') {
            return 0;
        }
        if (ch == '*') {
            if (mode != 0 || tag_pos != 0) {
                return 0;
            }
            part->universal = 1;
            continue;
        }
        if (ch == '.') {
            mode = 1;
            if (part->class_count >= BCSS_MAX_SELECTOR_CLASSES) {
                return 0;
            }
            class_pos = 0;
            continue;
        }
        if (ch == '#') {
            mode = 2;
            continue;
        }
        if (mode == 0) {
            if (part->universal) {
                return 0;
            }
            if (tag_pos < (int)sizeof(part->tag) - 1) {
                part->tag[tag_pos++] = browsercss_to_lower(ch);
            }
        } else if (mode == 1) {
            if (class_pos < (int)sizeof(part->classes[part->class_count]) - 1) {
                part->classes[part->class_count][class_pos++] = browsercss_to_lower(ch);
            }
        } else {
            if (id_pos < (int)sizeof(part->id) - 1) {
                part->id[id_pos++] = browsercss_to_lower(ch);
            }
        }

        if (mode == 1) {
            if (text[i + 1] == '.' || text[i + 1] == '#' || text[i + 1] == '\0') {
                if (class_pos <= 0) {
                    return 0;
                }
                ++part->class_count;
            }
        }
    }

    if (part->tag[0] == '\0' && !part->universal &&
        part->id[0] == '\0' && part->class_count <= 0) {
        return 0;
    }

    return 1;
}

static int browsercss_parse_selector(browsercss_Rule *rule, char *selector)
{
    char token[128];
    int token_len = 0;
    int combinator = BCSS_COMBINATOR_NONE;
    int saw_space = 0;
    const char *p;

    if (rule == NULL || selector == NULL) {
        return 0;
    }

    browsercss_trim(selector);
    if (selector[0] == '\0') {
        return 0;
    }

    memset(rule, 0, sizeof(*rule));
    p = selector;
    while (*p != '\0') {
        if (*p == ':' || *p == '[' || *p == ']' || *p == '+' || *p == '~') {
            return 0;
        }

        if (browsercss_is_space(*p)) {
            saw_space = 1;
            ++p;
            continue;
        }

        if (*p == '>') {
            if (rule->part_count <= 0) {
                return 0;
            }
            combinator = BCSS_COMBINATOR_CHILD;
            saw_space = 0;
            ++p;
            continue;
        }

        token_len = 0;
        while (*p != '\0' && !browsercss_is_space(*p) && *p != '>') {
            if (token_len < (int)sizeof(token) - 1) {
                token[token_len++] = *p;
            }
            ++p;
        }
        token[token_len] = '\0';
        browsercss_trim(token);
        if (token[0] == '\0' || rule->part_count >= BCSS_MAX_SELECTOR_PARTS) {
            return 0;
        }

        if (!browsercss_parse_selector_part(&rule->parts[rule->part_count], token)) {
            return 0;
        }
        rule->parts[rule->part_count].combinator_from_prev =
            rule->part_count == 0 ? BCSS_COMBINATOR_NONE :
            (combinator != BCSS_COMBINATOR_NONE ? combinator :
             (saw_space ? BCSS_COMBINATOR_DESCENDANT : BCSS_COMBINATOR_DESCENDANT));
        ++rule->part_count;
        combinator = BCSS_COMBINATOR_NONE;
        saw_space = 0;
    }

    return rule->part_count > 0;
}

static void browsercss_parse_box_shorthand(browser_css_document *doc, browsercss_Style *style, unsigned int bit_top, const char *value)
{
    int numbers[4];
    int autos[4];
    int count = 0;
    const char *p = value;
    char token[32];
    int token_len;

    memset(numbers, 0, sizeof(numbers));
    memset(autos, 0, sizeof(autos));

    while (*p != '\0' && count < 4) {
        while (browsercss_is_space(*p)) ++p;
        if (*p == '\0') break;
        token_len = 0;
        while (*p != '\0' && !browsercss_is_space(*p) && token_len < (int)sizeof(token) - 1) {
            token[token_len++] = *p++;
        }
        token[token_len] = '\0';
        if (bit_top == BCSS_PROP_MARGIN_TOP && browsercss_streq_nocase(token, "auto")) {
            autos[count] = 1;
            numbers[count++] = 0;
        } else {
            numbers[count++] = browsercss_parse_length(doc, token,
                                                       style->font_px > 0 ? style->font_px : BCSS_DEFAULT_FONT_PX);
        }
    }

    if (count == 1) {
        numbers[1] = numbers[2] = numbers[3] = numbers[0];
        autos[1] = autos[2] = autos[3] = autos[0];
    } else if (count == 2) {
        numbers[2] = numbers[0];
        numbers[3] = numbers[1];
        autos[2] = autos[0];
        autos[3] = autos[1];
    } else if (count == 3) {
        numbers[3] = numbers[1];
        autos[3] = autos[1];
    }

    if (count > 0) {
        if (bit_top == BCSS_PROP_MARGIN_TOP) {
            style->margin_top = numbers[0];
            if (autos[1]) {
                style->margin_right_auto = 1;
                style->props |= BCSS_PROP_MARGIN_RIGHT_AUTO;
            } else {
                style->margin_right = numbers[1];
                style->props |= BCSS_PROP_MARGIN_RIGHT;
            }
            style->margin_bottom = numbers[2];
            if (autos[3]) {
                style->margin_left_auto = 1;
                style->props |= BCSS_PROP_MARGIN_LEFT_AUTO;
            } else {
                style->margin_left = numbers[3];
                style->props |= BCSS_PROP_MARGIN_LEFT;
            }
            style->props |= BCSS_PROP_MARGIN_TOP | BCSS_PROP_MARGIN_BOTTOM;
        } else {
            style->padding_top = numbers[0];
            style->padding_right = numbers[1];
            style->padding_bottom = numbers[2];
            style->padding_left = numbers[3];
            style->props |= BCSS_PROP_PADDING_TOP | BCSS_PROP_PADDING_RIGHT | BCSS_PROP_PADDING_BOTTOM | BCSS_PROP_PADDING_LEFT;
        }
    }
}

static void browsercss_parse_border_shorthand(browser_css_document *doc,
                                              browsercss_Style *style,
                                              const char *value)
{
    const char *p = value;
    char token[32];
    int token_len;

    while (*p != '\0') {
        unsigned int color;

        while (browsercss_is_space(*p)) {
            ++p;
        }
        if (*p == '\0') {
            break;
        }

        token_len = 0;
        while (*p != '\0' && !browsercss_is_space(*p) &&
               token_len < (int)sizeof(token) - 1) {
            token[token_len++] = *p++;
        }
        token[token_len] = '\0';

        if (browsercss_parse_color(token, &color)) {
            style->border_color = color;
            style->props |= BCSS_PROP_BORDER_COLOR;
            continue;
        }

        if (browsercss_streq_nocase(token, "none")) {
            style->border_width = 0;
            style->props |= BCSS_PROP_BORDER_WIDTH;
            continue;
        }

        if (!browsercss_streq_nocase(token, "solid") &&
            !browsercss_streq_nocase(token, "dashed") &&
            !browsercss_streq_nocase(token, "dotted") &&
            !browsercss_streq_nocase(token, "double")) {
            style->border_width = browsercss_parse_length(doc, token,
                                                          style->font_px > 0 ? style->font_px : BCSS_DEFAULT_FONT_PX);
            style->props |= BCSS_PROP_BORDER_WIDTH;
        }
    }
}

static void browsercss_parse_declaration(browser_css_document *doc,
                                         browsercss_Style *style,
                                         const char *name,
                                         const char *value)
{
    int font_px;

    if (doc == NULL || style == NULL || name == NULL || value == NULL ||
        name[0] == '\0' || value[0] == '\0') {
        return;
    }

    font_px = style->font_px > 0 ? style->font_px : BCSS_DEFAULT_FONT_PX;

    if (browsercss_streq_nocase(name, "color")) {
        if (browsercss_parse_color(value, &style->color)) {
            style->props |= BCSS_PROP_COLOR;
        }
    } else if (browsercss_streq_nocase(name, "background-color") ||
               browsercss_streq_nocase(name, "background")) {
        if (browsercss_parse_color(value, &style->background)) {
            style->props |= BCSS_PROP_BACKGROUND;
        }
    } else if (browsercss_streq_nocase(name, "font-size")) {
        style->font_px = browsercss_parse_length(doc, value, BCSS_DEFAULT_FONT_PX);
        if (style->font_px < 10) {
            style->font_px = 10;
        }
        style->props |= BCSS_PROP_FONT_SIZE;
    } else if (browsercss_streq_nocase(name, "font-weight")) {
        style->bold = browsercss_streq_nocase(value, "bold") || browsercss_parse_int(value) >= 600;
        style->props |= BCSS_PROP_BOLD;
    } else if (browsercss_streq_nocase(name, "text-decoration")) {
        style->underline = browsercss_contains_word_nocase(value, "underline");
        style->props |= BCSS_PROP_UNDERLINE;
    } else if (browsercss_streq_nocase(name, "display")) {
        if (browsercss_streq_nocase(value, "none")) {
            style->display_mode = BCSS_DISPLAY_NONE;
        } else if (browsercss_streq_nocase(value, "block")) {
            style->display_mode = BCSS_DISPLAY_BLOCK;
        } else if (browsercss_streq_nocase(value, "flex")) {
            style->display_mode = BCSS_DISPLAY_FLEX;
        } else {
            style->display_mode = BCSS_DISPLAY_INLINE;
        }
        style->props |= BCSS_PROP_DISPLAY;
    } else if (browsercss_streq_nocase(name, "justify-content")) {
        style->justify_center = browsercss_streq_nocase(value, "center");
        style->props |= BCSS_PROP_JUSTIFY;
    } else if (browsercss_streq_nocase(name, "align-items")) {
        style->align_center = browsercss_streq_nocase(value, "center");
        style->props |= BCSS_PROP_ALIGN;
    } else if (browsercss_streq_nocase(name, "text-align")) {
        if (browsercss_streq_nocase(value, "center")) {
            style->text_align = BCSS_TEXT_ALIGN_CENTER;
        } else if (browsercss_streq_nocase(value, "right")) {
            style->text_align = BCSS_TEXT_ALIGN_RIGHT;
        } else {
            style->text_align = BCSS_TEXT_ALIGN_LEFT;
        }
        style->props |= BCSS_PROP_TEXT_ALIGN;
    } else if (browsercss_streq_nocase(name, "min-height")) {
        style->min_height = browsercss_parse_length(doc, value, font_px);
        style->props |= BCSS_PROP_MIN_HEIGHT;
    } else if (browsercss_streq_nocase(name, "max-width")) {
        style->max_width = browsercss_parse_length(doc, value, font_px);
        style->props |= BCSS_PROP_MAX_WIDTH;
    } else if (browsercss_streq_nocase(name, "width")) {
        style->width = browsercss_parse_length(doc, value, font_px);
        style->props |= BCSS_PROP_WIDTH;
    } else if (browsercss_streq_nocase(name, "height")) {
        style->height = browsercss_parse_length(doc, value, font_px);
        style->props |= BCSS_PROP_HEIGHT;
    } else if (browsercss_streq_nocase(name, "white-space")) {
        style->preformatted = browsercss_streq_nocase(value, "pre") ||
                              browsercss_streq_nocase(value, "pre-wrap");
        style->props |= BCSS_PROP_PRE;
    } else if (browsercss_streq_nocase(name, "margin")) {
        browsercss_parse_box_shorthand(doc, style, BCSS_PROP_MARGIN_TOP, value);
    } else if (browsercss_streq_nocase(name, "padding")) {
        browsercss_parse_box_shorthand(doc, style, BCSS_PROP_PADDING_TOP, value);
    } else if (browsercss_streq_nocase(name, "margin-top")) {
        style->margin_top = browsercss_parse_length(doc, value, font_px);
        style->props |= BCSS_PROP_MARGIN_TOP;
    } else if (browsercss_streq_nocase(name, "margin-right")) {
        if (browsercss_streq_nocase(value, "auto")) {
            style->margin_right_auto = 1;
            style->props |= BCSS_PROP_MARGIN_RIGHT_AUTO;
        } else {
            style->margin_right = browsercss_parse_length(doc, value, font_px);
            style->props |= BCSS_PROP_MARGIN_RIGHT;
        }
    } else if (browsercss_streq_nocase(name, "margin-bottom")) {
        style->margin_bottom = browsercss_parse_length(doc, value, font_px);
        style->props |= BCSS_PROP_MARGIN_BOTTOM;
    } else if (browsercss_streq_nocase(name, "margin-left")) {
        if (browsercss_streq_nocase(value, "auto")) {
            style->margin_left_auto = 1;
            style->props |= BCSS_PROP_MARGIN_LEFT_AUTO;
        } else {
            style->margin_left = browsercss_parse_length(doc, value, font_px);
            style->props |= BCSS_PROP_MARGIN_LEFT;
        }
    } else if (browsercss_streq_nocase(name, "padding-top")) {
        style->padding_top = browsercss_parse_length(doc, value, font_px);
        style->props |= BCSS_PROP_PADDING_TOP;
    } else if (browsercss_streq_nocase(name, "padding-right")) {
        style->padding_right = browsercss_parse_length(doc, value, font_px);
        style->props |= BCSS_PROP_PADDING_RIGHT;
    } else if (browsercss_streq_nocase(name, "padding-bottom")) {
        style->padding_bottom = browsercss_parse_length(doc, value, font_px);
        style->props |= BCSS_PROP_PADDING_BOTTOM;
    } else if (browsercss_streq_nocase(name, "padding-left")) {
        style->padding_left = browsercss_parse_length(doc, value, font_px);
        style->props |= BCSS_PROP_PADDING_LEFT;
    } else if (browsercss_streq_nocase(name, "border")) {
        browsercss_parse_border_shorthand(doc, style, value);
    } else if (browsercss_streq_nocase(name, "border-width")) {
        style->border_width = browsercss_parse_length(doc, value, font_px);
        style->props |= BCSS_PROP_BORDER_WIDTH;
    } else if (browsercss_streq_nocase(name, "border-color")) {
        if (browsercss_parse_color(value, &style->border_color)) {
            style->props |= BCSS_PROP_BORDER_COLOR;
        }
    }
}

static void browsercss_parse_declarations(browser_css_document *doc, browsercss_Style *style, const char *block)
{
    const char *p = block;
    char name[64];
    char value[128];

    while (*p != '\0') {
        int name_len = 0;
        int value_len = 0;
        while (*p != '\0' && browsercss_is_space(*p)) ++p;
        while (*p != '\0' && *p != ':' && *p != ';' && name_len < (int)sizeof(name) - 1) {
            name[name_len++] = browsercss_to_lower(*p++);
        }
        name[name_len] = '\0';
        if (*p != ':') {
            while (*p != '\0' && *p != ';') ++p;
            if (*p == ';') ++p;
            continue;
        }
        ++p;
        while (*p != '\0' && *p != ';' && value_len < (int)sizeof(value) - 1) {
            value[value_len++] = *p++;
        }
        value[value_len] = '\0';
        if (*p == ';') ++p;

        browsercss_trim(name);
        browsercss_trim(value);
        browsercss_parse_declaration(doc, style, name, value);
    }
}

static void browsercss_strip_comments(char *css)
{
    int read_pos = 0;
    int write_pos = 0;
    int in_comment = 0;

    while (css[read_pos] != '\0') {
        if (!in_comment && css[read_pos] == '/' && css[read_pos + 1] == '*') {
            in_comment = 1;
            read_pos += 2;
            continue;
        }
        if (in_comment && css[read_pos] == '*' && css[read_pos + 1] == '/') {
            in_comment = 0;
            read_pos += 2;
            continue;
        }
        if (!in_comment) {
            css[write_pos++] = css[read_pos];
        }
        ++read_pos;
    }
    css[write_pos] = '\0';
}

static int browsercss_add_rule_with_style(browser_css_document *doc,
                                          const char *selector_text,
                                          const browsercss_Style *decl)
{
    char            *selectors;
    char            *sel_ptr;
    int              added = 0;

    if (doc == NULL || selector_text == NULL || decl == NULL) {
        return 0;
    }

    selectors = (char *)malloc(strlen(selector_text) + 1);
    if (selectors == NULL) {
        return 0;
    }

    strcpy(selectors, selector_text);

    sel_ptr = selectors;
    while (*sel_ptr != '\0' && doc->rule_count < BCSS_MAX_RULES) {
        browsercss_Rule *rule = &doc->rules[doc->rule_count];
        char item[256];
        int item_len = 0;

        while (*sel_ptr != '\0' && browsercss_is_space(*sel_ptr)) {
            ++sel_ptr;
        }

        while (*sel_ptr != '\0' && *sel_ptr != ',' && item_len < (int)sizeof(item) - 1) {
            item[item_len++] = *sel_ptr++;
        }
        item[item_len] = '\0';

        if (*sel_ptr == ',') {
            ++sel_ptr;
        }

        memset(rule, 0, sizeof(*rule));
        if (!browsercss_parse_selector(rule, item)) {
            continue;
        }

        rule->style = *decl;
        ++doc->rule_count;
        ++added;
    }

    free(selectors);
    return added;
}

static const lxb_css_rule_list_t *browsercss_lexbor_at_rule_block(const lxb_css_rule_at_t *at)
{
    if (at == NULL) {
        return NULL;
    }

    if (at->type == LXB_CSS_AT_RULE_MEDIA) {
        return at->u.media != NULL ? at->u.media->block : NULL;
    }
    if (at->type == LXB_CSS_AT_RULE_FONT_FACE) {
        return at->u.font_face != NULL ? at->u.font_face->block : NULL;
    }
    if (at->type == LXB_CSS_AT_RULE__UNDEF) {
        return at->u.undef != NULL ? at->u.undef->block : NULL;
    }
    if (at->type == LXB_CSS_AT_RULE__CUSTOM) {
        return at->u.custom != NULL ? at->u.custom->block : NULL;
    }

    return NULL;
}

static int browsercss_copy_css_slice_trim(char *dst,
                                          int dst_size,
                                          const char *css_text,
                                          size_t begin,
                                          size_t end)
{
    size_t css_len;
    size_t copy_len;

    if (dst == NULL || dst_size <= 0) {
        return 0;
    }

    dst[0] = '\0';
    if (css_text == NULL || begin >= end) {
        return 0;
    }

    css_len = strlen(css_text);
    if (begin >= css_len) {
        return 0;
    }
    if (end > css_len) {
        end = css_len;
    }
    if (begin >= end) {
        return 0;
    }

    copy_len = end - begin;
    if (copy_len >= (size_t)dst_size) {
        copy_len = (size_t)dst_size - 1;
    }

    memcpy(dst, css_text + begin, copy_len);
    dst[copy_len] = '\0';
    browsercss_trim(dst);
    return dst[0] != '\0';
}

static void browsercss_fill_style_from_lexbor_declarations(browser_css_document *doc,
                                                           browsercss_Style *style,
                                                           const lxb_css_rule_declaration_list_t *list,
                                                           const char *css_text)
{
    const lxb_css_rule_t *rule;

    if (doc == NULL || style == NULL || list == NULL || css_text == NULL) {
        return;
    }

    for (rule = list->first; rule != NULL; rule = rule->next) {
        const lxb_css_rule_declaration_t *decl;
        char name[64];
        char value[128];

        if (rule->type != LXB_CSS_RULE_DECLARATION) {
            continue;
        }

        decl = lxb_css_rule_declaration(rule);
        if (decl == NULL) {
            continue;
        }

        if (!browsercss_copy_css_slice_trim(name, sizeof(name), css_text,
                                            decl->offset.name_begin,
                                            decl->offset.name_end) ||
            !browsercss_copy_css_slice_trim(value, sizeof(value), css_text,
                                            decl->offset.value_begin,
                                            decl->offset.value_end)) {
            continue;
        }

        {
            int i;
            for (i = 0; name[i] != '\0'; ++i) {
                name[i] = browsercss_to_lower(name[i]);
            }
        }

        browsercss_parse_declaration(doc, style, name, value);
    }
}

static void browsercss_collect_lexbor_rules(browser_css_document *doc,
                                            const lxb_css_rule_list_t *list,
                                            const char *css_text)
{
    const lxb_css_rule_t *rule;

    if (doc == NULL || list == NULL || css_text == NULL) {
        return;
    }

    for (rule = list->first; rule != NULL && doc->rule_count < BCSS_MAX_RULES; rule = rule->next) {
        if (rule->type == LXB_CSS_RULE_STYLE) {
            const lxb_css_rule_style_t *style = lxb_css_rule_style(rule);
            browsercss_Style decl;
            char selector_text[256];

            if (style == NULL) {
                continue;
            }

            if (browsercss_copy_css_slice_trim(selector_text, sizeof(selector_text), css_text,
                                               style->prelude_begin, style->prelude_end)) {
                browsercss_style_reset(&decl);
                browsercss_fill_style_from_lexbor_declarations(doc, &decl,
                                                               style->declarations,
                                                               css_text);
                (void)browsercss_add_rule_with_style(doc, selector_text, &decl);
            }

            if (style->child != NULL) {
                browsercss_collect_lexbor_rules(doc, style->child, css_text);
            }
        } else if (rule->type == LXB_CSS_RULE_AT_RULE) {
            browsercss_collect_lexbor_rules(doc,
                                            browsercss_lexbor_at_rule_block(lxb_css_rule_at(rule)),
                                            css_text);
        } else if (rule->type == LXB_CSS_RULE_LIST) {
            browsercss_collect_lexbor_rules(doc, lxb_css_rule_list(rule), css_text);
        }
    }
}

static int browsercss_parse_css_lexbor(browser_css_document *doc, const char *css_text)
{
    lxb_css_parser_t *parser;
    lxb_css_stylesheet_t *stylesheet;
    lxb_status_t status;
    int ok = 0;

    if (doc == NULL || css_text == NULL || css_text[0] == '\0') {
        return 0;
    }

    parser = lxb_css_parser_create();
    if (parser == NULL) {
        return 0;
    }

    status = lxb_css_parser_init(parser, NULL);
    if (status != LXB_STATUS_OK) {
        (void)lxb_css_parser_destroy(parser, true);
        return 0;
    }

    status = lxb_css_parser_selectors_init(parser);
    if (status != LXB_STATUS_OK) {
        (void)lxb_css_parser_destroy(parser, true);
        return 0;
    }

    stylesheet = lxb_css_stylesheet_create(NULL);
    if (stylesheet == NULL) {
        (void)lxb_css_parser_destroy(parser, true);
        return 0;
    }

    status = lxb_css_stylesheet_parse(stylesheet,
                                      parser,
                                      (const lxb_char_t *)css_text,
                                      strlen(css_text));
    if (status == LXB_STATUS_OK && stylesheet->root != NULL &&
        stylesheet->root->type == LXB_CSS_RULE_LIST) {
        browsercss_collect_lexbor_rules(doc,
                                        lxb_css_rule_list(stylesheet->root),
                                        css_text);
        ok = 1;
    }

    (void)lxb_css_stylesheet_destroy(stylesheet, true);
    (void)lxb_css_parser_destroy(parser, true);
    return ok;
}

/* Fall back to a tiny block parser only when Lexbor rejects the stylesheet. */
static void browsercss_parse_css_fallback(browser_css_document *doc, const char *css_text)
{
    char *buffer;
    char *p;

    if (doc == NULL || css_text == NULL || css_text[0] == '\0') {
        return;
    }

    buffer = (char *)malloc(strlen(css_text) + 1);
    if (buffer == NULL) {
        return;
    }
    strcpy(buffer, css_text);
    browsercss_strip_comments(buffer);

    p = buffer;
    while (*p != '\0' && doc->rule_count < BCSS_MAX_RULES) {
        char selector[128];
        char block[256];
        int selector_len = 0;
        int block_len = 0;
        char *brace;
        char *end;
        char *sel_ptr;

        while (*p != '\0' && browsercss_is_space(*p)) ++p;
        if (*p == '\0') break;
        brace = strchr(p, '{');
        if (brace == NULL) break;
        while (p < brace && selector_len < (int)sizeof(selector) - 1) {
            selector[selector_len++] = *p++;
        }
        selector[selector_len] = '\0';
        p = brace + 1;
        end = strchr(p, '}');
        if (end == NULL) break;
        while (p < end && block_len < (int)sizeof(block) - 1) {
            block[block_len++] = *p++;
        }
        block[block_len] = '\0';
        p = end + 1;

        sel_ptr = selector;
        while (*sel_ptr != '\0' && doc->rule_count < BCSS_MAX_RULES) {
            browsercss_Rule *rule = &doc->rules[doc->rule_count];
            char item[128];
            int item_len = 0;
            browsercss_Style decl;

            while (*sel_ptr != '\0' && browsercss_is_space(*sel_ptr)) ++sel_ptr;
            while (*sel_ptr != '\0' && *sel_ptr != ',' && item_len < (int)sizeof(item) - 1) {
                item[item_len++] = *sel_ptr++;
            }
            item[item_len] = '\0';
            if (*sel_ptr == ',') ++sel_ptr;

            browsercss_style_reset(&decl);
            browsercss_parse_declarations(doc, &decl, block);
            memset(rule, 0, sizeof(*rule));
            if (!browsercss_parse_selector(rule, item)) {
                continue;
            }
            rule->style = decl;
            ++doc->rule_count;
        }
    }

    free(buffer);
}

static void browsercss_parse_css(browser_css_document *doc, const char *css_text)
{
    if (!browsercss_parse_css_lexbor(doc, css_text)) {
        browsercss_parse_css_fallback(doc, css_text);
    }
}

const char *browsercss_find_closing(const char *text, const char *closing)
{
    int close_len;
    const char *p;
    int i;

    if (text == NULL || closing == NULL) {
        return NULL;
    }

    close_len = (int)strlen(closing);
    for (p = text; *p != '\0'; ++p) {
        for (i = 0; i < close_len; ++i) {
            if (p[i] == '\0' || browsercss_to_lower(p[i]) != browsercss_to_lower(closing[i])) {
                break;
            }
        }
        if (i == close_len) {
            return p;
        }
    }
    return NULL;
}

void browsercss_extract_tag_name(const char *tag_text, char *tag, int tag_size)
{
    int i = 0;
    while (*tag_text != '\0' && browsercss_is_space(*tag_text)) ++tag_text;
    while (*tag_text != '\0' && !browsercss_is_space(*tag_text) && *tag_text != '/' && *tag_text != '>' &&
           i < tag_size - 1) {
        tag[i++] = browsercss_to_lower(*tag_text++);
    }
    tag[i] = '\0';
}

int browsercss_extract_attr(const char *tag_text, const char *name, char *out, int out_size)
{
    int name_len = (int)strlen(name);
    const char *p = tag_text;

    if (out != NULL && out_size > 0) out[0] = '\0';

    while (*p != '\0') {
        while (*p != '\0' && browsercss_is_space(*p)) ++p;
        if (*p == '\0' || *p == '/' || *p == '>') break;
        {
            const char *key = p;
            int key_len = 0;
            char quote;
            while (*p != '\0' && !browsercss_is_space(*p) && *p != '=' && *p != '>' && *p != '/') {
                ++p;
                ++key_len;
            }
            while (*p != '\0' && browsercss_is_space(*p)) ++p;
            if (*p != '=') {
                continue;
            }
            ++p;
            while (*p != '\0' && browsercss_is_space(*p)) ++p;
            quote = (*p == '"' || *p == '\'') ? *p++ : '\0';
            if (key_len == name_len) {
                int i;
                int match = 1;
                for (i = 0; i < key_len; ++i) {
                    if (browsercss_to_lower(key[i]) != browsercss_to_lower(name[i])) {
                        match = 0;
                        break;
                    }
                }
                if (match && out != NULL && out_size > 0) {
                    int out_len = 0;
                    while (*p != '\0' && ((quote != '\0' && *p != quote) ||
                           (quote == '\0' && !browsercss_is_space(*p) && *p != '>')) &&
                           out_len < out_size - 1) {
                        out[out_len++] = *p++;
                    }
                    out[out_len] = '\0';
                    return 1;
                }
            }

            while (*p != '\0' && ((quote != '\0' && *p != quote) ||
                   (quote == '\0' && !browsercss_is_space(*p) && *p != '>'))) {
                ++p;
            }
            if (quote != '\0' && *p == quote) ++p;
        }
    }
    return 0;
}

static void browsercss_extract_title(browser_css_document *doc, const char *html)
{
    const char *open;
    const char *start;
    const char *end;
    int len;

    doc->title[0] = '\0';
    open = browsercss_find_closing(html, "<title");
    if (open == NULL) return;
    start = strchr(open, '>');
    if (start == NULL) return;
    ++start;
    end = browsercss_find_closing(start, "</title>");
    if (end == NULL) return;
    len = (int)(end - start);
    if (len >= (int)sizeof(doc->title)) len = (int)sizeof(doc->title) - 1;
    memcpy(doc->title, start, (size_t)len);
    doc->title[len] = '\0';
    browsercss_trim(doc->title);
}

static int browsercss_selector_part_matches(const browsercss_SelectorPart *part,
                                            const char *tag,
                                            const char *class_name,
                                            const char *id)
{
    int i;

    if (part == NULL) {
        return 0;
    }

    if (!part->universal && part->tag[0] != '\0' && !browsercss_streq_nocase(part->tag, tag)) {
        return 0;
    }
    if (part->id[0] != '\0' && !browsercss_streq_nocase(part->id, id)) {
        return 0;
    }
    for (i = 0; i < part->class_count; ++i) {
        if (!browsercss_contains_word_nocase(class_name, part->classes[i])) {
            return 0;
        }
    }
    return 1;
}

static int browsercss_rule_matches(const browsercss_Rule *rule,
                                   const browsercss_Frame *frames,
                                   int frame_top,
                                   const char *tag,
                                   const char *class_name,
                                   const char *id)
{
    int part_index;
    int ancestor_index;

    if (rule == NULL || rule->part_count <= 0) {
        return 0;
    }

    if (!browsercss_selector_part_matches(&rule->parts[rule->part_count - 1], tag, class_name, id)) {
        return 0;
    }

    ancestor_index = frame_top;
    for (part_index = rule->part_count - 2; part_index >= 0; --part_index) {
        int combinator = rule->parts[part_index + 1].combinator_from_prev;
        int found = 0;

        if (combinator == BCSS_COMBINATOR_CHILD) {
            if (ancestor_index < 0) {
                return 0;
            }
            if (!browsercss_selector_part_matches(&rule->parts[part_index],
                                                  frames[ancestor_index].tag,
                                                  frames[ancestor_index].class_name,
                                                  frames[ancestor_index].id)) {
                return 0;
            }
            --ancestor_index;
            continue;
        }

        while (ancestor_index >= 0) {
            if (browsercss_selector_part_matches(&rule->parts[part_index],
                                                 frames[ancestor_index].tag,
                                                 frames[ancestor_index].class_name,
                                                 frames[ancestor_index].id)) {
                found = 1;
                --ancestor_index;
                break;
            }
            --ancestor_index;
        }

        if (!found) {
            return 0;
        }
    }

    return 1;
}

void browsercss_compute_style(browser_css_document *doc,
                              const browsercss_Frame *frames,
                              int frame_top,
                              const browsercss_Style *parent,
                              const char *tag,
                              const char *class_name,
                              const char *id,
                              const char *inline_style,
                              browsercss_Style *out)
{
    browsercss_Style inline_decl;
    int i;

    browsercss_style_inherit(parent, out);
    browsercss_style_apply_tag_defaults(tag, out);
    for (i = 0; i < doc->rule_count; ++i) {
        if (browsercss_rule_matches(&doc->rules[i], frames, frame_top, tag, class_name, id)) {
            browsercss_style_apply(out, &doc->rules[i].style);
        }
    }
    browsercss_style_reset(&inline_decl);
    browsercss_parse_declarations(doc, &inline_decl, inline_style);
    browsercss_style_apply(out, &inline_decl);
}

static void browsercss_prepare_css(browser_css_document *doc, const char *html, const char *base_url)
{
    const char *p = html;
    doc->rule_count = 0;

    while (*p != '\0') {
        if (*p != '<') {
            ++p;
            continue;
        }

        if (browsercss_starts_with_nocase(p, "<style")) {
            const char *open_end = strchr(p, '>');
            const char *close = browsercss_find_closing(p, "</style>");
            if (open_end != NULL && close != NULL && close > open_end) {
                int len = (int)(close - open_end - 1);
                char *css = (char *)malloc((size_t)len + 1);
                if (css != NULL) {
                    memcpy(css, open_end + 1, (size_t)len);
                    css[len] = '\0';
                    browsercss_parse_css(doc, css);
                    free(css);
                }
                p = close + 8;
                continue;
            }
        }

        if (browsercss_starts_with_nocase(p, "<link")) {
            const char *tag_end = strchr(p, '>');
            if (tag_end != NULL) {
                char tag_text[512];
                int len = (int)(tag_end - p - 1);
                char rel[128];
                char href[768];
                if (len >= (int)sizeof(tag_text)) len = (int)sizeof(tag_text) - 1;
                memcpy(tag_text, p + 1, (size_t)len);
                tag_text[len] = '\0';
                if (browsercss_extract_attr(tag_text, "rel", rel, sizeof(rel)) &&
                    browsercss_extract_attr(tag_text, "href", href, sizeof(href)) &&
                    browsercss_contains_word_nocase(rel, "stylesheet") &&
                    doc->callbacks.fetch_resource != NULL) {
                    char *body = NULL;
                    int body_len = 0;
                    char final_url[768];
                    final_url[0] = '\0';
                    if (doc->callbacks.fetch_resource(doc->callbacks.user_data,
                                                      href,
                                                      base_url,
                                                      &body,
                                                      &body_len,
                                                      final_url,
                                                      sizeof(final_url)) &&
                        body != NULL) {
                        browsercss_parse_css(doc, body);
                        free(body);
                    }
                }
                p = tag_end + 1;
                continue;
            }
        }

        ++p;
    }
}

/* The main translation unit now owns parsing, style matching, and document setup only. */
extern "C" browser_css_document *browser_css_create(const browser_css_callbacks *callbacks)
{
    browser_css_document *doc = (browser_css_document *)malloc(sizeof(browser_css_document));
    if (doc == NULL) {
        return NULL;
    }
    memset(doc, 0, sizeof(*doc));
    if (callbacks != NULL) {
        doc->callbacks = *callbacks;
    }
    return doc;
}

extern "C" void browser_css_destroy(browser_css_document *document)
{
    if (document != NULL) {
        free(document);
    }
}

extern "C" void browser_css_clear(browser_css_document *document)
{
    if (document == NULL) return;
    document->rule_count = 0;
    document->run_count = 0;
    document->box_count = 0;
    document->link_count = 0;
    document->content_height = 0;
    document->title[0] = '\0';
}

/* Loading HTML runs in three stages: title extraction, stylesheet loading, then layout. */
extern "C" int browser_css_load_html(browser_css_document *document,
                                     const char *html,
                                     const char *base_url,
                                     int viewport_width,
                                     int viewport_height)
{
    if (document == NULL || html == NULL || viewport_width <= 0 || viewport_height <= 0) {
        return 0;
    }

    browser_css_clear(document);
    document->viewport_width = viewport_width;
    document->viewport_height = viewport_height;
    browsercss_copy(document->base_url, sizeof(document->base_url), base_url);
    browsercss_extract_title(document, html);
    browsercss_prepare_css(document, html, document->base_url);
    browsercss_layout_html(document, html);
    if (document->title[0] == '\0') {
        browsercss_copy(document->title, sizeof(document->title), document->base_url);
    }
    return document->run_count > 0 || document->box_count > 0;
}
