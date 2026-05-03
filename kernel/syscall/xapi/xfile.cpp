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
#include <mm/uaccess.h>
#include <pci/pci.h>
#include <pctable/gdt.h>
#include <pctable/idt.h>
#include <proto.hpp>
#include <rtc.h>
#include <syscall/pxapi.h>
#include <syscall/syscall.h>
#include <task/pcb.h>
#include <ttf.h>

static constexpr size_t XFILE_IO_BOUNCE_BYTES = 0x10000UL;

static ssize_t xfile_write_from_user(vfs_node_t node, const void *buffer, size_t offset, size_t size)
{
    if (node == NULL) return -EIO;
    if (size == 0) return 0;

    page_directory_t *pagedir = get_current_task()->parent_group->pagedir;
    size_t bounce_bytes = MIN(size, XFILE_IO_BOUNCE_BYTES);
    uint8_t *bounce = (uint8_t *)malloc(bounce_bytes);
    if (bounce == NULL) return -ENOMEM;

    size_t total = 0;
    while (total < size)
    {
        size_t chunk = MIN(size - total, bounce_bytes);
        if (!copy_from_user_pagedir(pagedir, bounce, (const uint8_t *)buffer + total, chunk))
        {
            free(bounce);
            return total > 0 ? (ssize_t)total : -EFAULT;
        }

        size_t wrote = vfs_write(node, bounce, offset + total, chunk);
        if (wrote == (size_t)VFS_STATUS_FAILED)
        {
            free(bounce);
            return total > 0 ? (ssize_t)total : -EIO;
        }

        total += wrote;
        if (wrote < chunk) break;
    }

    free(bounce);
    return (ssize_t)total;
}


uint64_t do_xapi_OpenFile(uint64_t path)
{
    char  *fpath = vfs_cwd_path_build((char *)path);
    if (fpath == NULL) return 0;

    page_directory_t *pagedir = get_current_task()->parent_group->pagedir;
    uint64_t fsptr_addr = page_alloc_random(pagedir, sizeof(XFILE), PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    if ((int64_t)fsptr_addr == -1)
    {
        free(fpath);
        return 0;
    }

    XFILE local;
    memset(&local, 0, sizeof(local));
    strcpy(local.path, fpath);

    vfs_node_t f = vfs_open(fpath);
    if (!f)
    {
        free(fpath);
        unmap_page_range(pagedir, fsptr_addr, sizeof(XFILE));
        return 0;
    }
    uint64_t length = f->size;
    local.length    = length;
    if (length > 0)
    {
        uint64_t buffer_addr = page_alloc_random(pagedir, length, PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
        if ((int64_t)buffer_addr == -1)
        {
            vfs_close(f);
            free(fpath);
            unmap_page_range(pagedir, fsptr_addr, sizeof(XFILE));
            return 0;
        }
        local.buffer = (void *)buffer_addr;

        size_t bounce_bytes = MIN(length, XFILE_IO_BOUNCE_BYTES);
        uint8_t *bounce = (uint8_t *)malloc(bounce_bytes);
        if (bounce == NULL)
        {
            vfs_close(f);
            free(fpath);
            unmap_page_range(pagedir, buffer_addr, length);
            unmap_page_range(pagedir, fsptr_addr, sizeof(XFILE));
            return 0;
        }

        size_t total = 0;
        while (total < length)
        {
            size_t chunk = MIN(length - total, bounce_bytes);
            ssize_t read = vfs_read(f, bounce, total, chunk);
            if (read == -1)
            {
                free(bounce);
                vfs_close(f);
                free(fpath);
                unmap_page_range(pagedir, buffer_addr, length);
                unmap_page_range(pagedir, fsptr_addr, sizeof(XFILE));
                return 0;
            }
            if (read == 0) break;

            if (!copy_to_user_pagedir(pagedir, (uint8_t *)local.buffer + total, bounce, (size_t)read))
            {
                free(bounce);
                vfs_close(f);
                free(fpath);
                unmap_page_range(pagedir, buffer_addr, length);
                unmap_page_range(pagedir, fsptr_addr, sizeof(XFILE));
                return 0;
            }
            total += (size_t)read;
        }
        free(bounce);
    }
    vfs_close(f);
    free(fpath);

    if (!copy_to_user_pagedir(pagedir, (void *)fsptr_addr, &local, sizeof(local)))
    {
        if (local.length > 0 && local.buffer != NULL)
        {
            unmap_page_range(pagedir, (uint64_t)local.buffer, local.length);
        }
        unmap_page_range(pagedir, fsptr_addr, sizeof(XFILE));
        return 0;
    }

    return fsptr_addr;
}

void do_xapi_CloseFile(uint64_t ptr)
{
    if (ptr == 0) return;

    page_directory_t *pagedir = get_current_task()->parent_group->pagedir;
    XFILE *fsptr = (XFILE *)ptr;
    XFILE local;
    memset(&local, 0, sizeof(local));
    if (!copy_from_user_pagedir(pagedir, &local, fsptr, sizeof(local)))
    {
        return;
    }

    char *fpath = vfs_cwd_path_build(local.path);
    if (fpath == NULL)
    {
        return;
    }

    vfs_node_t f = vfs_open(fpath);
    if (!f)
    {
        free(fpath);
        goto unmap_only;
    }

    if (local.length > 0)
    {
        ssize_t ret = xfile_write_from_user(f, local.buffer, 0, local.length);
        if (ret >= 0) vfs_update(f);
    }
    free(fpath);
    vfs_close(f);

unmap_only:
    if (local.length > 0 && local.buffer != NULL && user_range_mapped(pagedir, local.buffer, local.length))
    {
        unmap_page_range(pagedir, (uint64_t)local.buffer, local.length);
    }
    unmap_page_range(pagedir, (uint64_t)fsptr, sizeof(XFILE));
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

    vfs_child_lock();
    for (list_t node = v->child; node != NULL; node = node->next)
    {
        if (*fcount >= 255)
        {
            (*fcount)++;
            vfs_child_unlock();
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
    vfs_child_unlock();
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

    ssize_t ret = xfile_write_from_user(node, (void *)regs->rsi, regs->r9, regs->rdx);
    vfs_close(node);
    free(fpath);
    return ret;
}
