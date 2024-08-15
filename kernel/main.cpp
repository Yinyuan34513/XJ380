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

extern "C" void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, EFI_SIMPLE_POINTER_PROTOCOL &SPP) {
  unsigned long long spp_status;
  struct EFI_SIMPLE_POINTER_STATE spp_state;

  char                 str[3]; // 打印用的字符串
  struct EFI_INPUT_KEY key;    // 用于接收按键

  init_desktop(fbc);

  int fontx = 100;
  int fonty = 100;
  while (1) {
    if (!SystemTable.ConIn->ReadKeyStroke(SystemTable.ConIn, &key)) {
      if (key.UnicodeChar != L'\r') { // 不是换行
        str[0] = key.UnicodeChar;     // 第一个字符就输出
        str[1] = '\0';                // 第二个字符是\0，结束符
      } else {                        // 是换行或不可打印字符
        str[0] = '\r';                // \r
        str[1] = '\n'; // 给EFI擦屁股，\r\n才是一个换行，而enter只能接收到\r
        str[2] = '\0'; // 结束符
      }
      WriteString(fbc, fontx, fonty, str, {255, 255, 255});
      fontx += 8;
      if(fontx>500){
        fontx = 100;
        fonty += 16;
      }
    }
  }
  // 按理来说不可能到这儿
}
