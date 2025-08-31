#include <stdint.h>
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
#include <krlibc.h>
#include <mm/frame.h>
#include <pci/pci.h>
#include <proto.hpp>
#include <ps2/keyboard.h>
#include <ps2/mouse.h>
#include <rtc.h>
#include <syscall/syscall.h>
#include <task/pcb.h>
#include <elf.h>
#include <errno.h>

void get_thread_name_from_filepath(char *path, char *name)
{
    char *p = path;
    char *q = name;
    while (*p != '\0')
        p++;
    while (*p != '\\' && *p != '/' && *p != '|')
        p--;
    p++;
    while (*p != '\0' && *p != '.')
    {
        *q = *p;
        p++; q++;
    }
    *q = '\0';
}

uint64_t parse_elf_file(char *path)
{
    Elf64_Ehdr ehdr;
    Elf64_Phdr *phdrs = nullptr;
    uint64_t pos = 0;
    int retval = 0;
    uint8_t *buf = nullptr;

    vfs_node_t file;
    file = vfs_open(path);
    uint64_t file_size = file->size;

    // 读取整个文件
    buf = (uint8_t *)page_alloc_random(get_current_directory(), file_size, PTE_PRESENT | PTE_USER);
    if (!buf)
    {
        write_serial_fmt("[Warning] Parse ELF/EPF file failed.\n");
        write_serial_fmt("Reason:   Cannot Alloc Memory.\n");
        return NULL;
    }
    memset(buf, 0, file_size);

    if (vfs_read(file, buf, 0, file->size) == -1)
    {
        write_serial_fmt("[Warning] Parse ELF/EPF file failed.\n");
        write_serial_fmt("Reason:   Cannot Read File.\n");
        return NULL;
    }

    // 解析ELF/EPF头
    ehdr = *(Elf64_Ehdr *)buf;

    // 验证ELF/EPF魔数 (0x7F,'E','L','F') (0x24, 'E', 'P', 'F')
    if ((*(uint32_t *)ehdr.e_ident != 0x464C457F)       // ELF
         && (*(uint32_t *)ehdr.e_ident != 0x46504524))  // EPF
    {
        free(buf);
        write_serial_fmt("[Warning] Parse ELF/EPF file failed.\n");
        write_serial_fmt("Reason:   Bad Format.\n");
        return NULL;
    }

    // 设置程序入口点
    return ehdr.e_entry;
}

void create_user_thread_from_file(char *path, pcb_t pcb)
{
    char thread_name[50];
    void *entry = (void *)parse_elf_file(path);
    if (entry == NULL)
        return;
    get_thread_name_from_filepath(path, thread_name);
    write_serial_fmt("Creating User Thread. Thread Name: %s\n", thread_name);
    create_user_thread(entry, NULL, thread_name, pcb);
}