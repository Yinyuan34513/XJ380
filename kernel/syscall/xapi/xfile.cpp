#include <ahci/ahci.h>
#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>
#include <font.h>
#include <fs/fatfs/fatfs.h>
#include <fs/fs_syscall.h>
#include <fs/partition.h>
#include <fs/vfs/dev.h>
#include <fs/vfs/sys.h>
#include <fs/vfs/vfs.h>
#include <global_color.h>
#include <graphics/GOP.hpp>
#include <graphics/sheet.h>
#include <graphics/window/window.h>
#include <krlibc.h>
#include <mm/frame.h>
#include <pci/pci.h>
#include <proto.hpp>
#include <rtc.h>
#include <syscall/pxapi.h>
#include <task/pcb.h>
#include <ttf.h>

void do_xapi_SearchFile(uint64_t path, uint64_t count, uint64_t dir)
{
    char *fpath = (char *)path;
    uint32_t *fcount = (uint32_t *)count;
    XAPIT_DirNode *fdir = (XAPIT_DirNode *)dir;

    vfs_node_t v = vfs_open(fpath);
    if (!v)
    {
        *fcount = 404;
        return;
    }

    for (list_t node = v->child; node != NULL; node = node->next)
    {
        if (*fcount >= 255)
        {
            (*fcount)++;
            break;
        }
        void *data = node->data;
        strcpy(fdir[*fcount].filename, ((vfs_node_t)data)->name);
        fdir[*fcount].length = ((vfs_node_t)data)->size;
        fdir[*fcount].filetpye = (((vfs_node_t)data)->type & (file_symlink | file_dir)) == (file_symlink | file_dir) ? 1 : 0;
        (*fcount)++;
    }
}
