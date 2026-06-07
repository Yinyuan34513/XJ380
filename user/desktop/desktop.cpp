#include <x3api.h>
#include <krlibc.h>

static const char *DEFAULT_DESKTOP_BACKGROUND = "/system/resources/image/background2.png";
static const char *FALLBACK_DESKTOP_BACKGROUND = "/system/resources/image/background3.png";

static HDLE   g_desktop_handle = 0;
static UINT64 g_screen_width    = 0;
static UINT64 g_screen_height   = 0;
static bool   g_needs_redraw    = false;

static DirNode g_entries[256];
static UINT32  g_entry_count = 0;

static const int ICON_CELL_W = 110;
static const int ICON_CELL_H = 110;
static const int ICON_SIZE   = 74;

struct DesktopSettings
{
    char BackgroundFilePath[256];
    int  ClockHourOffset;
};

static void copy_cstr(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0) return;
    memset(dst, 0, dst_size);
    if (src == NULL) return;
    strncpy(dst, src, dst_size - 1);
}

static void path_join(char *out, size_t out_size, const char *base, const char *name)
{
    if (out == NULL || out_size == 0) return;
    memset(out, 0, out_size);
    if (base == NULL) return;
    strncpy(out, base, out_size - 1);
    size_t used = strlen(out);
    if (used + 1 < out_size && used > 0 && out[used - 1] != '/')
    {
        out[used++] = '/';
        out[used] = '\0';
    }
    if (name != NULL && used + 1 < out_size)
    {
        size_t copy_len = strlen(name);
        if (copy_len > out_size - used - 1) copy_len = out_size - used - 1;
        memcpy(out + used, name, copy_len);
        out[used + copy_len] = '\0';
    }
}

static void set_default_desktop_settings(DesktopSettings *settings)
{
    if (settings == NULL) return;
    memset(settings, 0, sizeof(DesktopSettings));
    strcpy(settings->BackgroundFilePath, DEFAULT_DESKTOP_BACKGROUND);
    settings->ClockHourOffset = 8;
}

static void load_desktop_settings(DesktopSettings *settings)
{
    if (settings == NULL) return;
    set_default_desktop_settings(settings);

    UserInfo user;
    memset(&user, 0, sizeof(user));
    xapi_GetCurrentUser(&user);
    if (user.name[0] == '\0') return;

    char path[256];
    memset(path, 0, sizeof(path));
    snprintf(path, sizeof(path), "/users/%s/settings.dat", user.name);

    XFILE *file = xapi_OpenFile(path);
    if (file == NULL || file->buffer == NULL)
    {
        if (file != NULL) xapi_CloseFile(file);
        return;
    }

    DesktopSettings loaded;
    memset(&loaded, 0, sizeof(loaded));
    size_t copy_size = file->length < sizeof(loaded) ? (size_t)file->length : sizeof(loaded);
    if (copy_size > 0) memcpy(&loaded, file->buffer, copy_size);
    xapi_CloseFile(file);

    loaded.BackgroundFilePath[sizeof(loaded.BackgroundFilePath) - 1] = '\0';
    if (loaded.BackgroundFilePath[0] != '\0')
    {
        copy_cstr(settings->BackgroundFilePath, sizeof(settings->BackgroundFilePath), loaded.BackgroundFilePath);
    }
    if (copy_size == sizeof(loaded)) settings->ClockHourOffset = loaded.ClockHourOffset;
}

static bool draw_background_image(const char *path)
{
    if (path == NULL || path[0] == '\0') return false;

    UINT32 image_width = 0;
    UINT32 image_height = 0;
    xapi_GetPicSize(&image_width, &image_height, (char *)path);
    if (image_width == 0 || image_height == 0) return false;

    xapi_DrawPicture(g_desktop_handle, 0, 0, (UINT32)g_screen_width, (UINT32)g_screen_height, (char *)path);
    return true;
}

static void draw_background(const char *preferred_path)
{
    if (draw_background_image(preferred_path)) return;
    if (preferred_path == NULL || strcmp(preferred_path, DEFAULT_DESKTOP_BACKGROUND) != 0)
    {
        if (draw_background_image(DEFAULT_DESKTOP_BACKGROUND)) return;
    }
    if (draw_background_image(FALLBACK_DESKTOP_BACKGROUND)) return;

    xapi_DrawRect(g_desktop_handle, 0, 0, (UINT32)g_screen_width - 1, (UINT32)g_screen_height - 1, 0x0f4c9aff, true);
}

static int desktop_rows()
{
    int rows = ((int)g_screen_height - 88) / ICON_CELL_H;
    return rows > 0 ? rows : 1;
}

static int icon_x(int index)
{
    return 20 + ICON_CELL_W * (index / desktop_rows());
}

static int icon_y(int index)
{
    int rows = desktop_rows();
    return 30 + (index - (index / rows) * rows) * ICON_CELL_H;
}

static int icon_image_x(int index)
{
    return icon_x(index) + (ICON_CELL_W - ICON_SIZE) / 2;
}

static void draw_centered_icon_text(const char *text, int index)
{
    int text_width = (int)xapi_CalcTextWidth((char *)text, 11);
    int x = icon_x(index) + ICON_CELL_W / 2 - text_width / 2;
    int y = icon_y(index) + ICON_SIZE + 1;
    xapi_DrawText(g_desktop_handle, x, y + 1, (char *)text, 11, 0x000000ff);
    xapi_DrawText(g_desktop_handle, x, y, (char *)text, 11, 0xffffffff);
}

