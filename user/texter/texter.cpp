#include <x3api.h>
#include "texter.h"

HDLE handle;

char file_path[256] = "newfile";

int txr_width = TXR_X;
int txr_height = TXR_Y;
int text_offest = 0;
bool need_flush = false;
bool need_exit = false;

static int texter_visible_lines()
{
    int lines = (txr_height - 20) / 16;
    return lines < 1 ? 1 : lines;
}

void texter_MessagePrcor(UINT64 Type, UINT64 hData, UINT64 lData)
{
    switch (Type)
    {
    case MSG_CHAR:
        process_key((char)lData);
        break;
    case MSG_RESIZE:
        txr_width = (int)hData;
        txr_height = (int)lData;
        need_flush = true;
        break;
    }
    return;
}

static int texter_main_impl(int argc, char *argv[], char *envp[])
{
    (void)envp;

    if (argc == 1)
    {
        strcpy(file_path, argv[0]);
    }

    XWINDOW Winfo;
    Winfo.title  = (char *)"文本编辑器";
    Winfo.width  = TXR_X;
    Winfo.height = TXR_Y;
    Winfo.sets   = XWIN_NORMAL | XWIN_SUPPORT_MAXIMIZE;
    xapi_CreateWindow(&handle, &Winfo);
    xapi_SetIcon(handle, "/system/icon/texter.png");
    SetMsgPrcor(handle, texter_MessagePrcor);

    XFILE *f = xapi_OpenFile(file_path);
    if (!f) return -1;

flush:

    xapi_DrawRect(handle, 0, 0, txr_width - 1, txr_height - 1, 0xffffffff, true);

    uint64_t offest = (uint64_t)f->buffer;
    int visible_lines = texter_visible_lines();
    for (int i = 0; ; i++)
    {
        char file_buffer[256];
        uint64_t begin_offest = offest;
        offest += getline_from_file((void *)offest, file_buffer);
        if (offest == begin_offest || i - text_offest >= visible_lines) break;
        if (i >= text_offest) xapi_DrawSWText(handle, 4, 4 + (i - text_offest) * 16, file_buffer, 0x000000ff);
    }

    xapi_DrawRect(handle, 0, txr_height - 16, txr_width - 1, txr_height - 1, 0xeaeaeaff, true);
    xapi_DrawText(handle, 2, txr_height - 19, file_path, 11, 0x000000ff);

    xapi_RefreshWindow(handle);

    while (true)
    {
        __asm__ __volatile__("pause");
        if (need_flush)
        {
            need_flush = false;
            goto flush;
        }
        if (need_exit)
        {
            break;
        }
    }

    xapi_CloseFile(f);

    return 0;
}

extern "C" int texter_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int texter_main_cpp(int argc, char *argv[], char *envp[])
{
    return texter_main_impl(argc, argv, envp);
}

// 从文件里读取字符直到结束符/换行符，返回偏移量
uint64_t getline_from_file(void *buffer, char *str)
{
    char *before = (char *)buffer;
    char *p = (char *)buffer;
    char *q = str;

    while (*p && *p != '\n' && *p != '\r')
    {    
        *q = *p;
        p++; q++;
    }

    *q = '\0';

    while (*p && (*p == '\n' || *p == '\r'))
        p++;

    return p - before;
}

void process_key(char ch)
{
    if (ch == '8' && text_offest > 0)
        text_offest--;
    else if (ch == '2')
        text_offest++;
    else if (ch == 'q' || ch == 'Q')
        need_exit = true;

    need_flush = true;
}
