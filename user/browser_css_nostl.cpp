#include "./xapi/include/x3api.h"
#include "browser_css_nostl.h"
#include "lexbor/css/css.h"

#define BCSS_MAX_RULES        256
#define BCSS_MAX_RUNS         4096
#define BCSS_MAX_BOXES        1024
#define BCSS_MAX_LINKS        512
#define BCSS_MAX_STACK        64
#define BCSS_MAX_SELECTOR     64
#define BCSS_MAX_SELECTOR_PARTS 8
#define BCSS_MAX_SELECTOR_CLASSES 4
#define BCSS_MAX_TAG          24
#define BCSS_MAX_TEXT         192
#define BCSS_DEFAULT_FONT_PX  16
#define BCSS_LINK_COLOR       0x2a6f97ff
#define BCSS_TEXT_COLOR       0x1f2933ff

enum
{
    BCSS_DISPLAY_INLINE = 0,
    BCSS_DISPLAY_BLOCK = 1,
    BCSS_DISPLAY_NONE = 2,
    BCSS_DISPLAY_FLEX = 3
};

enum
{
    BCSS_PROP_COLOR = 1u << 0,
    BCSS_PROP_BACKGROUND = 1u << 1,
    BCSS_PROP_FONT_SIZE = 1u << 2,
    BCSS_PROP_DISPLAY = 1u << 3,
    BCSS_PROP_BOLD = 1u << 4,
    BCSS_PROP_UNDERLINE = 1u << 5,
    BCSS_PROP_MARGIN_TOP = 1u << 6,
    BCSS_PROP_MARGIN_RIGHT = 1u << 7,
    BCSS_PROP_MARGIN_BOTTOM = 1u << 8,
    BCSS_PROP_MARGIN_LEFT = 1u << 9,
    BCSS_PROP_PADDING_TOP = 1u << 10,
    BCSS_PROP_PADDING_RIGHT = 1u << 11,
    BCSS_PROP_PADDING_BOTTOM = 1u << 12,
    BCSS_PROP_PADDING_LEFT = 1u << 13,
    BCSS_PROP_PRE = 1u << 14,
    BCSS_PROP_MIN_HEIGHT = 1u << 15,
    BCSS_PROP_MAX_WIDTH = 1u << 16,
    BCSS_PROP_JUSTIFY = 1u << 17,
    BCSS_PROP_ALIGN = 1u << 18
};

enum
{
    BCSS_COMBINATOR_NONE = 0,
    BCSS_COMBINATOR_DESCENDANT = 1,
    BCSS_COMBINATOR_CHILD = 2
};

typedef struct browsercss_Style
{
    unsigned int props;
    unsigned int color;
    unsigned int background;
    int          font_px;
    int          margin_top;
    int          margin_right;
    int          margin_bottom;
    int          margin_left;
    int          padding_top;
    int          padding_right;
    int          padding_bottom;
    int          padding_left;
    int          display_mode;
    int          bold;
    int          underline;
    int          preformatted;
    int          min_height;
    int          max_width;
    int          justify_center;
    int          align_center;
} browsercss_Style;

typedef struct browsercss_SelectorPart
{
    char tag[BCSS_MAX_TAG];
    char id[BCSS_MAX_SELECTOR];
    char classes[BCSS_MAX_SELECTOR_CLASSES][BCSS_MAX_SELECTOR];
    int  class_count;
    int  universal;
    int  combinator_from_prev;
} browsercss_SelectorPart;

typedef struct browsercss_Rule
{
    browsercss_SelectorPart parts[BCSS_MAX_SELECTOR_PARTS];
    int                  part_count;
    browsercss_Style style;
} browsercss_Rule;

typedef struct browsercss_Run
{
    int          x;
    int          y;
    int          width;
    int          height;
    int          font_px;
    unsigned int color;
    int          bold;
    int          underline;
    int          link_index;
    char         text[BCSS_MAX_TEXT];
} browsercss_Run;

typedef struct browsercss_Box
{
    int          x;
    int          y;
    int          width;
    int          height;
    unsigned int color;
} browsercss_Box;

typedef struct browsercss_Link
{
    int  x1;
    int  y1;
    int  x2;
    int  y2;
    char href[768];
    char base_url[768];
} browsercss_Link;

typedef struct browsercss_Frame
{
    char             tag[BCSS_MAX_TAG];
    char             class_name[BCSS_MAX_SELECTOR];
    char             id[BCSS_MAX_SELECTOR];
    browsercss_Style style;
    char             href[768];
    char             base_url[768];
    int              hidden;
    int              is_block;
    int              has_context;
} browsercss_Frame;

