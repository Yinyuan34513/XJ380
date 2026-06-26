#include <x3api.h>
#include <krlibc.h>
#include "fm_proto.h"
#include <libsys.h>
#include <xposix/errno.h>
#include <xposix/sys/stat.h>

char current_path[1024] = "/";
char path_his[20][1024];
int path_p = 0;
int path_r = 0;

HDLE handle;
int fmr_width = FMR_X;
int fmr_height = FMR_Y;

static void fm_cancel_inline_rename();

void fmanager_MessagePrcor(UINT64 Type, UINT64 hData, UINT64 lData)
{
    switch (Type)
    {
    case MSG_LBUTTON:
        register_click(hData, lData);
        break;
    case MSG_ROLLER:
        register_scroll_wheel((int)hData);
        break;
    case MSG_RBUTTON:
        fm_select_item_at((int)hData, (int)lData);
        yicixing_lock = true;
        break;
    case MSG_CRL:
        process_crl(hData);
        break;
    case MSG_SPCHAR:
        if ((char)(lData) == '\n') { fm_finish_inline_rename(); }
        break;
    case MSG_KEYDOWN:
        // xapi_OutputSerial("KEY DOWN:");
        // xapi_OutputSerial((char *)&lData);
        // xapi_OutputSerial("\n");
        if (lData == XKEY_ENTER || lData == '\n') { fm_finish_inline_rename(); }
        break;
    // case MSG_KEYUP:
    //     xapi_OutputSerial("KEY UP:");
    //     xapi_OutputSerial((char *)(uint64_t)(&lData));
    //     xapi_OutputSerial("\n");
    //     break;
    case MSG_RESIZE:
        fm_cancel_inline_rename();
        fmr_width = (int)hData;
        fmr_height = (int)lData;
        fm_clamp_scroll_base();
        need_paint = true;
        break;
    }
    return;
}

static int fmanager_main_impl(int argc, char *argv[], char *envp[])
{
    (void)envp;

    if (argc == 1)
    {
        strcpy(current_path, argv[0]);
    }

    XWINDOW Winfo;
    Winfo.title  = (char *)"文件管理器";
    Winfo.width  = FMR_X;
    Winfo.height = FMR_Y;
    Winfo.sets = XWIN_NORMAL | XWIN_SUPPORT_RESIZEABLE;
    xapi_CreateWindow(&handle, &Winfo);
    xapi_SetIcon(handle, "/system/icon/folder.png");
    SetMsgPrcor(handle, fmanager_MessagePrcor);
    memset(file_ti_index, 0, 2048 * sizeof(FILE_TYPEINFO_INDEX));

    paint_dir(current_path);

    // 初始化路径历史
    for (int i = 0; i < 20; i++) {
        memset(path_his[i], 0, 1024);
    }
    strcpy(path_his[0], "/");  // 初始路径为根目录
    path_p = 0;  // 当前位置指针
    path_r = 0;  // 最早位置指针

    RightMenuItem rmi[7];
    rmi[0].text = (char *)"新建文件夹";
    rmi[0].CRLid = FM_CMD_NEW_FOLDER;
    rmi[1].text = (char *)"重命名";
    rmi[1].CRLid = FM_CMD_RENAME;
    rmi[2].text = (char *)"删除";
    rmi[2].CRLid = FM_CMD_DELETE;
    rmi[3].text = (char *)"复制";
    rmi[3].CRLid = FM_CMD_COPY;
    rmi[4].text = (char *)"剪切";
    rmi[4].CRLid = FM_CMD_CUT;
    rmi[5].text = (char *)"粘贴";
    rmi[5].CRLid = FM_CMD_PASTE;
    rmi[6].text = (char *)"属性";
    rmi[6].CRLid = FM_CMD_PROPERTIES;
    xapi_RegisterRightButtonMenu(handle, rmi, 7);

    while (1)
    {
        if (double_click && !register_lock)
        {
            double_click = false;
            need_paint = false;

            int click_index = double_click_file_index;
            double_click_file_index = -1;
            if (file_count <= 0)
            {
                continue;
            }
            if (click_index < 0 || click_index >= file_count)
            {
                continue;
            }
            if (file_dir_cache[click_index].filetype == 1)
            {
                // 是文件夹
                record_current_path();
                cat_path(file_dir_cache[click_index].filename);
                paint_dir(current_path);
            }
            else
            {
                // 是文件
                char file_path[1024];
                memset(file_path, 0, 1024);
                strcat(file_path, current_path);
                strcat(file_path, "/");
                strcat(file_path, file_dir_cache[click_index].filename);
                xapi_Run(file_path);
            }
        }
        if (need_paint && !register_lock)
        {
            fm_cancel_inline_rename();
            paint_dir(current_path);
            need_paint = false;
        }
        xapi_Sleep(1);
    }
}

