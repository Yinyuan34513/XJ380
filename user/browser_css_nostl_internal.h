#pragma once

#include "./xapi/include/x3api.h"
#include "browser_css_nostl.h"

#define BCSS_MAX_RULES 256
#define BCSS_MAX_RUNS 4096
#define BCSS_MAX_BOXES 1024
#define BCSS_MAX_LINKS 512
#define BCSS_MAX_IMAGES 64
#define BCSS_MAX_VARS 128
#define BCSS_MAX_STACK 64
#define BCSS_MAX_SELECTOR 64
#define BCSS_MAX_SELECTOR_PARTS 8
#define BCSS_MAX_SELECTOR_CLASSES 4
#define BCSS_MAX_TAG 24
#define BCSS_MAX_TEXT 192
#define BCSS_DEFAULT_FONT_PX 16
#define BCSS_LINK_COLOR 0x2a6f97ff
#define BCSS_TEXT_COLOR 0x1f2933ff

enum
{
    BCSS_DISPLAY_INLINE = 0,
    BCSS_DISPLAY_BLOCK = 1,
    BCSS_DISPLAY_NONE = 2,
    BCSS_DISPLAY_FLEX = 3
};

enum
{
    BCSS_PROP_COLOR = 1ull << 0,
    BCSS_PROP_BACKGROUND = 1ull << 1,
    BCSS_PROP_FONT_SIZE = 1ull << 2,
    BCSS_PROP_DISPLAY = 1ull << 3,
    BCSS_PROP_BOLD = 1ull << 4,
    BCSS_PROP_UNDERLINE = 1ull << 5,
    BCSS_PROP_MARGIN_TOP = 1ull << 6,
    BCSS_PROP_MARGIN_RIGHT = 1ull << 7,
    BCSS_PROP_MARGIN_BOTTOM = 1ull << 8,
    BCSS_PROP_MARGIN_LEFT = 1ull << 9,
    BCSS_PROP_PADDING_TOP = 1ull << 10,
    BCSS_PROP_PADDING_RIGHT = 1ull << 11,
    BCSS_PROP_PADDING_BOTTOM = 1ull << 12,
    BCSS_PROP_PADDING_LEFT = 1ull << 13,
    BCSS_PROP_PRE = 1ull << 14,
    BCSS_PROP_MIN_HEIGHT = 1ull << 15,
    BCSS_PROP_MAX_WIDTH = 1ull << 16,
    BCSS_PROP_JUSTIFY = 1ull << 17,
    BCSS_PROP_ALIGN = 1ull << 18,
    BCSS_PROP_WIDTH = 1ull << 19,
    BCSS_PROP_HEIGHT = 1ull << 20,
    BCSS_PROP_TEXT_ALIGN = 1ull << 21,
    BCSS_PROP_MARGIN_LEFT_AUTO = 1ull << 22,
    BCSS_PROP_MARGIN_RIGHT_AUTO = 1ull << 23,
    BCSS_PROP_BORDER_WIDTH = 1ull << 24,
    BCSS_PROP_BORDER_COLOR = 1ull << 25,
    BCSS_PROP_LINE_HEIGHT = 1ull << 26,
    BCSS_PROP_BORDER_TOP_WIDTH = 1ull << 27,
    BCSS_PROP_BORDER_RIGHT_WIDTH = 1ull << 28,
    BCSS_PROP_BORDER_BOTTOM_WIDTH = 1ull << 29,
    BCSS_PROP_BORDER_LEFT_WIDTH = 1ull << 30,
    BCSS_PROP_NOWRAP = 1ull << 31,
    BCSS_PROP_BOX_SIZING = 1ull << 32,
    BCSS_PROP_WORD_BREAK = 1ull << 33
};

enum
{
    BCSS_COMBINATOR_NONE = 0,
    BCSS_COMBINATOR_DESCENDANT = 1,
    BCSS_COMBINATOR_CHILD = 2
};

