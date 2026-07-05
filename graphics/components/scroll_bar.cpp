#include <global_color.h>
#include <graphics/components/scroll_bar.h>
#include <krlibc.h>
#include <proto.hpp>
#include <syscall/syscall.h>

static scroll_bar_regt_p first_scroll_bar;

static const SHEET_BUFFER SCROLL_BAR_TRACK_COLOR  = {0xe7, 0xee, 0xf6, 0xff};
static const SHEET_BUFFER SCROLL_BAR_BORDER_COLOR = {0x9e, 0xaf, 0xc5, 0xff};
static const SHEET_BUFFER SCROLL_BAR_THUMB_COLOR  = {0xb8, 0xc7, 0xd9, 0xff};
static const SHEET_BUFFER SCROLL_BAR_CLEAR_COLOR  = WHITE;

static int normalize_scroll_bar_length(int length)
{
    if (length < SCROLL_BAR_SIZE) return SCROLL_BAR_SIZE;
    return length;
}

static int normalize_scroll_bar_thumb_length(int length, int thumb_length)
{
    if (thumb_length < SCROLL_BAR_MIN_THUMB) thumb_length = SCROLL_BAR_MIN_THUMB;
    if (thumb_length > length) thumb_length = length;
    return thumb_length;
}

static void draw_scroll_bar(scroll_bar_regt_p bar)
{
    if (bar == NULL || bar->obj_sheet == NULL) return;

    draw_rect(sht_img, bar->obj_sheet, bar->x1, bar->y1, bar->x2, bar->y2, SCROLL_BAR_TRACK_COLOR);
    draw_line(sht_img, bar->obj_sheet, bar->x1, bar->y1, bar->x2, bar->y1, SCROLL_BAR_BORDER_COLOR);
    draw_line(sht_img, bar->obj_sheet, bar->x1, bar->y2, bar->x2, bar->y2, SCROLL_BAR_BORDER_COLOR);
    draw_line(sht_img, bar->obj_sheet, bar->x1, bar->y1, bar->x1, bar->y2, SCROLL_BAR_BORDER_COLOR);
    draw_line(sht_img, bar->obj_sheet, bar->x2, bar->y1, bar->x2, bar->y2, SCROLL_BAR_BORDER_COLOR);

    draw_rect(sht_img, bar->obj_sheet, bar->thumb_x1, bar->thumb_y1, bar->thumb_x2, bar->thumb_y2,
              SCROLL_BAR_THUMB_COLOR);
    draw_line(sht_img, bar->obj_sheet, bar->thumb_x1, bar->thumb_y1, bar->thumb_x2, bar->thumb_y1,
              SCROLL_BAR_BORDER_COLOR);
    draw_line(sht_img, bar->obj_sheet, bar->thumb_x1, bar->thumb_y2, bar->thumb_x2, bar->thumb_y2,
              SCROLL_BAR_BORDER_COLOR);
    draw_line(sht_img, bar->obj_sheet, bar->thumb_x1, bar->thumb_y1, bar->thumb_x1, bar->thumb_y2,
              SCROLL_BAR_BORDER_COLOR);
    draw_line(sht_img, bar->obj_sheet, bar->thumb_x2, bar->thumb_y1, bar->thumb_x2, bar->thumb_y2,
              SCROLL_BAR_BORDER_COLOR);
}

static void refresh_scroll_bar(scroll_bar_regt_p bar)
{
    if (bar == NULL || bar->obj_sheet == NULL) return;

    refresh_part_sheet(sht_img, getBX(sht_img, bar->obj_sheet) + bar->x1, getBY(sht_img, bar->obj_sheet) + bar->y1,
                       getBX(sht_img, bar->obj_sheet) + bar->x2 + 1,
                       getBY(sht_img, bar->obj_sheet) + bar->y2 + 1);
}