extern "C" int fmanager_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int fmanager_main_cpp(int argc, char *argv[], char *envp[])
{
    return fmanager_main_impl(argc, argv, envp);
}

static char g_clipboard_path[1024];
static char g_clipboard_name[256];
static bool g_clipboard_is_dir = false;
static bool g_clipboard_cut = false;
static bool g_inline_rename_active = false;
static UINT64 g_inline_rename_box = 0;
static char g_inline_rename_old_name[256];

static bool fm_copy_string(char *out, int out_size, const char *src)
{
    if (out == NULL || out_size <= 0) return false;
    out[0] = '\0';
    if (src == NULL) return false;

    int i = 0;
    while (src[i] != '\0' && i < out_size - 1)
    {
        out[i] = src[i];
        i++;
    }
    out[i] = '\0';
    return src[i] == '\0';
}

static bool fm_append_string(char *out, int out_size, const char *src)
{
    if (out == NULL || src == NULL || out_size <= 0) return false;
    int len = strlen(out);
    if (len >= out_size) return false;

    int i = 0;
    while (src[i] != '\0' && len + i < out_size - 1)
    {
        out[len + i] = src[i];
        i++;
    }
    out[len + i] = '\0';
    return src[i] == '\0';
}

static bool fm_join_path_checked(char *out, int out_size, const char *base, const char *name)
{
    if (out == NULL || out_size <= 0) return false;
    out[0] = '\0';
    if (base == NULL || name == NULL || name[0] == '\0') return false;
    if (!fm_copy_string(out, out_size, base)) return false;
    if (strcmp(out, "/") != 0 && !fm_append_string(out, out_size, "/")) return false;
    return fm_append_string(out, out_size, name);
}

static bool fm_name_is_valid(const char *name)
{
    if (name == NULL || name[0] == '\0') return false;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return false;
    return strchr(name, '/') == NULL && strchr(name, '\\') == NULL;
}

static bool fm_is_path_prefix(const char *path, const char *prefix)
{
    if (path == NULL || prefix == NULL) return false;
    int len = strlen(prefix);
    if (len <= 0) return false;
    return strcmp(path, prefix) == 0 || (strncmp(path, prefix, len) == 0 && path[len] == '/');
}

static bool fm_path_exists(const char *path)
{
    struct stat st;
    memset(&st, 0, sizeof(st));
    return stat(path, &st) == 0;
}

static void fm_cancel_inline_rename()
{
    if (g_inline_rename_box != 0)
    {
        xapi_DeleteTextInputBox(g_inline_rename_box);
    }
    g_inline_rename_box = 0;
    g_inline_rename_old_name[0] = '\0';
    g_inline_rename_active = false;
}

static INT64 fm_create_empty_file(const char *path)
{
    INT64 fd = (INT64)xapi_CreateFile((char *)path);
    if (fd < 0) return fd;
    enter_syscall(SYS_CLOSE, (UINT64)fd, 0, 0, 0, 0, 0);
    return 0;
}

