#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct browser_css_document browser_css_document;

typedef int (*browser_css_fetch_resource_fn)(void *user_data,
                                             const char *url,
                                             const char *base_url,
                                             char **body_out,
                                             int *body_len_out,
                                             char *final_url_out,
                                             int final_url_out_size);

typedef void (*browser_css_navigate_fn)(void *user_data, const char *url, const char *base_url);

typedef struct browser_css_callbacks
{
    void                         *user_data;
    browser_css_fetch_resource_fn fetch_resource;
    browser_css_navigate_fn       navigate;
} browser_css_callbacks;

browser_css_document *browser_css_create(const browser_css_callbacks *callbacks);
void                  browser_css_destroy(browser_css_document *document);
void                  browser_css_clear(browser_css_document *document);
int                   browser_css_load_html(browser_css_document *document,
                                            const char *html,
                                            const char *base_url,
                                            int viewport_width,
                                            int viewport_height);
void                  browser_css_draw(browser_css_document *document,
                                       unsigned long long window,
                                       int x,
                                       int y,
                                       int scroll_y,
                                       int width,
                                       int height);
int                   browser_css_height(const browser_css_document *document);
const char           *browser_css_title(const browser_css_document *document);
int                   browser_css_mouse_move(browser_css_document *document, int x, int y, int scroll_y);
int                   browser_css_mouse_leave(browser_css_document *document);
int                   browser_css_mouse_click(browser_css_document *document, int x, int y, int scroll_y);

#ifdef __cplusplus
}
#endif
