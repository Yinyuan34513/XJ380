#pragma once

typedef unsigned char      xj380_inst_u8;
typedef unsigned short     xj380_inst_u16;
typedef unsigned int       xj380_inst_u32;
typedef unsigned long long xj380_inst_u64;
typedef long long          xj380_inst_i64;

#define XJ380_INSTALLER_MAX_DISKS      32
#define XJ380_INSTALLER_DISK_NAME_LEN  48
#define XJ380_INSTALLER_STAGE_LEN      96
#define XJ380_INSTALLER_DETAIL_LEN     192
#define XJ380_INSTALLER_BOOT_MAGIC     0x584A33383049534FULL
#define XJ380_INSTALLER_PAK_MAGIC      "XJPAK10"
#define XJ380_INSTALLER_PAK_MAGIC_SIZE 8

#define XJ380_PAK_ENTRY_FILE 1
#define XJ380_PAK_ENTRY_DIR  2

typedef struct {
    char           magic[XJ380_INSTALLER_PAK_MAGIC_SIZE];
    xj380_inst_u64 entry_count;
} xj380_pak_header;

typedef struct {
    xj380_inst_u16 path_len;
    xj380_inst_u16 type;
    xj380_inst_u32 mode;
    xj380_inst_u64 size;
} xj380_pak_entry_header;

typedef struct {
    xj380_inst_u32 id;
    xj380_inst_u32 sector_size;
    xj380_inst_u64 size_bytes;
    xj380_inst_u64 flags;
    char           name[XJ380_INSTALLER_DISK_NAME_LEN];
} xj380_installer_disk;

typedef struct {
    xj380_inst_u32 count;
    xj380_inst_u32 reserved;
    xj380_installer_disk disks[XJ380_INSTALLER_MAX_DISKS];
} xj380_installer_disk_list;

typedef enum {
    XJ380_INSTALLER_IDLE = 0,
    XJ380_INSTALLER_RUNNING,
    XJ380_INSTALLER_DONE,
    XJ380_INSTALLER_FAILED,
} xj380_installer_state;

typedef struct {
    xj380_inst_u32 state;
    xj380_inst_u32 percent;
    xj380_inst_i64 result;
    char           stage[XJ380_INSTALLER_STAGE_LEN];
    char           detail[XJ380_INSTALLER_DETAIL_LEN];
} xj380_installer_progress;