static const char *fm_error_text(INT64 ret)
{
    if (ret == 0) return "操作成功。";
    if (ret < 0) ret = -ret;
    switch (ret)
    {
    case ENOENT: return "路径不存在。";
    case EACCES: return "权限不足。";
    case EPERM: return "权限不足或文件系统拒绝操作。";
    case EISDIR: return "目标是文件夹，不能按文件处理。";
    case ENOTDIR: return "路径中的某一项不是文件夹。";
    case ENOTEMPTY: return "文件夹不是空的。";
    case EEXIST: return "目标已经存在。";
    case ENOSPC: return "磁盘空间不足。";
    case EROFS: return "目标文件系统是只读的。";
    case EINVAL: return "输入的名称或路径无效。";
    case ENAMETOOLONG: return "路径太长。";
    case EIO: return "磁盘读写失败。";
    default: return "文件系统返回了错误。";
    }
}

static bool fm_make_selected_path(char *out, int out_size, DirNode *out_node)
{
    DirNode selected_file;
    if (!fm_get_selected_file(&selected_file))
    {
        fm_show_error("未选择项目", "请先选择一个文件或文件夹。");
        return false;
    }
    if (!fm_join_path_checked(out, out_size, current_path, selected_file.filename))
    {
        fm_show_error("路径错误", "文件路径过长或名称无效。");
        return false;
    }
    if (out_node != NULL) *out_node = selected_file;
    return true;
}

bool fm_selected_path(char *out, int out_size, DirNode *out_node)
{
    return fm_make_selected_path(out, out_size, out_node);
}

static void fm_draw_modal_base(const char *title, int w, int h, int *out_x, int *out_y)
{
    int x = fmr_width / 2 - w / 2;
    int y = fmr_height / 2 - h / 2;
    if (x < 16) x = 16;
    if (y < 40) y = 40;
    xapi_DrawRect(handle, x, y, x + w, y + h, 0xf8fbffff, true);
    xapi_DrawRect(handle, x, y, x + w, y + h, 0x7aa7d9ff, false);
    xapi_DrawRect(handle, x, y, x + w, y + 30, 0x0f5fb8ff, true);
    xapi_DrawText(handle, x + 12, y + 8, (char *)title, 10, 0xffffffff);
    if (out_x != NULL) *out_x = x;
    if (out_y != NULL) *out_y = y;
}

static void fm_wait_modal()
{
    xapi_RefreshWindow(handle);
    xapi_Sleep(1200);
}

void fm_show_error(const char *title, const char *detail)
{
    int x, y;
    fm_draw_modal_base(title == NULL ? "错误" : title, 420, 126, &x, &y);
    xapi_DrawText(handle, x + 18, y + 48, (char *)(detail == NULL ? "操作失败。" : detail), 10, 0x333333ff);
    xapi_DrawText(handle, x + 18, y + 84, "窗口会自动关闭。", 10, 0x777777ff);
    fm_wait_modal();
    need_paint = true;
}

void fm_show_info(const char *title, const char *detail)
{
    int x, y;
    fm_draw_modal_base(title == NULL ? "提示" : title, 420, 126, &x, &y);
    xapi_DrawText(handle, x + 18, y + 48, (char *)(detail == NULL ? "操作完成。" : detail), 10, 0x333333ff);
    xapi_DrawText(handle, x + 18, y + 84, "窗口会自动关闭。", 10, 0x777777ff);
    fm_wait_modal();
    need_paint = true;
}

