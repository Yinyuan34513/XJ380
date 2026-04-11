#include "./xapi/include/x3api.h"
#include "browser_css_nostl_internal.h"

#ifndef BROWSER_NOSTL_DISABLE_LEXBOR
#include "lexbor/css/css.h"
#endif

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

static int browsercss_parse_number_scaled(const char *text, int scale)
{
    /*
     * 现代站点大量使用小数长度，例如 0.875rem / 1.5em。
     * 旧实现只取整数，会导致字号、边距、行高整体发散。
     *
     * 这里不引入浮点解析器，直接做一个“定点数读取”：
     * - scale=100 表示保留两位小数。
     * - 返回值仍然是整数，但语义是“原值 * scale”。
     * 这样后面可以继续用整数运算，适合当前 freestanding 环境。
     */
    long long value = 0;
    long long frac = 0;
    long long frac_scale = 1;
    int sign = 1;
    int saw_digit = 0;

    if (text == NULL || scale <= 0) {
        return 0;
    }

    while (browsercss_is_space(*text)) {
        ++text;
    }
    if (*text == '-') {
        sign = -1;
        ++text;
    } else if (*text == '+') {
        ++text;
    }

    while (browsercss_is_digit(*text)) {
        saw_digit = 1;
        /* 整数部分直接按十进制累加。 */
        value = value * 10 + (*text - '0');
        ++text;
    }

    if (*text == '.') {
        ++text;
        while (browsercss_is_digit(*text)) {
            saw_digit = 1;
            if (frac_scale < 1000) {
                /* 小数部分只保留有限精度，避免这里变成通用浮点库。 */
                frac = frac * 10 + (*text - '0');
                frac_scale *= 10;
            }
            ++text;
        }
    }

    if (!saw_digit) {
        return 0;
    }

    /* 最终统一折叠成一个定点整数。 */
    value = value * scale + (frac * scale) / frac_scale;
    return (int)(value * sign);
}

static int browsercss_scale_round(long long value, int scale)
{
    /* 把上面的定点数再还原成像素整数，并统一做四舍五入。 */
    if (scale <= 0) {
        return 0;
    }
    if (value >= 0) {
        return (int)((value + scale / 2) / scale);
    }
    return (int)((value - scale / 2) / scale);
}

static int browsercss_parse_length(browser_css_document *doc, const char *text, int font_px)
{
    /*
     * 这里负责把 CSS 长度转换成当前布局器能直接使用的像素值。
     * 目前支持的单位仍然是“实用优先”：
     * px / pt / em / rem / % / vh / vw
     *
     * 注意：
     * - 这里的 % 仍然是简化实现，按 font_px 推导，不是完整规范语义。
     * - 这足够覆盖按钮、标题、普通正文等大量网页样式。
     */
    int value_scaled;
    int i;
    char unit[8];
    const int scale = 100;

    if (text == NULL || text[0] == '\0') {
        return 0;
    }

    value_scaled = browsercss_parse_number_scaled(text, scale);
    i = 0;
    while (text[i] != '\0' && (browsercss_is_digit(text[i]) || text[i] == '-' || text[i] == '.')) {
        ++i;
    }
    browsercss_copy(unit, sizeof(unit), text + i);
    browsercss_trim(unit);

    if (unit[0] == '\0' || browsercss_streq_nocase(unit, "px")) {
        return browsercss_scale_round(value_scaled, scale);
    }
    if (browsercss_streq_nocase(unit, "pt")) {
        return browsercss_scale_round((long long)value_scaled * 4, scale * 3);
    }
    if (browsercss_streq_nocase(unit, "em")) {
        return browsercss_scale_round((long long)value_scaled * font_px, scale);
    }
    if (browsercss_streq_nocase(unit, "rem")) {
        return browsercss_scale_round((long long)value_scaled * BCSS_DEFAULT_FONT_PX, scale);
    }
    if (browsercss_streq_nocase(unit, "%")) {
        return browsercss_scale_round((long long)font_px * value_scaled, scale * 100);
    }
    if (browsercss_streq_nocase(unit, "vh")) {
        return browsercss_scale_round((long long)doc->viewport_height * value_scaled, scale * 100);
    }
    if (browsercss_streq_nocase(unit, "vw")) {
        return browsercss_scale_round((long long)doc->viewport_width * value_scaled, scale * 100);
    }
    return browsercss_scale_round(value_scaled, scale);
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
    if (browsercss_streq_nocase(name, "navy")) return browsercss_rgba(30, 64, 175, 255);
    if (browsercss_streq_nocase(name, "teal")) return browsercss_rgba(13, 148, 136, 255);
    if (browsercss_streq_nocase(name, "cyan")) return browsercss_rgba(8, 145, 178, 255);
    if (browsercss_streq_nocase(name, "indigo")) return browsercss_rgba(79, 70, 229, 255);
    if (browsercss_streq_nocase(name, "pink")) return browsercss_rgba(236, 72, 153, 255);
    if (browsercss_streq_nocase(name, "brown")) return browsercss_rgba(120, 53, 15, 255);
    if (browsercss_streq_nocase(name, "beige")) return browsercss_rgba(245, 245, 220, 255);
    if (browsercss_streq_nocase(name, "gold")) return browsercss_rgba(234, 179, 8, 255);
    if (browsercss_streq_nocase(name, "aliceblue")) return browsercss_rgba(240, 248, 255, 255);
    if (browsercss_streq_nocase(name, "whitesmoke")) return browsercss_rgba(245, 245, 245, 255);
    if (browsercss_streq_nocase(name, "lightgray") || browsercss_streq_nocase(name, "lightgrey")) return browsercss_rgba(209, 213, 219, 255);
    if (browsercss_streq_nocase(name, "darkgray") || browsercss_streq_nocase(name, "darkgrey")) return browsercss_rgba(75, 85, 99, 255);
    if (browsercss_streq_nocase(name, "transparent")) return browsercss_rgba(0, 0, 0, 0);
    *ok = 0;
    return 0;
}

