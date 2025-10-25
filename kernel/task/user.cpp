#include <ahci/ahci.h>
#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>
#include <elf.h>
#include <errno.h>
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
#include <stdint.h>
#include <syscall/syscall.h>
#include <task/pcb.h>
#include <user/user.h>
#include "../build_settings.h"

UserInfo *current_user = NULL;

char *current_user_envp[100] = {
    ENVP_SYSTEM_VERSION,
    NULL    // 必须以NULL结尾
};

void init_user()
{
    current_user = (UserInfo *)malloc(sizeof(UserInfo));
    // 这里先偷个懒，之后再改
    memcpy(current_user->name, "GuoqiFish", strlen("GuoqiFish"));
    current_user->user_type = XUT_Root;
    current_user->envc = 2;
    current_user->envp = current_user_envp;
    sys_mkdir("/Users", NULL);
    sys_mkdir(strcat("/Users/", current_user->name), NULL);
}

void copy_args(char *dst[], char *src[], int n)
{
    for (int i = 0; i < n; i++)
    {
        if (src[i] != NULL)
        {
            dst[i] = (char *)malloc(strlen(src[i]) * sizeof(char));
            strcpy(dst[i], src[i]);
        }
    }
}

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
        p++;
        q++;
    }
    *q = '\0';
}

uint64_t parse_elf_file(char *path, pcb_t group)
{
    Elf64_Ehdr  ehdr;
    Elf64_Phdr *phdrs  = nullptr;
    uint64_t    pos    = 0;
    int         retval = 0;
    uint8_t    *buf    = nullptr;

    vfs_node_t file;
    file               = vfs_open(path);
    uint64_t file_size = file->size;

    if (group == NULL)
    {
        write_serial_fmt("[Warning] Parse ELF/EPF file failed.\n");
        write_serial_fmt("Reason:   ? Where is your parent group?\n");
        return NULL;
    }

    // 读取整个文件
    buf = (uint8_t *)phys_to_virt(alloc_frames((file_size + PAGE_SIZE - 1) / PAGE_SIZE));
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
    if ((*(uint32_t *)ehdr.e_ident != 0x464C457F)     // ELF
        && (*(uint32_t *)ehdr.e_ident != 0x46504524)) // EPF
    {
        free_frames((uint64_t)virt_to_phys((uint64_t)buf), (file_size + PAGE_SIZE - 1) / PAGE_SIZE);
        write_serial_fmt("[Warning] Parse ELF/EPF file failed.\n");
        write_serial_fmt("Reason:   Bad Format.\n");
        return NULL;
    }

    uint64_t entry = ehdr.e_entry;

    elf_mapping_space *mapping_space = (elf_mapping_space *)malloc(sizeof(elf_mapping_space));
    CalcLoadAddressRange((Elf64_Ehdr *)buf, (uint64_t *)&mapping_space->load_start,
                         (uint64_t *)&mapping_space->load_end);

    page_map_range_to_random(group->pagedir, mapping_space->load_start,
                             mapping_space->load_end - mapping_space->load_start,
                             PTE_PRESENT | PTE_WRITEABLE | PTE_USER);

    switch_page_directory(group->pagedir);

    memset((void *)mapping_space->load_start, 0, mapping_space->load_end - mapping_space->load_start);

    // 获取程序头表
    phdrs = (Elf64_Phdr *)(buf + ehdr.e_phoff);

    // 呃，目前看起来好像没什么用？
    uint64_t start_code, end_code;
    uint64_t start_data, end_data;
    uint64_t start_rodata, end_rodata;
    uint64_t start_brk, end_brk;
    uint64_t start_stack;

    // 遍历并加载程序段
    for (int i = 0; i < ehdr.e_phnum; i++)
    {
        if (phdrs[i].p_type != PT_LOAD) { continue; }

        // 对齐到页大小
        uint64_t v_addr = phdrs[i].p_vaddr & ~(PAGE_SIZE - 1);
        uint64_t v_size = (phdrs[i].p_memsz + (phdrs[i].p_vaddr & (PAGE_SIZE - 1)) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        // 更新内存映射信息
        if (phdrs[i].p_flags & 0x1)
        { // 可执行
            start_code = min(start_code, v_addr);
            end_code   = max(end_code, v_addr + v_size);
        }
        else if (phdrs[i].p_flags & 0x2)
        { // 可写
            start_data = min(start_data, v_addr);
            end_data   = max(end_data, v_addr + v_size);
        }
        else
        {
            start_rodata = min(start_rodata, v_addr);
            end_rodata   = max(end_rodata, v_addr + v_size);
        }

        // 复制段内容
        uint64_t segm_in_file = (uint64_t)buf + phdrs[i].p_offset;

        memcpy((void *)phdrs[i].p_vaddr, (void *)segm_in_file, phdrs[i].p_filesz);

        // 清零未初始化数据
        if (phdrs[i].p_memsz > phdrs[i].p_filesz)
        {
            memset((void *)(phdrs[i].p_vaddr + phdrs[i].p_filesz), 0, phdrs[i].p_memsz - phdrs[i].p_filesz);
        }
    }

    uint64_t buf_len = (file_size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));
    uint8_t *buffer = (uint8_t *)EHDR_START_ADDR;
    page_map_range_to_random(get_current_directory(), EHDR_START_ADDR, buf_len,
                             PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    memcpy(buffer, buf, file_size);

    group->load_start = mapping_space->load_start;
    group->elf_file = buf;
    group->elf_size = file_size;

    free_frames((uint64_t)virt_to_phys((uint64_t)buf), (file_size + PAGE_SIZE - 1) / PAGE_SIZE);

    // 设置程序入口点
    return entry;
}

void getFileDirectory(const char* path, char* result) {
    // 查找最后一个路径分隔符的位置
    int lastSlash = -1;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/' || path[i] == '\\') {
            lastSlash = i;
        }
    }
    
    // 根据不同的情况处理
    if (lastSlash == -1) {
        // 没有分隔符，返回空字符串
        result[0] = '\0';
    } else if (lastSlash == 0) {
        // 只有根目录，如 "/file"
        result[0] = '/';
        result[1] = '\0';
    } else {
        // 正常路径，复制目录部分
        for (int i = 0; i < lastSlash; i++) {
            result[i] = path[i];
        }
        result[lastSlash] = '\0';
    }
}

