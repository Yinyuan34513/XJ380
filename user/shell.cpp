#include "./xapi/include/xposix.h"
#include "./xapi/include/xguiapi.h"

int main(int argc, char *argv[], char *envp[])
{
    write(1, "shell process is running!!!\n", 0);

    HDLE handle;
    XWINDOW Winfo;
    Winfo.title = "终端";
    Winfo.width = 320;
    Winfo.height = 180;
    xapi_CreateWindow(&handle, &Winfo);
    xapi_SetWindowTitle(handle, "Terminal");
    xapi_DrawLine(handle, 0, 0, 12, 12, 0x000000ff);
    xapi_DrawRect(handle, 0, 0, 12, 12, 0x000000ff, true);
    xapi_DrawText(handle, 20, 20, "文本Text", 11, 0x000000aa);

    while (1)
        __asm__ __volatile__("pause");
}