static void fm_show_progress(const char *title, const char *path, UINT64 done, UINT64 total)
{
    int x, y;
    fm_draw_modal_base(title == NULL ? "正在处理" : title, 500, 140, &x, &y);
    char line[512];
    snprintf(line, sizeof(line), "%s", path == NULL ? "" : path);
    xapi_DrawText(handle, x + 18, y + 48, line, 10, 0x333333ff);
    UINT64 percent = total == 0 ? 0 : (done * 100ULL) / total;
    if (percent > 100) percent = 100;
    xapi_DrawRect(handle, x + 18, y + 82, x + 482, y + 104, 0xe5eef8ff, true);
    xapi_DrawRect(handle, x + 18, y + 82, x + 482, y + 104, 0x9cc7f4ff, false);
    int fill = x + 18 + (int)((464ULL * percent) / 100ULL);
    if (fill > x + 18) xapi_DrawRect(handle, x + 18, y + 82, fill, y + 104, 0x136fdcff, true);
    snprintf(line, sizeof(line), "%llu / %llu KB  %llu%%", done / 1024ULL, total / 1024ULL, percent);
    xapi_DrawText(handle, x + 18, y + 112, line, 10, 0x555555ff);
    xapi_RefreshWindow(handle);
}

static void fm_show_copy_progress(const char *path, UINT64 done, UINT64 total)
{
    fm_show_progress(g_clipboard_cut ? "正在移动" : "正在复制", path, done, total);
}

static bool fm_select_file_by_name(const char *name)
{
    if (name == NULL) return false;
    for (int i = 0; i < file_count; i++)
    {
        if (strcmp(file_dir_cache[i].filename, name) != 0) continue;

        int visible_rows = fm_visible_rows();
        if (i < file_count_base)
        {
            file_count_base = i;
        }
        else if (i >= file_count_base + visible_rows)
        {
            file_count_base = i - visible_rows + 1;
        }
        fm_clamp_scroll_base();
        choosing_index = i - file_count_base;
        return true;
    }
    return false;
}

static bool fm_start_inline_rename(const char *old_name)
{
    if (old_name == NULL || old_name[0] == '\0') return false;

    fm_cancel_inline_rename();

    if (!fm_copy_string(g_inline_rename_old_name, sizeof(g_inline_rename_old_name), old_name))
    {
        fm_show_error("重命名失败", "文件名太长。");
        return false;
    }

    int x = fm_item_name_x() - 2;
    int y = fm_item_name_y(choosing_index) - 1;
    int width = fm_item_name_width();
    g_inline_rename_box = xapi_PutTextInputBox(handle, x, y, width, g_inline_rename_old_name);
    if (g_inline_rename_box == 0)
    {
        g_inline_rename_old_name[0] = '\0';
        fm_show_error("重命名失败", "无法创建输入框。");
        return false;
    }

    g_inline_rename_active = true;
    return true;
}

static void fm_rename_selected()
{
    DirNode selected_file;
    if (!fm_get_selected_file(&selected_file))
    {
        fm_show_error("重命名失败", "请先选择一个文件或文件夹。");
        return;
    }

    fm_start_inline_rename(selected_file.filename);
}

static void fm_new_folder()
{
    char folder_name[256];
    char folder_path[1024];

    for (int i = 0; i < 1000; i++)
    {
        snprintf(folder_name, sizeof(folder_name), "folder%d", i);
        if (!fm_join_path_checked(folder_path, sizeof(folder_path), current_path, folder_name))
        {
            fm_show_error("新建文件夹失败", "路径太长或名称无效。");
            return;
        }
        if (fm_path_exists(folder_path)) continue;

        INT64 ret = (INT64)xapi_Mkdir(folder_path);
        if (ret < 0)
        {
            char detail[256];
            snprintf(detail, sizeof(detail), "%s 错误码：%lld", fm_error_text(ret), (long long)ret);
            fm_show_error("新建文件夹失败", detail);
            return;
        }

        paint_dir(current_path);
        if (fm_select_file_by_name(folder_name))
        {
            paint_dir(current_path);
            fm_start_inline_rename(folder_name);
        }
        return;
    }

    fm_show_error("新建文件夹失败", "可用的默认文件夹名称已用完。");
}