char *getCwd(char* path)
{
    char result[strlen(path)+1];
    getFileDirectory(path,result);
    return result;
}

void create_user_process_from_file(char *path, pcb_t pcb)
{
    char  process_name[50];
    void *entry;
    if (entry == NULL) return;

    get_thread_name_from_filepath(path, process_name);

    write_serial_fmt("Creating User Process. Process Name: %s\n", process_name);
    page_directory_t *new_directory = clone_page_directory(get_current_directory(), true);
    pcb_t             group         = create_process_group(process_name, pcb, new_directory, (char *)"");

    entry = (void *)parse_elf_file(path, group);

    write_serial_fmt("Creating User Thread. Thread Name: %s\n", process_name);
    char cmdline[PAGE_SIZE];
    memset(cmdline, 0, sizeof(cmdline));
    char *cmdline_ptr = cmdline;
    char *argv[50];
    memset((void*)argv,0,sizeof(argv));
    argv[0] = "argv0";

    for (int i = 0; argv[i]; i++) {
        int len      = sprintf(cmdline_ptr, "%s ", argv[i]);
        cmdline_ptr += len;
    }

    char *old_cmdline = group->cmdline;
    group->cmdline  = strdup(cmdline);

    create_user_thread(entry, NULL, 1, process_name, group, getCwd(path));
}

void create_user_thread_from_file(char *path, pcb_t pcb)
{
    char  thread_name[50];
    void *entry = (void *)parse_elf_file(path, pcb);
    if (entry == NULL) return;
    get_thread_name_from_filepath(path, thread_name);
    write_serial_fmt("Creating User Thread. Thread Name: %s\n", thread_name);
    create_user_thread(entry, NULL, NULL, thread_name, pcb, getCwd(path));
}

