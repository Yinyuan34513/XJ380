#include <ahci/ahci.h>
#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>
#include <errno.h>
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
#include <pctable/gdt.h>
#include <pctable/idt.h>
#include <proto.hpp>
#include <rtc.h>
#include <syscall/pxapi.h>
#include <syscall/syscall.h>
#include <task/pcb.h>
#include <ttf.h>


uint64_t do_xapi_OpenFile(uint64_t path)
{
    char  *fpath = vfs_cwd_path_build((char *)path);
    XFILE *fsptr = (XFILE *)page_alloc_random(get_current_task()->parent_group->pagedir, sizeof(XFILE),  PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    strcpy(fsptr->path, fpath);

    vfs_node_t f = vfs_open(fpath);
    if (!f)
    {
        free(fpath);
        unmap_page_range(get_current_task()->parent_group->pagedir, (uint64_t)fsptr, sizeof(XFILE));
        return 0;
    }
    uint64_t length = f->size;
    fsptr->buffer   = (void *)page_alloc_random(get_current_task()->parent_group->pagedir, length,  PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    if ((int64_t)fsptr->buffer == -1)
    {
        vfs_close(f);
        free(fpath);
        unmap_page_range(get_current_task()->parent_group->pagedir, (uint64_t)fsptr, sizeof(XFILE));
        return 0;
    }
    vfs_read(f, fsptr->buffer, 0, length);
    vfs_close(f);
    free(fpath);

    return (uint64_t)fsptr;
}

void do_xapi_CloseFile(uint64_t ptr)
{
    XFILE *fsptr = (XFILE *)ptr;
    char  *fpath = vfs_cwd_path_build(fsptr->path);
    vfs_node_t f = vfs_open(fpath);
    if (!fsptr) 
    {
        free(fpath);
        return;
    }
    else if (!fsptr->buffer) 
    {
        free(fpath);
        return;
    }
    if (!f)
    {
        free(fpath);
        return;
    }
    vfs_write(f, fsptr->buffer, 0, fsptr->length);
    unmap_page_range(get_current_task()->parent_group->pagedir, (uint64_t)fsptr->buffer, fsptr->length);
    unmap_page_range(get_current_task()->parent_group->pagedir, (uint64_t)fsptr, sizeof(XFILE));
    free(fpath);
    vfs_close(f);
}

void do_xapi_SearchFile(uint64_t path, uint64_t count, uint64_t dir)
{
    uint32_t      *fcount = (uint32_t *)count;
    XAPIT_DirNode *fdir   = (XAPIT_DirNode *)dir;
    if (fcount == NULL || fdir == NULL || path == 0) return;

    *fcount = 0;

    char *fpath = vfs_cwd_path_build((char *)path);
    if (fpath == NULL)
    {
        *fcount = 404;
        return;
    }

    vfs_node_t v = vfs_open(fpath);
    if (!v)
    {
        *fcount = 404;
        free(fpath);
        return;
    }
    if (!(v->type & file_dir))
    {
        *fcount = 404;
        vfs_close(v);
        free(fpath);
        return;
    }
    v->refcount++;

    for (list_t node = v->child; node != NULL; node = node->next)
    {
        if (*fcount >= 255)
        {
            (*fcount)++;
            vfs_close(v);
            free(fpath);
            return;
        }
        void *data = node->data;
        if (strcmp(((vfs_node_t)data)->name, "") != 0 && ((vfs_node_t)data)->type != file_delete)
        {
            strcpy(fdir[*fcount].filename, ((vfs_node_t)data)->name);
            fdir[*fcount].length   = ((vfs_node_t)data)->size;
            fdir[*fcount].filetype = ((vfs_node_t)data)->type == file_dir ? 1 : 0;
            (*fcount)++;
        }
    }
    vfs_close(v);
    free(fpath);
}

uint64_t do_xapi_ReadFile(struct X64_REGS *regs)
{
    char *fpath  = vfs_cwd_path_build((char *)regs->rdi);
    vfs_node_t node = vfs_open(fpath);
    if (!node)
    {
        regs->rax = (uint64_t)-ENOENT;
        free(fpath);
        return 0;
    }

    if (node->type & file_dir)
    {
        vfs_close(node);
        free(fpath);
        regs->rax = (uint64_t)-EISDIR;
        return 0;
    }

    ssize_t ret = vfs_read(node, (void *)regs->rsi, regs->r9, regs->rdx);
    vfs_close(node);
    free(fpath);
    return ret;
}

uint64_t do_xapi_WriteFile(struct X64_REGS *regs)
{
    char *fpath  = vfs_cwd_path_build((char *)regs->rdi);
    vfs_node_t node = vfs_open(fpath);
    if (!node)
    {
        // 文件不存在，创建它
        int create_ret = vfs_mkfile((const char *)regs->rdi);
        if (create_ret < 0)
        {
            regs->rax = (uint64_t)-ENOENT;
            free(fpath);
            return 0;
        }
        node = vfs_open((const char *)regs->rdi);
        if (!node)
        {
            regs->rax = (uint64_t)-ENOENT;
            free(fpath);
            return 0;
        }
    }

    if (node->type & file_dir)
    {
        vfs_close(node);
        regs->rax = (uint64_t)-EISDIR;
        free(fpath);
        return 0;
    }

    ssize_t ret = vfs_write(node, (void *)regs->rsi, regs->r9, regs->rdx);
    vfs_close(node);
    free(fpath);
    return ret;
}