static int browsercss_find_variable(const browser_css_document *doc, const char *name)
{
    /* 在线性表里查找 CSS 变量。当前数量不大，没必要上哈希表。 */
    int i;

    if (doc == NULL || name == NULL || name[0] == '\0') {
        return -1;
    }

    for (i = 0; i < doc->var_count; ++i) {
        if (browsercss_streq_nocase(doc->vars[i].name, name)) {
            return i;
        }
    }
    return -1;
}

static void browsercss_set_variable(browser_css_document *doc, const char *name, const char *value)
{
    /*
     * 记录 --var-name: xxx;
     * 当前策略很直接：后出现的同名变量覆盖前值。
     * 这不完整等价于标准级联，但对多数页面已经够用了。
     */
    int index;

    if (doc == NULL || name == NULL || value == NULL ||
        name[0] == '\0' || value[0] == '\0') {
        return;
    }

    index = browsercss_find_variable(doc, name);
    if (index < 0) {
        if (doc->var_count >= BCSS_MAX_VARS) {
            return;
        }
        index = doc->var_count++;
    }

    browsercss_copy(doc->vars[index].name, sizeof(doc->vars[index].name), name);
    browsercss_copy(doc->vars[index].value, sizeof(doc->vars[index].value), value);
}

static int browsercss_parse_var_expression(const char *text,
                                           int start,
                                           int *next_index,
                                           char *name,
                                           int name_size,
                                           char *fallback,
                                           int fallback_size)
{
    /*
     * 从文本中拆出 var(--name, fallback)：
     * - name 返回变量名
     * - fallback 返回回退值
     * - next_index 指向右括号后一个字符
     *
     * 这里单独写成函数，是为了把“括号匹配”逻辑和真正的值替换逻辑分开，
     * 后续你如果要补 calc()/env()，可以沿用同样的扫描框架。
     */
    int i;
    int depth = 1;
    int comma = -1;
    int inner_start = start + 4;
    int inner_end = -1;

    if (text == NULL || next_index == NULL || name == NULL || fallback == NULL ||
        strncmp(text + start, "var(", 4) != 0) {
        return 0;
    }

    for (i = inner_start; text[i] != '\0'; ++i) {
        if (text[i] == '(') {
            /* fallback 里本身可能还有函数，所以括号层级必须自己数。 */
            ++depth;
        } else if (text[i] == ')') {
            --depth;
            if (depth == 0) {
                /* 找到和最外层 var( 对应的右括号，表达式到这里结束。 */
                inner_end = i;
                break;
            }
        } else if (text[i] == ',' && depth == 1 && comma < 0) {
            /* 只有最外层逗号才表示“变量名 / fallback”的分界。 */
            comma = i;
        }
    }

    if (inner_end < 0) {
        return 0;
    }

    if (comma < 0) {
        browsercss_copy(name, name_size, "");
        browsercss_copy(fallback, fallback_size, "");
        if (inner_end > inner_start) {
            int len = inner_end - inner_start;
            if (len >= name_size) len = name_size - 1;
            memcpy(name, text + inner_start, (size_t)len);
            name[len] = '\0';
            browsercss_trim(name);
        }
    } else {
        int name_len = comma - inner_start;
        int fallback_len = inner_end - comma - 1;
        if (name_len >= name_size) name_len = name_size - 1;
        if (fallback_len >= fallback_size) fallback_len = fallback_size - 1;
        memcpy(name, text + inner_start, (size_t)name_len);
        name[name_len] = '\0';
        memcpy(fallback, text + comma + 1, (size_t)fallback_len);
        fallback[fallback_len] = '\0';
        browsercss_trim(name);
        browsercss_trim(fallback);
    }

    *next_index = inner_end + 1;
    return name[0] != '\0';
}

