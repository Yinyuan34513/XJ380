#include <x3api.h>
#include <string.h>

static const UINT32 RUN_WIDTH = 520;
static const UINT32 RUN_HEIGHT = 210;
static const UINT64 RUN_BTN_OK = 1001;
static const UINT64 RUN_BTN_CANCEL = 1002;
static const int RUN_INPUT_MAX = 256;
static const int RUN_ARG_MAX = 16;

static HDLE g_handle = 0;
static UINT64 g_input_id = 0;
static bool g_running = true;
static char g_status[96] = "请输入 ELF 文件路径";

static void draw_status()
{
    xapi_DrawRect(g_handle, 16, 150, 310, 177, 0xffffffff, true);
    xapi_DrawSWText(g_handle, 18, 154, g_status, 0x4b5563ff);
    xapi_RefreshPartWindow(g_handle, 16, 150, 310, 177);
}

static void set_status(const char *text)
{
    if (text == NULL) text = "";
    strncpy(g_status, text, sizeof(g_status) - 1);
    g_status[sizeof(g_status) - 1] = '\0';
    draw_status();
}

static void close_run_dialog()
{
    if (g_input_id != 0)
    {
        xapi_DeleteTextInputBox(g_input_id);
        g_input_id = 0;
    }
    xapi_CloseWindow(g_handle);
    g_running = false;
}

static bool split_command(char *command, char **argv)
{
    if (command == NULL || argv == NULL) return false;

    while (*command == ' ' || *command == '\t') command++;
    if (*command == '\0') return false;

    int argc = 0;
    char *cursor = command;
    while (*cursor != '\0' && argc < RUN_ARG_MAX - 1)
    {
        while (*cursor == ' ' || *cursor == '\t')
        {
            *cursor = '\0';
            cursor++;
        }
        if (*cursor == '\0') break;

        argv[argc++] = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') cursor++;
    }
    argv[argc] = NULL;
    return argc > 0;
}

static void run_command()
{
    char command[RUN_INPUT_MAX];
    memset(command, 0, sizeof(command));
    xapi_GetTextInputBox(g_input_id, command);

    char *argv[RUN_ARG_MAX];
    memset(argv, 0, sizeof(argv));
    if (!split_command(command, argv))
    {
        set_status("请输入要运行的 ELF 路径");
        return;
    }

    INT64 pid = (INT64)xapi_RunArgs(argv[0], argv);
    if (pid <= 0)
    {
        set_status("启动失败，请检查路径是否正确");
        return;
    }

    close_run_dialog();
}

static void draw_run_dialog()
{
    xapi_DrawRect(g_handle, 0, 0, RUN_WIDTH - 1, RUN_HEIGHT - 1, 0xf4f8ffff, true);
    xapi_DrawRect(g_handle, 0, 0, RUN_WIDTH - 1, 58, 0x2474c6ff, true);
    xapi_DrawRect(g_handle, 0, 58, RUN_WIDTH - 1, RUN_HEIGHT - 1, 0xffffffff, true);
    xapi_DrawSWText(g_handle, 18, 18, (char *)"运行", 0xffffffff);
    xapi_DrawSWText(g_handle, 18, 78, (char *)"输入 ELF 文件路径，按 Enter 或“确定”运行。", 0x1d3557ff);
    xapi_DrawSWText(g_handle, 18, 116, (char *)"路径:", 0x111111ff);
    draw_status();

    xapi_DeleteButton(g_handle, RUN_BTN_OK);
    xapi_DeleteButton(g_handle, RUN_BTN_CANCEL);
    xapi_Button(g_handle, RUN_BTN_OK, 332, 160, (char *)"确定");
    xapi_ButtonEmp(g_handle, RUN_BTN_CANCEL, 418, 160, (char *)"取消");
    xapi_RefreshWindow(g_handle);
}

static void run_message(UINT64 type, UINT64 hData, UINT64 lData)
{
    switch (type)
    {
    case MSG_CRL:
        if (hData == RUN_BTN_OK) run_command();
        else if (hData == RUN_BTN_CANCEL) close_run_dialog();
        break;
    case MSG_KEYDOWN:
        if (lData == '\n') run_command();
        else if (lData == XKEY_ESC) close_run_dialog();
        break;
    default:
        break;
    }
}

static int elfrun_main_impl(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    XWINDOW window;
    window.title = (char *)"运行";
    window.width = RUN_WIDTH;
    window.height = RUN_HEIGHT;
    window.sets = XWIN_NORMAL;
    xapi_CreateWindow(&g_handle, &window);
    xapi_SetIcon(g_handle, (char *)"/system/icon/unknowexe.png");
    SetMsgPrcor(g_handle, run_message);

    draw_run_dialog();
    g_input_id = xapi_PutTextInputBox(g_handle, 70, 112, 420, (char *)"/apps/system/");

    while (g_running)
    {
        xapi_Sleep(16);
    }
    return 0;
}

extern "C" int elfrun_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int elfrun_main_cpp(int argc, char *argv[], char *envp[])
{
    return elfrun_main_impl(argc, argv, envp);
}