static void clear_scroll_bar(scroll_bar_regt_p bar)
{
    if (bar == NULL || bar->obj_sheet == NULL) return;

    draw_rect(sht_img, bar->obj_sheet, bar->x1, bar->y1, bar->x2, bar->y2, SCROLL_BAR_CLEAR_COLOR);
    refresh_scroll_bar(bar);
}

void register_scroll_bar_components(SHEET *sheet, int x, int y, int length, int thumb_length, int CRLid,
                                    ScrollBarDirection direction)
{
    if (sheet == NULL) return;

    length       = normalize_scroll_bar_length(length);
    thumb_length = normalize_scroll_bar_thumb_length(length, thumb_length);

    scroll_bar_regt_p new_bar = (scroll_bar_regt_p)malloc(sizeof(scroll_bar_regt));
    if (new_bar == NULL) return;
    memset(new_bar, 0, sizeof(scroll_bar_regt));

    new_bar->obj_sheet  = sheet;
    new_bar->x1         = x;
    new_bar->y1         = y;
    new_bar->CRLid      = CRLid;
    new_bar->direction  = direction;
    if (direction == ScrollBarVertical)
    {
        new_bar->x2       = x + SCROLL_BAR_SIZE - 1;
        new_bar->y2       = y + length - 1;
        new_bar->thumb_x1 = x + 2;
        new_bar->thumb_y1 = y + 2;
        new_bar->thumb_x2 = x + SCROLL_BAR_SIZE - 3;
        new_bar->thumb_y2 = y + thumb_length - 3;
    }
    else
    {
        new_bar->x2       = x + length - 1;
        new_bar->y2       = y + SCROLL_BAR_SIZE - 1;
        new_bar->thumb_x1 = x + 2;
        new_bar->thumb_y1 = y + 2;
        new_bar->thumb_x2 = x + thumb_length - 3;
        new_bar->thumb_y2 = y + SCROLL_BAR_SIZE - 3;
    }

    scroll_bar_regt_p front_p = first_scroll_bar;
    if (front_p != NULL)
    {
        while (front_p->next != NULL) front_p = front_p->next;
        front_p->next = new_bar;
        new_bar->prev = front_p;
    }
    else
    {
        first_scroll_bar = new_bar;
    }

    draw_scroll_bar(new_bar);
    refresh_scroll_bar(new_bar);
}

void unregister_scroll_bar_components(SHEET *sheet, int CRLid)
{
    scroll_bar_regt_p front_p = first_scroll_bar;
    while (front_p != NULL)
    {
        scroll_bar_regt_p next = front_p->next;
        if (front_p->obj_sheet == sheet && front_p->CRLid == CRLid)
        {
            clear_scroll_bar(front_p);
            if (front_p->prev != NULL)
                front_p->prev->next = front_p->next;
            else
                first_scroll_bar = front_p->next;
            if (front_p->next != NULL) front_p->next->prev = front_p->prev;
            free(front_p);
        }
        front_p = next;
    }
}

void process_scroll_bar_click_event(SHEET *current_sht, int x, int y)
{
    scroll_bar_regt_p front_p = first_scroll_bar;
    while (front_p != NULL)
    {
        if (front_p->obj_sheet == current_sht && x >= front_p->x1 && x <= front_p->x2 &&
            y >= front_p->y1 && y <= front_p->y2)
        {
            WINDOWLSP current_window = sht_found_win(xwmii, sht_img, current_sht);
            if (current_window != NULL)
            {
                do_message(MSG_CRL, front_p->CRLid, NULL, current_window->WinMPf, current_window->w_task);
            }
        }
        front_p = front_p->next;
    }
}

void put_scroll_bar_theme(WINDOWLS *windowls, int x, int y, int length, int thumb_length, int CRLid,
                          ScrollBarDirection direction)
{
    if (windowls == NULL) return;

    SHEET *sheet = found_sheet_byid(sht_img, windowls->w_sheet);
    register_scroll_bar_components(sheet, x, y, length, thumb_length, CRLid, direction);
}
