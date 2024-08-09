#include "../include/include.h"

//初始化桌面
void init_desktop(const FrameBufferConfig &fbc)
{
    int scrx = fbc.horizontal_resolution;
    int scry = fbc.vertical_resolution;
    rect(fbc, 0, 0, scrx, scry, {0, 162, 232});
    init_dock(fbc);
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
}