static INT64 fm_copy_file(const char *src, const char *dst, UINT64 *done, UINT64 total)
{
    struct stat st;
    memset(&st, 0, sizeof(st));
    if (stat(src, &st) != 0) return -ENOENT;
    if (st.st_size == 0) return fm_create_empty_file(dst);

    static char buffer[65536];
    UINT64 offset = 0;
    while (offset < (UINT64)st.st_size)
    {
        UINT64 chunk = (UINT64)sizeof(buffer);
        if (chunk > (UINT64)st.st_size - offset) chunk = (UINT64)st.st_size - offset;
        INT64 read_ret = (INT64)xapi_ReadFile((char *)src, buffer, chunk, offset);
        if (read_ret < 0) return read_ret;
        if (read_ret == 0 && chunk != 0) return -EIO;
        INT64 write_ret = (INT64)xapi_WriteFile((char *)dst, buffer, (UINT64)read_ret, offset);
        if (write_ret < 0) return write_ret;
        if (write_ret != read_ret) return -EIO;
        offset += (UINT64)read_ret;
        if (done != NULL)
        {
            *done += (UINT64)read_ret;
            if ((offset & ((1ULL << 20) - 1)) == 0 || offset == (UINT64)st.st_size)
                fm_show_copy_progress(src, *done, total);
        }
    }
    return 0;
}

static INT64 fm_copy_tree(const char *src, const char *dst, UINT64 *done, UINT64 total)
{
    struct stat st;
    memset(&st, 0, sizeof(st));
    if (stat(src, &st) != 0) return -ENOENT;
    if (!S_ISDIR(st.st_mode)) return fm_copy_file(src, dst, done, total);

    INT64 ret = (INT64)xapi_Mkdir((char *)dst);
    if (ret < 0 && ret != -EEXIST) return ret;

    DirNode children[256];
    uint32_t count = 0;
    memset(children, 0, sizeof(children));
    xapi_SearchFile((char *)src, &count, children);
    if (count == 404) return -ENOENT;
    if (count > 255) count = 255;
    for (uint32_t i = 0; i < count; i++)
    {
        char child_src[1024];
        char child_dst[1024];
        if (!fm_join_path_checked(child_src, sizeof(child_src), src, children[i].filename)) return -ENAMETOOLONG;
        if (!fm_join_path_checked(child_dst, sizeof(child_dst), dst, children[i].filename)) return -ENAMETOOLONG;
        ret = fm_copy_tree(child_src, child_dst, done, total);
        if (ret < 0) return ret;
    }
    return 0;
}

static UINT64 fm_tree_size(const char *path)
{
    struct stat st;
    memset(&st, 0, sizeof(st));
    if (stat(path, &st) != 0) return 0;
    if (!S_ISDIR(st.st_mode)) return st.st_size < 0 ? 0 : (UINT64)st.st_size;

    UINT64 total = 0;
    DirNode children[256];
    uint32_t count = 0;
    memset(children, 0, sizeof(children));
    xapi_SearchFile((char *)path, &count, children);
    if (count == 404) return 0;
    if (count > 255) count = 255;
    for (uint32_t i = 0; i < count; i++)
    {
        char child[1024];
        if (!fm_join_path_checked(child, sizeof(child), path, children[i].filename)) return total;
        total += fm_tree_size(child);
    }
    return total;
}

static INT64 fm_delete_tree(const char *path)
{
    struct stat st;
    memset(&st, 0, sizeof(st));
    if (stat(path, &st) != 0) return -ENOENT;
    if (!S_ISDIR(st.st_mode)) return (INT64)xapi_DeleteFile((char *)path);

    DirNode children[256];
    uint32_t count = 0;
    memset(children, 0, sizeof(children));
    xapi_SearchFile((char *)path, &count, children);
    if (count == 404) return -ENOENT;
    if (count > 255) count = 255;
    for (uint32_t i = 0; i < count; i++)
    {
        char child[1024];
        if (!fm_join_path_checked(child, sizeof(child), path, children[i].filename)) return -ENAMETOOLONG;
        INT64 ret = fm_delete_tree(child);
        if (ret < 0) return ret;
    }
    return (INT64)xapi_Rmdir((char *)path);
}