static void browsercss_resolve_value(browser_css_document *doc,
                                     const char *value,
                                     char *out,
                                     int out_size,
                                     int depth)
{
    /*
     * 在声明真正进入解析前，先把 var(...) 展开成普通文本。
     * 例如：
     * color: var(--primary, #333)
     * 会先被替换成具体颜色字符串，再交给 parse_color / parse_length。
     *
     * depth 用来限制递归层数，避免变量相互引用时死循环。
     */
    int in_pos = 0;
    int out_pos = 0;

    if (out == NULL || out_size <= 0) {
        return;
    }

    out[0] = '\0';
    if (value == NULL) {
        return;
    }

    if (depth > 4) {
        browsercss_copy(out, out_size, value);
        return;
    }

    while (value[in_pos] != '\0' && out_pos < out_size - 1) {
        if (strncmp(value + in_pos, "var(", 4) == 0) {
            char name[64];
            char fallback[192];
            char resolved[192];
            int next_index;
            int var_index;

            if (!browsercss_parse_var_expression(value, in_pos, &next_index,
                                                 name, sizeof(name),
                                                 fallback, sizeof(fallback))) {
                out[out_pos++] = value[in_pos++];
                continue;
            }

            resolved[0] = '\0';
            var_index = browsercss_find_variable(doc, name);
            if (var_index >= 0) {
                /* 找到变量时，递归展开它自己的值，支持变量套变量。 */
                browsercss_resolve_value(doc, doc->vars[var_index].value,
                                         resolved, sizeof(resolved), depth + 1);
            } else if (fallback[0] != '\0') {
                /* 没找到变量时，尝试展开 fallback。 */
                browsercss_resolve_value(doc, fallback, resolved, sizeof(resolved), depth + 1);
            }

            if (resolved[0] != '\0') {
                int i = 0;
                while (resolved[i] != '\0' && out_pos < out_size - 1) {
                    out[out_pos++] = resolved[i++];
                }
            }
            in_pos = next_index;
            continue;
        }

        out[out_pos++] = value[in_pos++];
    }

    out[out_pos] = '\0';
    browsercss_trim(out);
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

    if (browsercss_contains_word_nocase(value, "rgb(") ||
        browsercss_contains_word_nocase(value, "rgba(")) {
        const char *p = strchr(value, '(');
        int r, g, b;
        int a = 255;
        if (p == NULL) return 0;
        ++p;
        r = browsercss_parse_int(p);
        p = strchr(p, ',');
        if (p == NULL) return 0;
        g = browsercss_parse_int(p + 1);
        p = strchr(p + 1, ',');
        if (p == NULL) return 0;
        b = browsercss_parse_int(p + 1);
        if (browsercss_contains_word_nocase(value, "rgba(")) {
            const char *a_sep = strchr(p + 1, ',');
            if (a_sep != NULL) {
                const char *a_text = a_sep + 1;
                int alpha_scaled;
                while (browsercss_is_space(*a_text)) {
                    ++a_text;
                }
                if (strchr(a_text, '%') != NULL) {
                    alpha_scaled = browsercss_parse_number_scaled(a_text, 100);
                    a = browsercss_scale_round((long long)alpha_scaled * 255, 100);
                } else {
                    alpha_scaled = browsercss_parse_number_scaled(a_text, 1000);
                    if (alpha_scaled >= 0 && alpha_scaled <= 1000) {
                        a = browsercss_scale_round((long long)alpha_scaled * 255, 1000);
                    } else {
                        a = browsercss_parse_int(a_text);
                    }
                }
            }
        }
        if (r < 0) r = 0;
        if (r > 255) r = 255;
        if (g < 0) g = 0;
        if (g > 255) g = 255;
        if (b < 0) b = 0;
        if (b > 255) b = 255;
        if (a < 0) a = 0;
        if (a > 255) a = 255;
        *color = browsercss_rgba((unsigned int)r, (unsigned int)g, (unsigned int)b, (unsigned int)a);
        return 1;
    }

    *color = browsercss_named_color(value, &ok);
    return ok;
}

