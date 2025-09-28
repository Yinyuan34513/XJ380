#include "x3api.h"
#include "krlibc.h"
#include "gh_sign.h"
#include "st_proto.h"

void paint_arrow_sign(int sx, int sy)
{
    UserInfo current_user;
    uint32_t user_name_width;
    xapi_GetCurrentUser(current_user);
    xapi_DrawTextl(handle, sx + 12, sy, current_user.name, 10, 0xffffffff, &user_name_width);
    xapi_DrawRect(handle, sx, sy, sx + user_name_width + 4, sy + 20, 0x00a2e8ff, true);

    for (int y = 0; y < 10; y++) {
        for (int x = 0; x < 20; x++) {
            if (arrow_head[y][x] == '@') xapi_DrawPoint(handle, sx + x, sy + y, 0x00a2e8ff);
        }
    }
}