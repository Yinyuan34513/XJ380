#include <graphics/sheet.h>
#include <graphics/components/button.h>
#include <proto.hpp>
#include <ttf.h>
#include <syscall/syscall.h>

button_regt_p first_button;

// X 和 Y 为相对位置
void register_button_components(SHEET *sheet, int x1, int y1, int x2, int y2, int CRLid)
{
    button_regt_p new_button = (button_regt_p)malloc(sizeof(button_regt));
    new_button->obj_sheet = sheet;
    new_button->x1 = x1;
    new_button->y1 = y1;
    new_button->x2 = x2;
    new_button->y2 = y2;
    new_button->CRLid = CRLid;
    new_button->next = NULL;

    button_regt_p front_p = first_button;
    if (front_p != NULL)
    {
        while (true)
        {
            if (front_p->next == NULL)
            {
                // 仅需在末尾插入即可
                front_p->next = new_button;
                new_button->prev = front_p;
                break;
            }
            front_p = front_p->next;
        }
    }
    else
    {
        first_button = new_button;
        new_button->prev = NULL;
    }
}

void unregister_button_components(SHEET *sheet, int CRLid)
{
    button_regt_p front_p = first_button;
    while (true)
    {
        if (front_p == NULL)
        {
            break;
        }
        else if (front_p->obj_sheet == sheet && front_p->CRLid == CRLid)
        {
            if (front_p->prev != NULL) front_p->prev->next = front_p->next;
            if (front_p->next != NULL) front_p->next->prev = front_p->prev;
            free(front_p);
        }
        front_p = front_p->next;
    }
}

void process_click_event(SHEET *current_sht, int x, int y)
{
    button_regt_p front_p = first_button;
    while (true)
    {
        if (front_p == NULL)
        {
            break;
        }
        else if (front_p->obj_sheet == current_sht)
        {
            WINDOWLSP current_window = sht_found_win(xwmii, sht_img, current_sht);
            if (x >= front_p->x1 && x <= front_p->x2 
             && y >= front_p->y1 && y <= front_p->y2)
            {
                if (current_window != NULL)
                {
                    do_message(MSG_CRL, front_p->CRLid, NULL, current_window->WinMPf, current_window->w_task);
                }
            }
        }
        front_p = front_p->next;
    }
}

void put_button_theme(WINDOWLS *windowls, int x, int y, char *str, int CRLid, bool underline)
{
    uint64_t width = calc_ttf_length(str, 10);
    SHEET_BUFFER *mtl;
    SHEET_BUFFER *dst = (SHEET_BUFFER *)malloc((width + 22) * 24 * 4);
    if (dst == NULL) return;
    if (!underline) { LoadPictureOgM(&mtl, "/system/icon/button.png"); }
    else { LoadPictureOgM(&mtl, "/system/icon/buttonemp.png"); }
    resize_theme_width(dst, mtl, width + 22, 52, 24);
    copy_buffer_blend_by_id(sht_img, windowls->w_sheet, dst, x, y, width + 22, 24);
    register_button_components(found_sheet_byid(sht_img, windowls->w_sheet), 
                               x, y, x + width + 22, y + 24, CRLid);

    if (!underline) { print_box_ttf(sht_img, windowls->w_sheet, str, {0x00, 0x00, 0x00, 0xff}, x + 12, y + 2, 10); }
    else { print_box_ttf(sht_img, windowls->w_sheet, str, {0xff, 0xff, 0xff, 0xff}, x + 12, y + 2, 10); }
    
    free(mtl);
    free(dst);
}
