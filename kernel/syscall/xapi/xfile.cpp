#include <ahci/ahci.h>
#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>
#include <font.h>
#include <fs/fatfs/fatfs.h>
#include <fs/partition.h>
#include <fs/vfs/devfs.h>
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
#include <pctable/gdt.h>
#include <pctable/idt.h>
#include <syscall/syscall.h>
#include <syscall/pxapi.h>
#include <task/pcb.h>
#include <ttf.h>
#include <errno.h>

uint64_t do_xapi_OpenFile(uint64_t path)
{
    char    *fpath = (char *)path;
    XFILE   *fsptr = (XFILE *)malloc(sizeof(XFILE));

    vfs_node_t f = vfs_open(fpath);
    uint64_t length = f->size;
    fsptr->buffer = malloc(length);
    
    vfs_read(f, fsptr->buffer, 0, length);

    return (uint64_t)fsptr;
}

void do_xapi_CloseFile(uint64_t ptr)
{
    XFILE *fsptr = (XFILE *)ptr;
    free(fsptr->buffer);
    free(fsptr);
}

void do_xapi_SearchFile(uint64_t path, uint64_t count, uint64_t dir)
{
    char          *fpath  = (char *)path;
    uint32_t      *fcount = (uint32_t *)count;
    XAPIT_DirNode *fdir   = (XAPIT_DirNode *)dir;

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
        if (strcmp(((vfs_node_t)data)->name,"")!=0)
        {
            strcpy(fdir[*fcount].filename, ((vfs_node_t)data)->name);
            fdir[*fcount].length   = ((vfs_node_t)data)->size;
            fdir[*fcount].filetype = ((vfs_node_t)data)->type == file_dir ? 1 : 0;
            (*fcount)++;
        }
    }
}

uint64_t do_xapi_ReadFile(struct X64_REGS *regs)
{
    vfs_node_t node = vfs_open((const char *)regs->rdi);
    if (!node)
    {
        regs->rax = (uint64_t)-ENOENT;
        return 0;
    }

    if (node->type & file_dir)
    {
        vfs_close(node);
        regs->rax = (uint64_t)-EISDIR;
        return 0;
    }

    ssize_t ret = vfs_read(node, (void *)regs->rsi, regs->r9, regs->rdx);
    vfs_close(node);
    return ret;
}

uint64_t do_xapi_WriteFile(struct X64_REGS *regs)
{
    vfs_node_t node = vfs_open((const char *)regs->rdi);
    if (!node)
    {
        // 文件不存在，创建它
        int create_ret = vfs_mkfile((const char *)regs->rdi);
        if (create_ret < 0)
        {
            regs->rax = (uint64_t)-ENOENT;
            return 0;
        }
        node = vfs_open((const char *)regs->rdi);
        if (!node)
        {
            regs->rax = (uint64_t)-ENOENT;
            return 0;
        }
    }

    if (node->type & file_dir)
    {
        vfs_close(node);
        regs->rax = (uint64_t)-EISDIR;
        return 0;
    }

    ssize_t ret = vfs_write(node, (void *)regs->rsi, regs->r9, regs->rdx);
    vfs_close(node);
    return ret;
}
