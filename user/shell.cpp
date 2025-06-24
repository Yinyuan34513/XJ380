#include "./xapi/include/xguiapi.h"
#include "./xapi/include/xposix.h"

HDLE handle;
void shell_MessagePrcor(UINT64 Type, UINT64 hData, UINT64 lData) {
    WSTR str;
    str[0] = lData;
    switch (Type) {
    case MSG_CHAR: xapi_DrawText(handle, 100, 100, str, 11, 0x000000ff); break;
    }
}

int main(int argc, char *argv[], char *envp[]) {
    write(1, "shell process is running!!!\n", 0);

    XWINDOW Winfo;
    Winfo.title  = "终端";
    Winfo.width  = 320;
    Winfo.height = 180;
    xapi_CreateWindow(&handle, &Winfo);
    xapi_SetWindowTitle(handle, "Terminal");
    xapi_DrawLine(handle, 0, 0, 12, 12, 0x000000ff);
    xapi_DrawRect(handle, 0, 0, 12, 12, 0x000000ff, true);
    xapi_DrawText(handle, 20, 20, "文本Text", 11, 0x000000aa);
    SetMsgPrcor(handle, shell_MessagePrcor);

    while (1)
        __asm__ __volatile__("pause");
}
