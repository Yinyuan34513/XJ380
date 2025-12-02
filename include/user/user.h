#pragma once

#include <stdint.h>

typedef enum
{
    XUT_Root,
    XUT_System,
    XUT_Admin,
    XUT_Visitor,
    XUT_Custom
} UserType;

typedef struct
{
    bool ReadFile;       // 读取普通文件
    bool WriteFile;      // 写入普通文件
    bool ChangeSettings; // 更改系统设置
    bool VisitSysDir;    // 访问系统目录
    bool ReadSysFile;    // 读取系统文件
    bool WriteSysFile;   // 写入系统文件
} UserPermisson;

typedef struct
{
    char            name[64];   // 用户名
    UserType        user_type;  // 用户等级
    UserPermisson   user_prms;  // 用户权限（仅等级为Custom时可用）
    char   **envp;              // 环境变量指针
    size_t   envc;              // 环境变量数量
} UserInfo;

void init_user();
void copy_args(char *dst[], char *src[], int n);
