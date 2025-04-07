#pragma once

#include "../../../include.h"

#define DEVICE_NR 64 // 设备数量
#define NAMELEN 16

// 设备类型
enum device_type_t
{
  DEV_NULL,  // 空设备
  DEV_CHAR,  // 字符设备
  DEV_BLOCK, // 块设备
  DEV_NET,   // 网络设备
};

// 设备子类型
enum device_subtype_t
{
  DEV_CONSOLE = 1, // 控制台
  DEV_KEYBOARD,    // 键盘
  DEV_SERIAL,      // 串口
  DEV_TTY,         // TTY 设备
  DEV_DISK,        // IDE 磁盘
  DEV_PART,        // IDE 磁盘分区
  DEV_RAMDISK,     // 虚拟磁盘
  DEV_NETIF,       // 网卡
};

// 设备控制命令
enum device_cmd_t
{
  DEV_CMD_SECTOR_START = 1, // 获得设备扇区开始位置 lba
  DEV_CMD_SECTOR_COUNT,     // 获得设备扇区数量
  DEV_CMD_SECTOR_SIZE,      // 获得设备扇区大小
};

#define REQ_READ 0  // 块设备读
#define REQ_WRITE 1 // 块设备写

#define DIRECT_UP 0   // 上楼
#define DIRECT_DOWN 1 // 下楼

typedef int (*ioctl_t)(void *dev, int cmd, void *args, int flags);
typedef int (*read_t)(void *dev, void *buf, size_t count, int idx, int flags);
typedef int (*write_t)(void *dev, void *buf, size_t count, int idx, int flags);

typedef struct device_t
{
  char name[NAMELEN]; // 设备名
  int type;           // 设备类型
  int subtype;        // 设备子类型
  int dev;            // 设备号
  int parent;         // 父设备号
  void *ptr;          // 设备指针
  bool direct;        // 磁盘寻道方向

  // 设备控制
  ioctl_t ioctl;
  // 读设备
  read_t read;
  // 写设备
  write_t write;
} device_t;

// 安装设备
int device_install(
    int type, int subtype,
    void *ptr, char *name, int parent,
    void *ioctl, void *read, void *write);

// 根据子类型查找设备
device_t *device_find(int type, int idx);

// 根据设备号查找设备
device_t *device_get(int dev);

// 控制设备
int device_ioctl(int dev, int cmd, void *args, int flags);

// 读设备
int device_read(int dev, void *buf, size_t count, int idx, int flags);

// 写设备
int device_write(int dev, void *buf, size_t count, int idx, int flags);

void init_device();

static inline char *LeadingWhitespace(char *beg, char *end)
{
  while (end > beg && *--end <= 0x20)
  {
    *end = 0;
  }
  while (beg < end && *beg <= 0x20)
  {
    beg++;
  }
  return beg;
}
