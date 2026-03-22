#include <x3api.h>
#include <krlibc.h>
#include "fm_proto.h"

bool first_click = false;
bool double_click = false;
int double_click_file_index = -1;
UINT64 clicktime_recorder = 0;
int last_click_file_index = -1;

int click_x = 0;
int click_y = 0;

int choosing_index = -1;
int choosing_history = -1;

volatile bool register_lock = false;
bool yicixing_lock = false;

bool need_paint = false;

static bool get_clicked_file_index(int x, int y, int *file_index, int *visible_index)
{
    if (x < 4 || x > FMR_X / 10 * 8 || y < 148)
    {
        return false;
    }

    int current_visible_index = (y - 148) / 24;
    int visible_count = file_count - file_count_base;
    if (visible_count > 12)
    {
        visible_count = 12;
    }

    if (visible_count <= 0 || current_visible_index < 0 || current_visible_index >= visible_count)
    {
        return false;
    }

    *visible_index = current_visible_index;
    *file_index = file_count_base + current_visible_index;
    return true;
}

void reg_lock()
{
    while (register_lock);
    register_lock = true;
    need_paint = false;
}

void reg_unlock()
{
    register_lock = false;
}

void register_click(int x, int y)
{
    if (register_lock) return;

    if (yicixing_lock)
    {
        yicixing_lock = false;
        return;
    }
    
    click_x = x;
    click_y = y;

    int  current_file_index = -1;
    int  current_visible_index = -1;
    bool click_on_file = get_clicked_file_index(x, y, &current_file_index, &current_visible_index);
    UINT64 current_time = xapi_GetTime();

    if (click_on_file && first_click && current_time - clicktime_recorder < 2 && current_file_index == last_click_file_index)
    {
        double_click = true;
        double_click_file_index = current_file_index;
        first_click = false;
    }
    else
    {
        double_click = false;
        double_click_file_index = -1;
        first_click = click_on_file;
        clicktime_recorder = click_on_file ? current_time : 0;
    }

    last_click_file_index = click_on_file ? current_file_index : -1;

    if (x >= 5 && x <= 23 && y >= 4 && y <= 22)
    {
        revert_path(current_path);
        need_paint = true;
    }
    else if (x >= 29 && x <= 47 && y >= 4 && y <= 22)
    {
        if (path_p != path_r)
        {
            if (path_p == 0) { path_p = 19; }
            else { path_p--; }
            strcpy(current_path, path_his[path_p]);

            need_paint = true;
        }
    }
    
    if (need_duopage)
    {
        if (x > FMR_X - 19)
        {
            if (y > 148 && y < 166 && file_count_base > 0)
            {
                file_count_base--;
                need_paint = true;
            }
            else if (y > 170 && y < 188 && file_count_base + 13 <= file_count)
            {
                file_count_base++;
                need_paint = true;
            }
        }
    }

    if (click_on_file)
    {
        if (double_click)
        {
            choosing_history = choosing_index;
            choosing_index = current_visible_index;
            return;
        }

        choosing_history = choosing_index;
        choosing_index = current_visible_index;
        if (choosing_history != choosing_index)
        {
            refresh_choose_change(choosing_history, choosing_index);
        }
    }
}
