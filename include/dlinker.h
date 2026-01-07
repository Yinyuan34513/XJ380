#pragma once

#define MAX_KERNEL_MODULE 256

#include "krlibc.h"
#include <proto.hpp>

typedef struct {
    bool     is_use;
    char     module_name[64];
    char    *path;
    uint8_t *data;
    size_t   size;
    char     raw_name[64];
} cp_module_t;

void module_setup();
cp_module_t *get_module(const char *module_name);
cp_module_t *get_module_raw(const char *module_name);

#define EXPORT_SYMBOL(name)                                                                        \
    __attribute__((used, section(".ksymtab"))) static const dlfunc_t __ksym_##name = {             \
        #name, (void *)name}

#define EXPORT_SYMBOL_F(func_name, name)                                                           \
    __attribute__((used, section(".ksymtab"))) static const dlfunc_t __ksym_##name = {             \
        #func_name, (void *)name}

typedef int (*dlinit_t)(void);

typedef struct {
    char *name;
    void *addr;
} dlfunc_t;

typedef struct kernel_mode {
    cp_module_t *module;
    dlinit_t     entry;
    dlinit_t     task_entry;
    int          entry_exit_code;
} kernel_mode_t;

/**
 * 加载一个内核模块
 * @param kmod 内核模块管理单元
 * @param module 文件句柄
 */
void dlinker_load(kernel_mode_t *kmod, cp_module_t *module);

dlfunc_t *find_func(const char *name);

void find_kernel_symbol();

void dlinker_init();

void load_all_kernel_module();

void start_all_kernel_module();

void module_setup();
