#include "../include/include.h"
#include "xj380_logo_rect_72x64px.hpp"

//初始化桌面
void init_desktop(const FrameBufferConfig &fbc)
{
    int scrx = fbc.horizontal_resolution;
    int scry = fbc.vertical_resolution;
    //#136dff
    rect(fbc, 0, 0, scrx, scry, {0x13, 0x6d, 0xff});
    init_dock(fbc);
    init_shortcut_dock(fbc);
}

void init_dock(const FrameBufferConfig &fbc)
{
    int scrx = fbc.horizontal_resolution;
    int scry = fbc.vertical_resolution;
    int tmpx = scrx  * 0.75;            
    int tmpy = scry - 48;                // 75%
    rect(fbc, 0, scry - 48, tmpx, scry, {230, 230, 230});

    for (int y = 0; y <= 47; y++) {
        for (int x = 0; x <= y; x++) {
            dot(fbc, x + tmpx, y + tmpy, {230, 230, 230});
        }
    }

    draw_logo(fbc, 24, scry - 24);
}

void init_shortcut_dock(const FrameBufferConfig &fbc)
{
    int x1 = fbc.horizontal_resolution * 0.1875;
    int y1 = 0;
    int x2 = fbc.horizontal_resolution * 0.8125;
    int y2 = 24;
    rect(fbc, x1, y1, x2, y2, {230, 230, 230});

    WriteString(fbc, x1 + 24 + 8, 4, "Files", {0, 0, 0}); 
    WriteString(fbc, x1 + 24 + 8 + 40 + 8, 4, "Settings", {0, 0, 0}); 
}

void draw_logo(const FrameBufferConfig &fbc, int xi, int yi)
{
    yi -= 63;
    for (int y = 0; y < 63; y++) {
        for (int x = 0; x < 73; x++) {
            if (xj380_logo_rect_72x64px[y][x] == '.')
            {
                // pass
            }
            else if (xj380_logo_rect_72x64px[y][x] == '#')
            {
                dot(fbc, x + xi, y + yi, {0, 162, 232});
            }
            else if (xj380_logo_rect_72x64px[y][x] == '@')
            {
                dot(fbc, x + xi, y + yi, {255, 230, 0});
            }
        }
    }
}
