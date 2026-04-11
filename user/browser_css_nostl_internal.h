#pragma once

#include "browser_css_nostl.h"

#define BCSS_MAX_RULES 256
#define BCSS_MAX_RUNS 4096
#define BCSS_MAX_BOXES 1024
#define BCSS_MAX_LINKS 512
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
    BCSS_PROP_ALIGN = 1u << 18,
    BCSS_PROP_WIDTH = 1u << 19,
    BCSS_PROP_HEIGHT = 1u << 20,
    BCSS_PROP_TEXT_ALIGN = 1u << 21,
    BCSS_PROP_MARGIN_LEFT_AUTO = 1u << 22,
    BCSS_PROP_MARGIN_RIGHT_AUTO = 1u << 23,
    BCSS_PROP_BORDER_WIDTH = 1u << 24,
    BCSS_PROP_BORDER_COLOR = 1u << 25
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
    int          width;
    int          height;
    int          justify_center;
    int          align_center;
    int          text_align;
    int          margin_left_auto;
    int          margin_right_auto;
    int          border_width;
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

/*
 * The document owns parsed CSS rules plus the flattened draw lists that the
 * layout/render path produces.
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
};

/* Shared helpers used by the parser half and the layout/render half. */
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
