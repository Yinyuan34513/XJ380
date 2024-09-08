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

extern "C" void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, EFI_SIMPLE_POINTER_PROTOCOL &SPP, BOOT_CONFIG *BootConfig) {
  unsigned long long spp_status;
  struct EFI_SIMPLE_POINTER_STATE spp_state;

  char                 str[3]; // 打印用的字符串
  struct EFI_INPUT_KEY key;    // 用于接收按键

  init_desktop(fbc);

  SPP.Reset(&SPP, 0);
  int fontx = 100;
  int fonty = 100;

  WriteString(fbc, 300, 284, "PointerInputOut", WHITE);
  WriteHex(fbc, 500, 500, BootConfig->MemoryMap.MapKey, WHITE);
  
  init_MemoryManager(fbc, BootConfig);

  while (1) {
    spp_status = SPP.GetState(&SPP, &spp_state); // 如果这玩意前面有WriteString或者在循环里就会在下面键盘那行卡住
    if (!SystemTable.ConIn->ReadKeyStroke(SystemTable.ConIn, &key)) {
      if (key.UnicodeChar != L'\r') { // 是字符
        str[0] = key.UnicodeChar;     // 第一个字符就输出
        str[1] = '\0';                // 第二个字符是\0，结束符
        WriteString(fbc, fontx, fonty, str, WHITE);
        fontx += 8;
      } else if (key.UnicodeChar == L'\r') { // 是换行
        fontx = 100;
        fonty += 16;
      } else {                        // 是不可打印字符
        // pass
        fontx += 8;
      }
      if(fontx > 500){
        fontx = 100;
        fonty += 16;
      }
    }
    if (!spp_status) {
      // out
      rect(fbc, 300, 300, 820, 348, WIN_BLUE);

      WriteHex(fbc, 500, 300, spp_state.RelativeMovementX, WHITE);
      WriteHex(fbc, 500, 316, spp_state.RelativeMovementY, WHITE);
      WriteHex(fbc, 500, 332, spp_state.RelativeMovementZ, WHITE);
    }
  }
  // 按理来说不可能到这儿
}