void browsercss_style_reset(browsercss_Style *style)
{
    /* 初始化“浏览器默认值”。这里只放布局器真的会读取的默认语义。 */
    memset(style, 0, sizeof(*style));
    style->font_px = BCSS_DEFAULT_FONT_PX;
    style->color = BCSS_TEXT_COLOR;
    style->display_mode = BCSS_DISPLAY_INLINE;
    style->text_align = BCSS_TEXT_ALIGN_LEFT;
    style->border_color = BCSS_TEXT_COLOR;
}

static void browsercss_style_inherit(const browsercss_Style *parent, browsercss_Style *style)
{
    /*
     * 只继承当前引擎已经支持、并且按 CSS 语义应当继承的字段。
     * 例如颜色、字号、行高、文本装饰、white-space。
     * margin/padding/border 这些盒模型字段不会走继承。
     */
    browsercss_style_reset(style);
    if (parent != NULL) {
        style->font_px = parent->font_px;
        style->color = parent->color;
        style->bold = parent->bold;
        style->underline = parent->underline;
        style->preformatted = parent->preformatted;
        style->nowrap = parent->nowrap;
        style->break_word = parent->break_word;
        style->text_align = parent->text_align;
        style->line_height = parent->line_height;
    }
}

static void browsercss_style_apply(browsercss_Style *dst, const browsercss_Style *src)
{
    /*
     * 按 props 位图做“显式覆盖”。
     * 这一步是整个简化 CSS 引擎的关键：
     * - 没设置的字段不动
     * - 设置过的字段覆盖到目标样式
     *
     * 这样同一个结构既能表示“最终值”，也能表示“一个声明块里实际写过什么”。
     */
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
    if ((src->props & BCSS_PROP_NOWRAP) != 0u) dst->nowrap = src->nowrap;
    if ((src->props & BCSS_PROP_WORD_BREAK) != 0u) dst->break_word = src->break_word;
    if ((src->props & BCSS_PROP_MIN_HEIGHT) != 0u) dst->min_height = src->min_height;
    if ((src->props & BCSS_PROP_MAX_WIDTH) != 0u) dst->max_width = src->max_width;
    if ((src->props & BCSS_PROP_WIDTH) != 0u) dst->width = src->width;
    if ((src->props & BCSS_PROP_HEIGHT) != 0u) dst->height = src->height;
    if ((src->props & BCSS_PROP_BOX_SIZING) != 0u) dst->box_sizing_border_box = src->box_sizing_border_box;
    if ((src->props & BCSS_PROP_JUSTIFY) != 0u) dst->justify_center = src->justify_center;
    if ((src->props & BCSS_PROP_ALIGN) != 0u) dst->align_center = src->align_center;
    if ((src->props & BCSS_PROP_TEXT_ALIGN) != 0u) dst->text_align = src->text_align;
    if ((src->props & BCSS_PROP_LINE_HEIGHT) != 0u) dst->line_height = src->line_height;
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
    if ((src->props & BCSS_PROP_BORDER_TOP_WIDTH) != 0u) dst->border_top_width = src->border_top_width;
    if ((src->props & BCSS_PROP_BORDER_RIGHT_WIDTH) != 0u) dst->border_right_width = src->border_right_width;
    if ((src->props & BCSS_PROP_BORDER_BOTTOM_WIDTH) != 0u) dst->border_bottom_width = src->border_bottom_width;
    if ((src->props & BCSS_PROP_BORDER_LEFT_WIDTH) != 0u) dst->border_left_width = src->border_left_width;
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

static void browsercss_sanitize_selector(char *selector)
{
    /*
     * 现代站点常用伪类、伪元素、属性选择器、兄弟选择器。
     * 当前引擎并没有完整支持这些语义，如果直接拒绝，整条规则都会丢失。
     *
     * 这里做一个“降级清洗”：
     * - 去掉 :hover / :nth-child(...) / ::before 之类伪选择器
     * - 去掉 [data-x] 这类属性选择器
     * - 把 + / ~ 兄弟关系退化成普通后代空格
     *
     * 目的不是精准还原，而是尽量保住规则里还能利用的 tag/class/id 部分。
     */
    int read_pos = 0;
    int write_pos = 0;
    int attr_depth = 0;
    int pseudo_depth = 0;

    if (selector == NULL) {
        return;
    }

    while (selector[read_pos] != '\0') {
        char ch = selector[read_pos];

        if (attr_depth > 0) {
            /* 当前引擎不支持属性选择器，直接整段跳过。 */
            if (ch == '[') ++attr_depth;
            else if (ch == ']') --attr_depth;
            ++read_pos;
            continue;
        }

        if (pseudo_depth > 0) {
            /* :hover / :nth-child(...) / ::before 都先降级丢弃，只保留基础选择器主体。 */
            if (ch == '(') ++pseudo_depth;
            else if (ch == ')') --pseudo_depth;
            else if (pseudo_depth == 1 &&
                     (browsercss_is_space(ch) || ch == '>' || ch == '+' || ch == '~' || ch == ',')) {
                pseudo_depth = 0;
                continue;
            }
            ++read_pos;
            continue;
        }

        if (ch == '[') {
            attr_depth = 1;
            ++read_pos;
            continue;
        }
        if (ch == ':') {
            pseudo_depth = 1;
            ++read_pos;
            continue;
        }
        if (ch == '+' || ch == '~') {
            /* 兄弟选择器暂时退化成普通空格，尽量保住规则。 */
            ch = ' ';
        }

        selector[write_pos++] = ch;
        ++read_pos;
    }

    selector[write_pos] = '\0';
}

static int browsercss_parse_selector(browsercss_Rule *rule, char *selector)
{
    /*
     * 把清洗后的选择器拆成若干 part，供后面的匹配器使用。
     * 目前支持：
     * - tag
     * - .class
     * - #id
     * - 后代选择器
     * - 子选择器 >
     *
     * 不支持的语法不会再直接炸掉整条规则，而是尽量在 sanitize 阶段降级。
     */
    char token[128];
    int token_len = 0;
    int combinator = BCSS_COMBINATOR_NONE;
    int saw_space = 0;
    const char *p;

    if (rule == NULL || selector == NULL) {
        return 0;
    }

    browsercss_trim(selector);
    browsercss_sanitize_selector(selector);
    browsercss_trim(selector);
    if (selector[0] == '\0') {
        return 0;
    }

    memset(rule, 0, sizeof(*rule));
    p = selector;
    while (*p != '\0') {
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
        if (token[0] == '\0') {
            continue;
        }
        if (rule->part_count >= BCSS_MAX_SELECTOR_PARTS) {
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

static void browsercss_set_uniform_border(browsercss_Style *style, int width)
{
    style->border_width = width;
    style->border_top_width = width;
    style->border_right_width = width;
    style->border_bottom_width = width;
    style->border_left_width = width;
    style->props |= BCSS_PROP_BORDER_WIDTH |
                    BCSS_PROP_BORDER_TOP_WIDTH |
                    BCSS_PROP_BORDER_RIGHT_WIDTH |
                    BCSS_PROP_BORDER_BOTTOM_WIDTH |
                    BCSS_PROP_BORDER_LEFT_WIDTH;
}

static int browsercss_parse_background_color(const char *value, unsigned int *color)
{
    const char *p = value;
    char token[64];
    int token_len;

    while (*p != '\0') {
        while (browsercss_is_space(*p)) {
            ++p;
        }
        if (*p == '\0') {
            break;
        }

        token_len = 0;
        if (browsercss_starts_with_nocase(p, "rgb(") || browsercss_starts_with_nocase(p, "rgba(")) {
            while (*p != '\0' && *p != ')' && token_len < (int)sizeof(token) - 1) {
                token[token_len++] = *p++;
            }
            if (*p == ')' && token_len < (int)sizeof(token) - 1) {
                token[token_len++] = *p++;
            }
        } else {
            while (*p != '\0' && !browsercss_is_space(*p) &&
                   token_len < (int)sizeof(token) - 1) {
                token[token_len++] = *p++;
            }
        }
        token[token_len] = '\0';
        if (browsercss_parse_color(token, color)) {
            return 1;
        }
    }

    return 0;
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
            browsercss_set_uniform_border(style, 0);
            continue;
        }

        if (!browsercss_streq_nocase(token, "solid") &&
            !browsercss_streq_nocase(token, "dashed") &&
            !browsercss_streq_nocase(token, "dotted") &&
            !browsercss_streq_nocase(token, "double")) {
            browsercss_set_uniform_border(style,
                                          browsercss_parse_length(doc, token,
                                                                  style->font_px > 0 ? style->font_px : BCSS_DEFAULT_FONT_PX));
        }
    }
}

static void browsercss_parse_declaration(browser_css_document *doc,
                                         browsercss_Style *style,
                                         const char *name,
                                         const char *value)
{
    /*
     * 这是“CSS 文本 -> 内部样式字段”的总入口。
     *
     * 当前处理顺序是：
     * 1. 先展开 var(...)
     * 2. 如果是 --xxx，就记录到变量表
     * 3. 否则按属性名把文本解释成具体字段
     *
     * 后续扩展新属性时，优先在这里加分支；如果涉及复合语法，再拆辅助函数。
     */
    int font_px;
    char resolved_value[192];
    const char *effective_value = value;

    if (doc == NULL || style == NULL || name == NULL || value == NULL ||
        name[0] == '\0' || value[0] == '\0') {
        return;
    }

    browsercss_resolve_value(doc, value, resolved_value, sizeof(resolved_value), 0);
    if (resolved_value[0] != '\0') {
        /* 后面的所有属性解析都尽量基于展开后的文本。 */
        effective_value = resolved_value;
    }

    if (name[0] == '-' && name[1] == '-') {
        /* 自定义属性本身不参与布局，先写进变量表，给后续 var(...) 用。 */
        browsercss_set_variable(doc, name, effective_value);
        return;
    }

    font_px = style->font_px > 0 ? style->font_px : BCSS_DEFAULT_FONT_PX;

    if (browsercss_streq_nocase(name, "color")) {
        if (browsercss_parse_color(effective_value, &style->color)) {
            style->props |= BCSS_PROP_COLOR;
        }
    } else if (browsercss_streq_nocase(name, "background-color") ||
               browsercss_streq_nocase(name, "background")) {
        if (browsercss_parse_background_color(effective_value, &style->background)) {
            style->props |= BCSS_PROP_BACKGROUND;
        }
    } else if (browsercss_streq_nocase(name, "font-size")) {
        style->font_px = browsercss_parse_length(doc, effective_value, BCSS_DEFAULT_FONT_PX);
        if (style->font_px < 10) {
            style->font_px = 10;
        }
        style->props |= BCSS_PROP_FONT_SIZE;
    } else if (browsercss_streq_nocase(name, "font-weight")) {
        style->bold = browsercss_streq_nocase(effective_value, "bold") || browsercss_parse_int(effective_value) >= 600;
        style->props |= BCSS_PROP_BOLD;
    } else if (browsercss_streq_nocase(name, "text-decoration")) {
        style->underline = browsercss_contains_word_nocase(effective_value, "underline");
        style->props |= BCSS_PROP_UNDERLINE;
    } else if (browsercss_streq_nocase(name, "display")) {
        if (browsercss_streq_nocase(effective_value, "none")) {
            style->display_mode = BCSS_DISPLAY_NONE;
        } else if (browsercss_streq_nocase(effective_value, "block") ||
                   browsercss_streq_nocase(effective_value, "inline-block") ||
                   browsercss_streq_nocase(effective_value, "grid") ||
                   browsercss_streq_nocase(effective_value, "table") ||
                   browsercss_streq_nocase(effective_value, "table-row") ||
                   browsercss_streq_nocase(effective_value, "table-cell")) {
            /* 这里先按“可占块”的语义降级，不尝试实现完整 table/grid。 */
            style->display_mode = BCSS_DISPLAY_BLOCK;
        } else if (browsercss_streq_nocase(effective_value, "flex")) {
            style->display_mode = BCSS_DISPLAY_FLEX;
        } else {
            style->display_mode = BCSS_DISPLAY_INLINE;
        }
        style->props |= BCSS_PROP_DISPLAY;
    } else if (browsercss_streq_nocase(name, "line-height")) {
        if (browsercss_streq_nocase(effective_value, "normal")) {
            style->line_height = style->font_px + 6;
        } else {
            style->line_height = browsercss_parse_length(doc, effective_value, font_px);
            if (style->line_height <= 0) {
                style->line_height = font_px + 6;
            }
        }
        style->props |= BCSS_PROP_LINE_HEIGHT;
    } else if (browsercss_streq_nocase(name, "justify-content")) {
        style->justify_center = browsercss_streq_nocase(effective_value, "center");
        style->props |= BCSS_PROP_JUSTIFY;
    } else if (browsercss_streq_nocase(name, "align-items")) {
        style->align_center = browsercss_streq_nocase(effective_value, "center");
        style->props |= BCSS_PROP_ALIGN;
    } else if (browsercss_streq_nocase(name, "text-align")) {
        if (browsercss_streq_nocase(effective_value, "center")) {
            style->text_align = BCSS_TEXT_ALIGN_CENTER;
        } else if (browsercss_streq_nocase(effective_value, "right")) {
            style->text_align = BCSS_TEXT_ALIGN_RIGHT;
        } else {
            style->text_align = BCSS_TEXT_ALIGN_LEFT;
        }
        style->props |= BCSS_PROP_TEXT_ALIGN;
    } else if (browsercss_streq_nocase(name, "min-height")) {
        style->min_height = browsercss_parse_length(doc, effective_value, font_px);
        style->props |= BCSS_PROP_MIN_HEIGHT;
    } else if (browsercss_streq_nocase(name, "max-width")) {
        style->max_width = browsercss_parse_length(doc, effective_value, font_px);
        style->props |= BCSS_PROP_MAX_WIDTH;
    } else if (browsercss_streq_nocase(name, "width")) {
        style->width = browsercss_parse_length(doc, effective_value, font_px);
        style->props |= BCSS_PROP_WIDTH;
    } else if (browsercss_streq_nocase(name, "height")) {
        style->height = browsercss_parse_length(doc, effective_value, font_px);
        style->props |= BCSS_PROP_HEIGHT;
    } else if (browsercss_streq_nocase(name, "white-space")) {
        style->preformatted = browsercss_streq_nocase(effective_value, "pre") ||
                              browsercss_streq_nocase(effective_value, "pre-wrap");
        /* nowrap 和 pre 语义不同，所以单独存一位，给行内布局判断。 */
        style->nowrap = browsercss_streq_nocase(effective_value, "nowrap");
        style->props |= BCSS_PROP_PRE | BCSS_PROP_NOWRAP;
    } else if (browsercss_streq_nocase(name, "overflow-wrap") ||
               browsercss_streq_nocase(name, "word-wrap")) {
        /*
         * 现代站点很常见的长链接、哈希串、题面 token，很多都依赖这里避免横向顶出容器。
         * 这里只抓最有价值的两个值：
         * - break-word
         * - anywhere
         */
        style->break_word = browsercss_streq_nocase(effective_value, "break-word") ||
                            browsercss_streq_nocase(effective_value, "anywhere");
        style->props |= BCSS_PROP_WORD_BREAK;
    } else if (browsercss_streq_nocase(name, "word-break")) {
        /*
         * 这里做一个“实用优先”的降级：
         * - break-all / break-word：允许在单词内部拆分
         * - keep-all / normal：恢复普通行为
         */
        style->break_word = browsercss_streq_nocase(effective_value, "break-all") ||
                            browsercss_streq_nocase(effective_value, "break-word");
        style->props |= BCSS_PROP_WORD_BREAK;
    } else if (browsercss_streq_nocase(name, "margin")) {
        browsercss_parse_box_shorthand(doc, style, BCSS_PROP_MARGIN_TOP, effective_value);
    } else if (browsercss_streq_nocase(name, "padding")) {
        browsercss_parse_box_shorthand(doc, style, BCSS_PROP_PADDING_TOP, effective_value);
    } else if (browsercss_streq_nocase(name, "margin-top")) {
        style->margin_top = browsercss_parse_length(doc, effective_value, font_px);
        style->props |= BCSS_PROP_MARGIN_TOP;
    } else if (browsercss_streq_nocase(name, "margin-right")) {
        if (browsercss_streq_nocase(effective_value, "auto")) {
            style->margin_right_auto = 1;
            style->props |= BCSS_PROP_MARGIN_RIGHT_AUTO;
        } else {
            style->margin_right = browsercss_parse_length(doc, effective_value, font_px);
            style->props |= BCSS_PROP_MARGIN_RIGHT;
        }
    } else if (browsercss_streq_nocase(name, "margin-bottom")) {
        style->margin_bottom = browsercss_parse_length(doc, effective_value, font_px);
        style->props |= BCSS_PROP_MARGIN_BOTTOM;
    } else if (browsercss_streq_nocase(name, "margin-left")) {
        if (browsercss_streq_nocase(effective_value, "auto")) {
            style->margin_left_auto = 1;
            style->props |= BCSS_PROP_MARGIN_LEFT_AUTO;
        } else {
            style->margin_left = browsercss_parse_length(doc, effective_value, font_px);
            style->props |= BCSS_PROP_MARGIN_LEFT;
        }
    } else if (browsercss_streq_nocase(name, "padding-top")) {
        style->padding_top = browsercss_parse_length(doc, effective_value, font_px);
        style->props |= BCSS_PROP_PADDING_TOP;
    } else if (browsercss_streq_nocase(name, "padding-right")) {
        style->padding_right = browsercss_parse_length(doc, effective_value, font_px);
        style->props |= BCSS_PROP_PADDING_RIGHT;
    } else if (browsercss_streq_nocase(name, "padding-bottom")) {
        style->padding_bottom = browsercss_parse_length(doc, effective_value, font_px);
        style->props |= BCSS_PROP_PADDING_BOTTOM;
    } else if (browsercss_streq_nocase(name, "padding-left")) {
        style->padding_left = browsercss_parse_length(doc, effective_value, font_px);
        style->props |= BCSS_PROP_PADDING_LEFT;
    } else if (browsercss_streq_nocase(name, "border")) {
        browsercss_parse_border_shorthand(doc, style, effective_value);
    } else if (browsercss_streq_nocase(name, "border-width")) {
        browsercss_set_uniform_border(style, browsercss_parse_length(doc, effective_value, font_px));
    } else if (browsercss_streq_nocase(name, "border-color")) {
        if (browsercss_parse_color(effective_value, &style->border_color)) {
            style->props |= BCSS_PROP_BORDER_COLOR;
        }
    } else if (browsercss_streq_nocase(name, "border-top") ||
               browsercss_streq_nocase(name, "border-right") ||
               browsercss_streq_nocase(name, "border-bottom") ||
               browsercss_streq_nocase(name, "border-left")) {
        browsercss_Style border_side = *style;
        browsercss_parse_border_shorthand(doc, &border_side, effective_value);
        if (browsercss_streq_nocase(name, "border-top")) {
            style->border_top_width = border_side.border_top_width;
            style->props |= BCSS_PROP_BORDER_TOP_WIDTH;
        } else if (browsercss_streq_nocase(name, "border-right")) {
            style->border_right_width = border_side.border_right_width;
            style->props |= BCSS_PROP_BORDER_RIGHT_WIDTH;
        } else if (browsercss_streq_nocase(name, "border-bottom")) {
            style->border_bottom_width = border_side.border_bottom_width;
            style->props |= BCSS_PROP_BORDER_BOTTOM_WIDTH;
        } else {
            style->border_left_width = border_side.border_left_width;
            style->props |= BCSS_PROP_BORDER_LEFT_WIDTH;
        }
        if ((border_side.props & BCSS_PROP_BORDER_COLOR) != 0u) {
            style->border_color = border_side.border_color;
            style->props |= BCSS_PROP_BORDER_COLOR;
        }
    } else if (browsercss_streq_nocase(name, "border-top-width")) {
        style->border_top_width = browsercss_parse_length(doc, effective_value, font_px);
        style->props |= BCSS_PROP_BORDER_TOP_WIDTH;
    } else if (browsercss_streq_nocase(name, "border-right-width")) {
        style->border_right_width = browsercss_parse_length(doc, effective_value, font_px);
        style->props |= BCSS_PROP_BORDER_RIGHT_WIDTH;
    } else if (browsercss_streq_nocase(name, "border-bottom-width")) {
        style->border_bottom_width = browsercss_parse_length(doc, effective_value, font_px);
        style->props |= BCSS_PROP_BORDER_BOTTOM_WIDTH;
    } else if (browsercss_streq_nocase(name, "border-left-width")) {
        style->border_left_width = browsercss_parse_length(doc, effective_value, font_px);
        style->props |= BCSS_PROP_BORDER_LEFT_WIDTH;
    } else if (browsercss_streq_nocase(name, "border-top-color") ||
               browsercss_streq_nocase(name, "border-right-color") ||
               browsercss_streq_nocase(name, "border-bottom-color") ||
               browsercss_streq_nocase(name, "border-left-color")) {
        if (browsercss_parse_color(effective_value, &style->border_color)) {
            style->props |= BCSS_PROP_BORDER_COLOR;
        }
    } else if (browsercss_streq_nocase(name, "box-sizing")) {
        /* 这里只记录语义，真正换算宽高在布局阶段处理。 */
        style->box_sizing_border_box = browsercss_streq_nocase(effective_value, "border-box");
        style->props |= BCSS_PROP_BOX_SIZING;
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

#ifndef BROWSER_NOSTL_DISABLE_LEXBOR
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
#else
static int browsercss_parse_css_lexbor(browser_css_document *doc, const char *css_text)
{
    (void)doc;
    (void)css_text;
    return 0;
}
#endif

/* 只有在 Lexbor 解析失败时，才退回到这个很小的兜底块解析器。 */
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

static void browsercss_release_images(browser_css_document *document)
{
    int i;

    if (document == NULL) {
        return;
    }

    for (i = 0; i < document->image_count; ++i) {
        free(document->images[i].pixels);
        document->images[i].pixels = NULL;
    }
    document->image_count = 0;
}

/* 这个主翻译单元现在只负责解析、样式匹配和文档初始化。 */
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
        browsercss_release_images(document);
        free(document);
    }
}

extern "C" void browser_css_clear(browser_css_document *document)
{
    if (document == NULL) return;
    browsercss_release_images(document);
    document->rule_count = 0;
    document->run_count = 0;
    document->box_count = 0;
    document->link_count = 0;
    document->var_count = 0;
    document->content_height = 0;
    document->title[0] = '\0';
}

/* 加载 HTML 分三步：提取标题、准备样式表、执行布局。 */
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