void fm_copy_selected(bool cut)
{
    DirNode selected_file;
    char path[1024];
    if (!fm_make_selected_path(path, sizeof(path), &selected_file)) return;
    if (!fm_copy_string(g_clipboard_path, sizeof(g_clipboard_path), path) ||
        !fm_copy_string(g_clipboard_name, sizeof(g_clipboard_name), selected_file.filename))
    {
        fm_show_error("无法记录剪贴板", "文件路径或名称过长。");
        return;
    }
    g_clipboard_is_dir = selected_file.filetype == 1;
    g_clipboard_cut = cut;
    fm_show_info(cut ? "已剪切" : "已复制", cut ? "已记录剪切项目，请到目标文件夹粘贴。" : "已记录复制项目，请到目标文件夹粘贴。");
}

void fm_paste_clipboard()
{
    if (g_clipboard_path[0] == '\0')
    {
        fm_show_error("无法粘贴", "剪贴板中没有文件或文件夹。");
        return;
    }

    char dst[1024];
    if (!fm_join_path_checked(dst, sizeof(dst), current_path, g_clipboard_name))
    {
        fm_show_error("无法粘贴", "目标路径过长或名称无效。");
        return;
    }
    if (strcmp(g_clipboard_path, dst) == 0)
    {
        fm_show_error("无法粘贴", "源路径和目标路径相同。");
        return;
    }
    if (g_clipboard_is_dir && fm_is_path_prefix(current_path, g_clipboard_path))
    {
        fm_show_error("无法粘贴", "不能把文件夹复制或移动到它自己的内部。");
        return;
    }
    if (fm_path_exists(dst))
    {
        fm_show_error("无法粘贴", "目标位置已经存在同名项目。");
        return;
    }

    INT64 ret = 0;
    if (g_clipboard_cut)
    {
        ret = (INT64)xapi_RenameFile(g_clipboard_path, dst);
        if (ret < 0)
        {
            UINT64 total = fm_tree_size(g_clipboard_path);
            UINT64 done = 0;
            fm_show_progress("正在移动", g_clipboard_path, 0, total);
            ret = fm_copy_tree(g_clipboard_path, dst, &done, total);
            if (ret >= 0) ret = fm_delete_tree(g_clipboard_path);
        }
        if (ret >= 0)
        {
            g_clipboard_path[0] = '\0';
            g_clipboard_name[0] = '\0';
            g_clipboard_cut = false;
        }
    }
    else
    {
        UINT64 total = fm_tree_size(g_clipboard_path);
        UINT64 done = 0;
        fm_show_progress("正在复制", g_clipboard_path, 0, total);
        ret = fm_copy_tree(g_clipboard_path, dst, &done, total);
    }

    if (ret < 0)
    {
        char detail[256];
        snprintf(detail, sizeof(detail), "%s 错误码：%lld", fm_error_text(ret), (long long)ret);
        fm_show_error("粘贴失败", detail);
    }
    else
    {
        need_paint = true;
    }
}

