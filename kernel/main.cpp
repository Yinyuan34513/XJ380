// main.cpp
extern "C" {
#include "../include/efi.h"
}
#include "../include/include.h"

static inline __attribute__((always_inline)) char *u64tostrb16(char *buf, size_t len, uint64_t n) {
  char *s = buf + len;
  *--s    = '\0';
  if (n == 0) return (*--s = '0', s);
  for (; n; n >>= 4)
    *--s = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"[n & 15];
  return s;
}

extern "C" void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable) {
  char                 str[3]; // 打印用的字符串
  struct EFI_INPUT_KEY key;    // 用于接收按键

  init_desktop(fbc);
  bmp(fbc,&SystemTable, (CHAR16*)"\\EFI\\BOOT\\test.bmp", 200, 200);
  int fontx = 100;
  int fonty = 100;
  while (1) {
    WriteString(fbc, fontx+6, fonty,"|", {255, 255, 255});
    if (!SystemTable.ConIn->ReadKeyStroke(SystemTable.ConIn, &key)) {
      if (key.UnicodeChar == 0x08) //bakespace
      {
        rect(fbc,fontx+7,fonty,fontx+10,fonty+16,{0x13, 0x6d, 0xff});
      	fontx -= 8;
        if(fontx < 100) {
        	fontx = 492;
        	fonty -= 16;
		}
        rect(fbc,fontx+8,fonty,fontx+16,fonty+16,{0x13, 0x6d, 0xff});
        str[0] = '\0';
      }
      else if (key.UnicodeChar != L'\r') { // 不是换行
        rect(fbc,fontx+7,fonty,fontx+10,fonty+16,{0x13, 0x6d, 0xff});
        str[0] = key.UnicodeChar;     // 第一个字符就输出
        str[1] = '\0';
		fontx += 8;
		if(fontx > 500) {
			fontx = 108;
			fonty += 16;
		}
      }
	  else {                        // 是换行或不可打印字符
        rect(fbc,fontx+7,fonty,fontx+10,fonty+16,{0x13, 0x6d, 0xff});
        fontx = 100;
        fonty += 16;
        str[0] = '\0';
        str[1] = '\0';                // 第二个字符是\0，结束符
        if(fontx>500){
          fontx = 100;
          fonty += 16;
    	}
      }
      WriteString(fbc, fontx, fonty, str, {255, 255, 255});
    }
  }
  // 按理来说不可能到这儿
}