static void draw_desktop_icon(const char *label, const char *icon_path, int index)
{
    int x = icon_image_x(index);
    int y = icon_y(index);
    xapi_DrawPicture(g_desktop_handle, x, y, ICON_SIZE, ICON_SIZE, (char *)icon_path);
    draw_centered_icon_text(label, index);
}

static void current_desktop_path(char *path, size_t path_size)
{
    UserInfo user;
    memset(&user, 0, sizeof(user));
    xapi_GetCurrentUser(&user);
    if (user.name[0] == '\0')
    {
        copy_cstr(path, path_size, "/users/Root/desktop");
        return;
    }

    memset(path, 0, path_size);
    snprintf(path, path_size, "/users/%s/desktop", user.name);
}

static void draw_desktop_icons()
{
    char desktop_path[256];
    current_desktop_path(desktop_path, sizeof(desktop_path));
    xapi_Mkdir(desktop_path);

    draw_desktop_icon("系统", "/system/icon/d_system.png", 0);
    draw_desktop_icon("垃圾桶", "/system/icon/bin.png", 1);

    memset(g_entries, 0, sizeof(g_entries));
    g_entry_count = 0;
    xapi_SearchFile(desktop_path, &g_entry_count, g_entries);
    if (g_entry_count == 404)
    {
        g_entry_count = 0;
        return;
    }
    if (g_entry_count > 255) g_entry_count = 255;

    int index = 2;
    for (UINT32 i = 0; i < g_entry_count; i++)
    {
        const char *icon_path = g_entries[i].filetype == 1 ? "/system/icon/folder.png" : "/system/icon/texter.png";
        draw_desktop_icon(g_entries[i].filename, icon_path, index);
        index++;
    }
}

static void redraw_desktop()
{
    DesktopSettings settings;
    load_desktop_settings(&settings);
    draw_background(settings.BackgroundFilePath);
    draw_desktop_icons();
    xapi_RefreshWindow(g_desktop_handle);
}

static void run_with_one_arg(const char *path, const char *arg)
{
    char *argv[2];
    argv[0] = (char *)arg;
    argv[1] = NULL;
    xapi_RunArgs((char *)path, argv);
}

static void open_desktop_file(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (dot != NULL && (strcmp(dot, ".elf") == 0 || strcmp(dot, ".epf") == 0))
    {
        xapi_Run((char *)path);
        return;
    }

    if (dot != NULL)
    {
        if (strcmp(dot, ".png") == 0 || strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0 ||
            strcmp(dot, ".bmp") == 0)
        {
            run_with_one_arg("/apps/builtin/picturer.elf", path);
            return;
        }
        if (strcmp(dot, ".txt") == 0 || strcmp(dot, ".inf") == 0 || strcmp(dot, ".xtb") == 0)
        {
            run_with_one_arg("/apps/builtin/texter.elf", path);
            return;
        }
    }

    run_with_one_arg("/apps/builtin/texter.elf", path);
}

static void open_desktop_icon_at(int x, int y)
{
    char desktop_path[256];
    current_desktop_path(desktop_path, sizeof(desktop_path));

    for (int i = 0; i < 2; i++)
    {
        int bx = icon_x(i);
        int by = icon_y(i);
        if (x > bx && x < bx + ICON_CELL_W && y > by && y < by + ICON_CELL_H)
        {
            if (i == 0) xapi_Run("/apps/system/fmanager.elf");
            return;
        }
    }

    for (UINT32 i = 0; i < g_entry_count; i++)
    {
        int index = (int)i + 2;
        int bx = icon_x(index);
        int by = icon_y(index);
        if (x > bx && x < bx + ICON_CELL_W && y > by && y < by + ICON_CELL_H)
        {
            char run_path[512];
            path_join(run_path, sizeof(run_path), desktop_path, g_entries[i].filename);
            if (g_entries[i].filetype == 1) run_with_one_arg("/apps/system/fmanager.elf", run_path);
            else open_desktop_file(run_path);
            return;
        }
    }
}

static void desktop_MessagePrcor(UINT64 Type, UINT64 hData, UINT64 lData)
{
    if (Type == MSG_LBUTTON)
    {
        int x = (int)hData;
        int y = (int)lData;
        if (y >= 24) open_desktop_icon_at(x, y);
    }
    else if (Type == MSG_RBUTTON || Type == MSG_CRL)
    {
        g_needs_redraw = true;
    }
}

static int desktop_main_impl(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    XWINDOW window;
    memset(&window, 0, sizeof(window));
    window.title = (char *)"桌面";
    window.sets = XWIN_DESKTOP;
    xapi_CreateWindow(&g_desktop_handle, &window);
    if (g_desktop_handle == 0) return 1;

    xapi_GetWindowSize(g_desktop_handle, &g_screen_width, &g_screen_height);
    SetMsgPrcor(g_desktop_handle, desktop_MessagePrcor);
    redraw_desktop();

    while (1)
    {
        if (g_needs_redraw)
        {
            g_needs_redraw = false;
            redraw_desktop();
        }
        xapi_Sleep(200);
    }

    return 0;
}

extern "C" int desktop_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int desktop_main_cpp(int argc, char *argv[], char *envp[])
{
    return desktop_main_impl(argc, argv, envp);
}