void fm_show_properties()
{
    DirNode selected_file;
    char path[1024];
    if (!fm_make_selected_path(path, sizeof(path), &selected_file)) return;
    struct stat st;
    memset(&st, 0, sizeof(st));
    if (stat(path, &st) != 0)
    {
        fm_show_error("无法读取属性", "目标路径不存在或无法访问。");
        return;
    }

    int x, y;
    fm_draw_modal_base("属性", 520, 246, &x, &y);
    char line[640];
    snprintf(line, sizeof(line), "名称：%s", selected_file.filename);
    xapi_DrawText(handle, x + 18, y + 46, line, 10, 0x333333ff);
    snprintf(line, sizeof(line), "路径：%s", path);
    xapi_DrawText(handle, x + 18, y + 70, line, 10, 0x333333ff);
    snprintf(line, sizeof(line), "类型：%s", S_ISDIR(st.st_mode) ? "文件夹" : "文件");
    xapi_DrawText(handle, x + 18, y + 94, line, 10, 0x333333ff);
    snprintf(line, sizeof(line), "大小：%lld 字节", (long long)st.st_size);
    xapi_DrawText(handle, x + 18, y + 118, line, 10, 0x333333ff);
    snprintf(line, sizeof(line), "权限：0%o", st.st_mode & 0777);
    xapi_DrawText(handle, x + 18, y + 142, line, 10, 0x333333ff);
    snprintf(line, sizeof(line), "修改时间：%lld", (long long)st.st_mtime);
    xapi_DrawText(handle, x + 18, y + 166, line, 10, 0x333333ff);
    snprintf(line, sizeof(line), "创建/变更时间：%lld", (long long)st.st_ctime);
    xapi_DrawText(handle, x + 18, y + 190, line, 10, 0x333333ff);
    xapi_DrawText(handle, x + 18, y + 216, "窗口会自动关闭。", 10, 0x777777ff);
    fm_wait_modal();
    need_paint = true;
}

void process_crl(int CRLid)
{
    if (CRLid == FM_CMD_NEW_FOLDER) fm_new_folder();
    else if (CRLid == FM_CMD_RENAME) fm_rename_selected();
    else if (CRLid == FM_CMD_DELETE)
    {
        DirNode selected_file;
        char filepath[1024];
        if (!fm_make_selected_path(filepath, sizeof(filepath), &selected_file)) return;

        INT64 ret = fm_delete_tree(filepath);
        if (ret < 0)
        {
            char detail[256];
            snprintf(detail, sizeof(detail), "%s 错误码：%lld", fm_error_text(ret), (long long)ret);
            fm_show_error("删除失败", detail);
            return;
        }

        paint_dir(current_path);
    }
    else if (CRLid == FM_CMD_COPY) fm_copy_selected(false);
    else if (CRLid == FM_CMD_CUT) fm_copy_selected(true);
    else if (CRLid == FM_CMD_PASTE) fm_paste_clipboard();
    else if (CRLid == FM_CMD_PROPERTIES) fm_show_properties();
}

void fm_finish_inline_rename()
{
    if (!g_inline_rename_active || g_inline_rename_box == 0) return;

    char new_name[XAPI_TEXT_INPUT_BOX_BUFFER_SIZE];
    memset(new_name, 0, sizeof(new_name));
    xapi_GetTextInputBox(g_inline_rename_box, new_name);

    if (!fm_name_is_valid(new_name))
    {
        fm_cancel_inline_rename();
        fm_show_error("重命名失败", "名称不能为空，且不能包含路径分隔符。");
        return;
    }

    if (strcmp(new_name, g_inline_rename_old_name) == 0)
    {
        fm_cancel_inline_rename();
        paint_dir(current_path);
        return;
    }

    char old_path[1024];
    if (!fm_join_path_checked(old_path, sizeof(old_path), current_path, g_inline_rename_old_name))
    {
        fm_cancel_inline_rename();
        fm_show_error("重命名失败", "原路径太长或名称无效。");
        return;
    }

    char new_path[1024];
    if (!fm_join_path_checked(new_path, sizeof(new_path), current_path, new_name))
    {
        fm_cancel_inline_rename();
        fm_show_error("重命名失败", "新路径太长或名称无效。");
        return;
    }

    INT64 ret = (INT64)xapi_RenameFile(old_path, new_path);
    fm_cancel_inline_rename();
    if (ret < 0)
    {
        char detail[256];
        snprintf(detail, sizeof(detail), "%s 错误码：%lld", fm_error_text(ret), (long long)ret);
        fm_show_error("重命名失败", detail);
        return;
    }

    paint_dir(current_path);
}