enum
{
    BCSS_TEXT_ALIGN_LEFT = 0,
    BCSS_TEXT_ALIGN_CENTER = 1,
    BCSS_TEXT_ALIGN_RIGHT = 2
};

/*
 * 这是布局阶段传递的核心样式对象。
 * 目前它不是完整 CSSOM，而是“当前渲染器真正会用到的最小子集”：
 * 1. 解析器把声明折叠进这里。
 * 2. 选择器命中后按 props 位图覆盖。
 * 3. 布局和绘制只读取这里，不再回头看原始 CSS 文本。
 *
 * 后续如果你要扩展更多属性，通常分三步：
 * 1. 在这里新增字段。
 * 2. 在 props 里加一个对应的“是否显式设置”位。
 * 3. 在 parse_declaration / style_apply / render 使用点补齐。
 */
typedef struct browsercss_Style
{
    unsigned long long props;
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
    int          nowrap;
    int          break_word;
    int          min_height;
    int          max_width;
    int          width;
    int          height;
    int          box_sizing_border_box;
    int          justify_center;
    int          align_center;
    int          text_align;
    int          line_height;
    int          margin_left_auto;
    int          margin_right_auto;
    int          border_width;
    int          border_top_width;
    int          border_right_width;
    int          border_bottom_width;
    int          border_left_width;
    unsigned int border_color;
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
    int                     part_count;
    browsercss_Style        style;
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

typedef struct browsercss_Image
{
    int      x;
    int      y;
    int      width;
    int      height;
    int      source_width;
    int      source_height;
    int      link_index;
    XCOLORA *pixels;
} browsercss_Image;

/* 变量表只做最朴素的名字到文本值映射，供 var(--x) 替换阶段使用。 */
typedef struct browsercss_Variable
{
    char name[64];
    char value[192];
} browsercss_Variable;

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
    int              line_box_start;
    int              line_link_start;
    int              line_image_start;
    int              box_index;
    int              content_height;
    int              first_run_index;
    int              first_box_index;
    int              first_link_index;
    int              first_image_index;
} browsercss_Context;

/*
 * The document owns parsed CSS rules plus the flattened draw lists that the
 * layout/render path produces.
 */
/*
 * 文档对象同时承担三层职责：
 * 1. 保存外部回调与视口信息。
 * 2. 保存解析后的 CSS 规则、变量表。
 * 3. 保存布局展开后的扁平绘制列表。
 *
 * 现在的设计目标是“便于直接画出来”，不是保留完整 DOM/CSS 树。
 * 所以后面的渲染函数只遍历这些数组，不再做复杂查询。
 */
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
    browsercss_Image      images[BCSS_MAX_IMAGES];
    int                   image_count;
    browsercss_Variable   vars[BCSS_MAX_VARS];
    int                   var_count;
};

/* 解析阶段和布局/绘制阶段共用的辅助函数声明。 */
int         browsercss_is_space(char ch);
int         browsercss_streq_nocase(const char *lhs, const char *rhs);
void        browsercss_copy(char *dst, int dst_size, const char *src);
void        browsercss_style_reset(browsercss_Style *style);
const char *browsercss_find_closing(const char *text, const char *closing);
void        browsercss_extract_tag_name(const char *tag_text, char *tag, int tag_size);
int         browsercss_extract_attr(const char *tag_text, const char *name, char *out, int out_size);
void        browsercss_compute_style(browser_css_document *doc,
                                     const browsercss_Frame *frames,
                                     int frame_top,
                                     const browsercss_Style *parent,
                                     const char *tag,
                                     const char *class_name,
                                     const char *id,
                                     const char *inline_style,
                                     browsercss_Style *out);
void        browsercss_layout_html(browser_css_document *doc, const char *html);
int         browsercss_decode_webp_image(const char *data,
                                         int data_len,
                                         XCOLORA **pixels_out,
                                         int *width_out,
                                         int *height_out);
