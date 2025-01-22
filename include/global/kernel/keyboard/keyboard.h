#include "../../../include.h"
#define KB_BUF_SIZE 100

#define PORT_KB_DATA 0x60
#define PORT_KB_STATUS 0x64
#define PORT_KB_CMD 0x64

#define KBCMD_WRITE_CMD 0x60
#define KBCMD_READ_CMD 0x20

#define KB_INIT_MODE 0x47

#define KBSTATUS_IBF 0x02
#define KBSTATUS_OBF 0x01

#define wait_KB_write() while (inb(PORT_KB_STATUS) & KBSTATUS_IBF)
#define wait_KB_read() while (inb(PORT_KB_STATUS) & KBSTATUS_OBF)

struct keyboard_buf {
    UINT8 * p_head;
    UINT8 * p_tail;
    INT32 count;
    UINT8 buf[KB_BUF_SIZE];
};