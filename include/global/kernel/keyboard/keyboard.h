#ifndef _KEBOARD_H_
#define _KEBOARD_H_

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

enum special_key_code {
	KEY_ESC = 128,
	KEY_BACKSPACE,
	KEY_TAB,
	KEY_ENTER,
	KEY_CAPS,
	KEY_SHIFT,
	KEY_CTRL,
	KEY_ALT,
	KEY_SPACE,
	KEY_F1,
	KEY_F2,
	KEY_F3,
	KEY_F4,
	KEY_F5,
	KEY_F6,
	KEY_F7,
	KEY_F8,
	KEY_F9,
	KEY_F10,
	KEY_NUML,
	KEY_SCROLL,
	KEY_F11,
	KEY_F12
};

#define wait_KB_write() while (inb(PORT_KB_STATUS) & KBSTATUS_IBF)
#define wait_KB_read() while (inb(PORT_KB_STATUS) & KBSTATUS_OBF)

struct keyboard_buf {
    UINT8  *p_head;
    UINT8  *p_tail;
    INT32  	count;
	bool	ctrl;
	bool 	shift;
	bool	alt;
	bool 	caps;
    UINT8  	buf[KB_BUF_SIZE];
};
struct mouse_packet
{	
	unsigned char Byte0;	//7:Y overflow,6:X overflow,5:Y sign bit,4:X sign bit,3:Always,2:Middle Btn,1:Right Btn,0:Left Btn
	char Byte1;	//X movement
	char Byte2;	//Y movement
};
#define KB_SEND2MOUSE	0xd4
#define MOUSE_EN		0xf4

#define KB_EN_MOUSE_INTFACE	0xa8

#endif