typedef struct browsercss_Context
{
    browsercss_Style style;
    int              box_x;
    int              box_y;
    int              box_width;
    int              content_x;
    int              content_width;
    int              cursor_y;
    int              line_x;
    int              line_y;
    int              line_height;
    int              line_run_start;
    int              box_index;
    int              content_height;
    int              first_run_index;
    int              first_box_index;
    int              first_link_index;
} browsercss_Context;

struct browser_css_document
{
    browser_css_callbacks callbacks;
    int                   viewport_width;
    int                   viewport_height;
    int                   content_height;
    char                  title[160];
    char                  base_url[768];
    browsercss_Rule       rules[BCSS_MAX_RULES];
    int                   rule_count;
    browsercss_Run        runs[BCSS_MAX_RUNS];
    int                   run_count;
    browsercss_Box        boxes[BCSS_MAX_BOXES];
    int                   box_count;
    browsercss_Link       links[BCSS_MAX_LINKS];
    int                   link_count;
};

static int browsercss_is_space(char ch)
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

static int browsercss_streq_nocase(const char *lhs, const char *rhs)
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

static void browsercss_copy(char *dst, int dst_size, const char *src)
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

static void browsercss_style_reset(browsercss_Style *style)
{
    memset(style, 0, sizeof(*style));
    style->font_px = BCSS_DEFAULT_FONT_PX;
    style->color = BCSS_TEXT_COLOR;
    style->display_mode = BCSS_DISPLAY_INLINE;
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
    if ((src->props & BCSS_PROP_JUSTIFY) != 0u) dst->justify_center = src->justify_center;
    if ((src->props & BCSS_PROP_ALIGN) != 0u) dst->align_center = src->align_center;
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
    int count = 0;
    const char *p = value;
    char token[32];
    int token_len;

    while (*p != '\0' && count < 4) {
        while (browsercss_is_space(*p)) ++p;
        if (*p == '\0') break;
        token_len = 0;
        while (*p != '\0' && !browsercss_is_space(*p) && token_len < (int)sizeof(token) - 1) {
            token[token_len++] = *p++;
        }
        token[token_len] = '\0';
        numbers[count++] = browsercss_parse_length(doc, token, style->font_px > 0 ? style->font_px : BCSS_DEFAULT_FONT_PX);
    }

    if (count == 1) {
        numbers[1] = numbers[2] = numbers[3] = numbers[0];
    } else if (count == 2) {
        numbers[2] = numbers[0];
        numbers[3] = numbers[1];
    } else if (count == 3) {
        numbers[3] = numbers[1];
    }

    if (count > 0) {
        if (bit_top == BCSS_PROP_MARGIN_TOP) {
            style->margin_top = numbers[0];
            style->margin_right = numbers[1];
            style->margin_bottom = numbers[2];
            style->margin_left = numbers[3];
            style->props |= BCSS_PROP_MARGIN_TOP | BCSS_PROP_MARGIN_RIGHT | BCSS_PROP_MARGIN_BOTTOM | BCSS_PROP_MARGIN_LEFT;
        } else {
            style->padding_top = numbers[0];
            style->padding_right = numbers[1];
            style->padding_bottom = numbers[2];
            style->padding_left = numbers[3];
            style->props |= BCSS_PROP_PADDING_TOP | BCSS_PROP_PADDING_RIGHT | BCSS_PROP_PADDING_BOTTOM | BCSS_PROP_PADDING_LEFT;
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

        if (browsercss_streq_nocase(name, "color")) {
            if (browsercss_parse_color(value, &style->color)) style->props |= BCSS_PROP_COLOR;
        } else if (browsercss_streq_nocase(name, "background-color") || browsercss_streq_nocase(name, "background")) {
            if (browsercss_parse_color(value, &style->background)) style->props |= BCSS_PROP_BACKGROUND;
        } else if (browsercss_streq_nocase(name, "font-size")) {
            style->font_px = browsercss_parse_length(doc, value, BCSS_DEFAULT_FONT_PX);
            if (style->font_px < 10) style->font_px = 10;
            style->props |= BCSS_PROP_FONT_SIZE;
        } else if (browsercss_streq_nocase(name, "font-weight")) {
            style->bold = browsercss_streq_nocase(value, "bold") || browsercss_parse_int(value) >= 600;
            style->props |= BCSS_PROP_BOLD;
        } else if (browsercss_streq_nocase(name, "text-decoration")) {
            style->underline = browsercss_contains_word_nocase(value, "underline");
            style->props |= BCSS_PROP_UNDERLINE;
        } else if (browsercss_streq_nocase(name, "display")) {
            if (browsercss_streq_nocase(value, "none")) style->display_mode = BCSS_DISPLAY_NONE;
            else if (browsercss_streq_nocase(value, "block")) style->display_mode = BCSS_DISPLAY_BLOCK;
            else if (browsercss_streq_nocase(value, "flex")) style->display_mode = BCSS_DISPLAY_FLEX;
            else style->display_mode = BCSS_DISPLAY_INLINE;
            style->props |= BCSS_PROP_DISPLAY;
        } else if (browsercss_streq_nocase(name, "justify-content")) {
            style->justify_center = browsercss_streq_nocase(value, "center");
            style->props |= BCSS_PROP_JUSTIFY;
        } else if (browsercss_streq_nocase(name, "align-items")) {
            style->align_center = browsercss_streq_nocase(value, "center");
            style->props |= BCSS_PROP_ALIGN;
        } else if (browsercss_streq_nocase(name, "min-height")) {
            style->min_height = browsercss_parse_length(doc, value, style->font_px > 0 ? style->font_px : BCSS_DEFAULT_FONT_PX);
            style->props |= BCSS_PROP_MIN_HEIGHT;
        } else if (browsercss_streq_nocase(name, "max-width")) {
            style->max_width = browsercss_parse_length(doc, value, style->font_px > 0 ? style->font_px : BCSS_DEFAULT_FONT_PX);
            style->props |= BCSS_PROP_MAX_WIDTH;
        } else if (browsercss_streq_nocase(name, "white-space")) {
            style->preformatted = browsercss_streq_nocase(value, "pre") || browsercss_streq_nocase(value, "pre-wrap");
            style->props |= BCSS_PROP_PRE;
        } else if (browsercss_streq_nocase(name, "margin")) {
            browsercss_parse_box_shorthand(doc, style, BCSS_PROP_MARGIN_TOP, value);
        } else if (browsercss_streq_nocase(name, "padding")) {
            browsercss_parse_box_shorthand(doc, style, BCSS_PROP_PADDING_TOP, value);
        } else if (browsercss_streq_nocase(name, "margin-top")) {
            style->margin_top = browsercss_parse_length(doc, value, style->font_px);
            style->props |= BCSS_PROP_MARGIN_TOP;
        } else if (browsercss_streq_nocase(name, "margin-right")) {
            style->margin_right = browsercss_parse_length(doc, value, style->font_px);
            style->props |= BCSS_PROP_MARGIN_RIGHT;
        } else if (browsercss_streq_nocase(name, "margin-bottom")) {
            style->margin_bottom = browsercss_parse_length(doc, value, style->font_px);
            style->props |= BCSS_PROP_MARGIN_BOTTOM;
        } else if (browsercss_streq_nocase(name, "margin-left")) {
            style->margin_left = browsercss_parse_length(doc, value, style->font_px);
            style->props |= BCSS_PROP_MARGIN_LEFT;
        } else if (browsercss_streq_nocase(name, "padding-top")) {
            style->padding_top = browsercss_parse_length(doc, value, style->font_px);
            style->props |= BCSS_PROP_PADDING_TOP;
        } else if (browsercss_streq_nocase(name, "padding-right")) {
            style->padding_right = browsercss_parse_length(doc, value, style->font_px);
            style->props |= BCSS_PROP_PADDING_RIGHT;
        } else if (browsercss_streq_nocase(name, "padding-bottom")) {
            style->padding_bottom = browsercss_parse_length(doc, value, style->font_px);
            style->props |= BCSS_PROP_PADDING_BOTTOM;
        } else if (browsercss_streq_nocase(name, "padding-left")) {
            style->padding_left = browsercss_parse_length(doc, value, style->font_px);
            style->props |= BCSS_PROP_PADDING_LEFT;
        }
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

typedef struct browsercss_TextBuffer
{
    char  *data;
    size_t length;
    size_t capacity;
    int    failed;
} browsercss_TextBuffer;

static void browsercss_text_buffer_init(browsercss_TextBuffer *buffer)
{
    if (buffer == NULL) {
        return;
    }

    buffer->capacity = 256;
    buffer->length = 0;
    buffer->failed = 0;
    buffer->data = (char *)malloc(buffer->capacity);
    if (buffer->data == NULL) {
        buffer->capacity = 0;
        buffer->failed = 1;
        return;
    }

    buffer->data[0] = '\0';
}

static int browsercss_text_buffer_reserve(browsercss_TextBuffer *buffer, size_t extra)
{
    char *data;
    size_t needed;
    size_t capacity;

    if (buffer == NULL || buffer->failed) {
        return 0;
    }

    needed = buffer->length + extra + 1;
    if (needed <= buffer->capacity) {
        return 1;
    }

    capacity = buffer->capacity > 0 ? buffer->capacity : 256;
    while (capacity < needed) {
        if (capacity > (size_t)-1 / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }

    data = (char *)realloc(buffer->data, capacity);
    if (data == NULL) {
        buffer->failed = 1;
        return 0;
    }

    buffer->data = data;
    buffer->capacity = capacity;
    return 1;
}

static void browsercss_text_buffer_destroy(browsercss_TextBuffer *buffer)
{
    if (buffer == NULL) {
        return;
    }

    if (buffer->data != NULL) {
        free(buffer->data);
    }

    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
    buffer->failed = 0;
}

static lxb_status_t browsercss_lexbor_text_cb(const lxb_char_t *data, size_t length, void *ctx)
{
    browsercss_TextBuffer *buffer = (browsercss_TextBuffer *)ctx;

    if (buffer == NULL) {
        return LXB_STATUS_ERROR_WRONG_ARGS;
    }

    if (length == 0) {
        return LXB_STATUS_OK;
    }

    if (!browsercss_text_buffer_reserve(buffer, length)) {
        return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
    }

    memcpy(buffer->data + buffer->length, data, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';

    return LXB_STATUS_OK;
}

static int browsercss_add_rule_from_text(browser_css_document *doc,
                                         const char *selector_text,
                                         const char *declaration_text)
{
    browsercss_Style decl;
    char            *selectors;
    char            *sel_ptr;
    int              added = 0;

    if (doc == NULL || selector_text == NULL || declaration_text == NULL) {
        return 0;
    }

    selectors = (char *)malloc(strlen(selector_text) + 1);
    if (selectors == NULL) {
        return 0;
    }

    strcpy(selectors, selector_text);
    browsercss_style_reset(&decl);
    browsercss_parse_declarations(doc, &decl, declaration_text);

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

        rule->style = decl;
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

static void browsercss_collect_lexbor_rules(browser_css_document *doc, const lxb_css_rule_list_t *list)
{
    const lxb_css_rule_t *rule;

    if (doc == NULL || list == NULL) {
        return;
    }

    for (rule = list->first; rule != NULL && doc->rule_count < BCSS_MAX_RULES; rule = rule->next) {
        if (rule->type == LXB_CSS_RULE_STYLE) {
            const lxb_css_rule_style_t *style = lxb_css_rule_style(rule);
            browsercss_TextBuffer selector_text;
            browsercss_TextBuffer declaration_text;
            lxb_status_t status;

            if (style == NULL) {
                continue;
            }

            browsercss_text_buffer_init(&selector_text);
            browsercss_text_buffer_init(&declaration_text);

            status = LXB_STATUS_ERROR;
            if (!selector_text.failed && !declaration_text.failed &&
                style->selector != NULL && style->declarations != NULL) {
                status = lxb_css_selector_serialize_list_chain(style->selector,
                                                               browsercss_lexbor_text_cb,
                                                               &selector_text);
                if (status == LXB_STATUS_OK) {
                    status = lxb_css_rule_declaration_list_serialize(style->declarations,
                                                                     browsercss_lexbor_text_cb,
                                                                     &declaration_text);
                }
            }

            if (status == LXB_STATUS_OK && !selector_text.failed && !declaration_text.failed &&
                selector_text.data != NULL && declaration_text.data != NULL) {
                (void)browsercss_add_rule_from_text(doc,
                                                    selector_text.data,
                                                    declaration_text.data);
            }

            browsercss_text_buffer_destroy(&selector_text);
            browsercss_text_buffer_destroy(&declaration_text);

            if (style->child != NULL) {
                browsercss_collect_lexbor_rules(doc, style->child);
            }
        } else if (rule->type == LXB_CSS_RULE_AT_RULE) {
            browsercss_collect_lexbor_rules(doc,
                                            browsercss_lexbor_at_rule_block(lxb_css_rule_at(rule)));
        } else if (rule->type == LXB_CSS_RULE_LIST) {
            browsercss_collect_lexbor_rules(doc, lxb_css_rule_list(rule));
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
        browsercss_collect_lexbor_rules(doc, lxb_css_rule_list(stylesheet->root));
        ok = 1;
    }

    (void)lxb_css_stylesheet_destroy(stylesheet, true);
    (void)lxb_css_parser_destroy(parser, true);
    return ok;
}

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

static const char *browsercss_find_closing(const char *text, const char *closing)
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

static void browsercss_extract_tag_name(const char *tag_text, char *tag, int tag_size)
{
    int i = 0;
    while (*tag_text != '\0' && browsercss_is_space(*tag_text)) ++tag_text;
    while (*tag_text != '\0' && !browsercss_is_space(*tag_text) && *tag_text != '/' && *tag_text != '>' &&
           i < tag_size - 1) {
        tag[i++] = browsercss_to_lower(*tag_text++);
    }
    tag[i] = '\0';
}

static int browsercss_extract_attr(const char *tag_text, const char *name, char *out, int out_size)
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

static void browsercss_compute_style(browser_css_document *doc,
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

static int browsercss_add_box(browser_css_document *doc, int x, int y, int width, int height, unsigned int color)
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

static void browsercss_open_block(browser_css_document *doc,
                                  browsercss_Context *parent,
                                  const browsercss_Frame *frame,
                                  browsercss_Context *ctx)
{
    int box_x;
    int box_y;
    int box_width;

    browsercss_finalize_line(doc, parent);

    box_x = parent->content_x + frame->style.margin_left;
    box_y = parent->cursor_y + frame->style.margin_top;
    box_width = parent->content_width - frame->style.margin_left - frame->style.margin_right;
    if (frame->style.max_width > 0 && box_width > frame->style.max_width) {
        box_width = frame->style.max_width;
    }
    if (box_width < 32) {
        box_width = 32;
    }

    ctx->style = frame->style;
    ctx->box_x = box_x;
    ctx->box_y = box_y;
    ctx->box_width = box_width;
    ctx->content_x = box_x + frame->style.padding_left;
    ctx->content_width = box_width - frame->style.padding_left - frame->style.padding_right;
    if (ctx->content_width < 16) {
        ctx->content_width = 16;
    }
    ctx->content_height = frame->style.min_height - frame->style.padding_top - frame->style.padding_bottom;
    if (ctx->content_height < 0) {
        ctx->content_height = 0;
    }
    ctx->cursor_y = box_y + frame->style.padding_top;
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

static void browsercss_close_block(browser_css_document *doc,
                                   browsercss_Context *parent,
                                   browsercss_Context *ctx)
{
    int box_height;
    int dx = 0;
    int dy = 0;
    browsercss_finalize_line(doc, ctx);
    box_height = (ctx->cursor_y - ctx->box_y) + ctx->style.padding_bottom;
    if (ctx->style.min_height > 0 && box_height < ctx->style.min_height) {
        box_height = ctx->style.min_height;
    }
    if (box_height < ctx->style.font_px + 6) {
        box_height = ctx->style.font_px + 6;
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
    parent->cursor_y = ctx->box_y + box_height + ctx->style.margin_bottom;
    parent->line_y = parent->cursor_y;
    parent->line_x = parent->content_x;
}

static void browsercss_layout_html(browser_css_document *doc, const char *html)
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
                if (end == NULL) break;
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
                if (end == NULL) break;
                {
                    int len = (int)(end - p - 1);
                    if (len >= (int)sizeof(tag_text)) len = (int)sizeof(tag_text) - 1;
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
            if (end == NULL) break;
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

extern "C" void browser_css_draw(browser_css_document *document,
                                 unsigned long long window,
                                 int x,
                                 int y,
                                 int scroll_y,
                                 int width,
                                 int height)
{
    int i;
    if (document == NULL) return;

    for (i = 0; i < document->box_count; ++i) {
        const browsercss_Box *box = &document->boxes[i];
        int draw_y = y + box->y - scroll_y;
        if (draw_y + box->height < y || draw_y > y + height) continue;
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
        if (draw_y + run->height < y || draw_y > y + height) continue;
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
                          (UINT32)(draw_x + run->width),
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
    if (document == NULL) return "";
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

extern "C" int browser_css_mouse_click(browser_css_document *document, int x, int y, int scroll_y)
{
    int i;
    if (document == NULL) return 0;

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
