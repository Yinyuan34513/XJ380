#include "../include/include.h"
#include "../include/efi.h"
#include "./build_settings.h"

#define NULL        0

static inline __attribute__((always_inline)) char *u64tostrb16(char *buf, size_t len, uint64_t n) {
  char *s = buf + len;
  *--s    = '\0';
  if (n == 0) return (*--s = '0', s);
  for (; n; n >>= 4)
    *--s = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"[n & 15];
  return s;
}

extern "C" void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, EFI_SIMPLE_POINTER_PROTOCOL &SPP, 
                              BOOT_CONFIG *BootConfig, EFI_BATTERY_CHARGING_PROTOCOL &battery) {
  unsigned long long spp_status;
  struct EFI_SIMPLE_POINTER_STATE spp_state;

  char                 str[3]; // 打印用的字符串
  struct EFI_INPUT_KEY key;    // 用于接收按键

  EFI_TIME time;
  EFI_TIME_CAPABILITIES time_capab;

  init_desktop(fbc);
  
  init_MemoryManager(fbc, BootConfig);

  SHEET_INFO shtinf;
  UINT16 desktop_sheet_id = 1145; // 如果SheetTest那行输出的是1145的话那么绝对有问题
  init_sheet(fbc, &shtinf);

  *shtinf.sheet[0].number = 1145;
  create_sheet(&shtinf, 0, 0, fbc.horizontal_resolution, fbc.vertical_resolution, FixedSheetType, 0, &desktop_sheet_id);
  //delete_sheet(&shtinf, &desktop_sheet_id); // Test

  //draw_point(shtinf, &desktop_sheet_id, 0, 0, {0, 0, 0, 255});
  //refresh_sheet(shtinf);  // debug，运行之后如果桌面和快捷栏等init_desktop的内容没了就代表成功

  SPP.Reset(&SPP, 0);
  int fontx = 8;
  int fonty = 50;

  // DEBUG
  if (BUILD_EDITION == DEBUG_VERSION) {
    WriteString(fbc, fbc.horizontal_resolution - 64, fbc.vertical_resolution - 40, "X:", WHITE);
    WriteString(fbc, fbc.horizontal_resolution - 64, fbc.vertical_resolution - 24, "Y:", WHITE);
    WriteDec(fbc, fbc.horizontal_resolution - 48, fbc.vertical_resolution - 40, shtinf.scrx, WHITE);
    WriteDec(fbc, fbc.horizontal_resolution - 48, fbc.vertical_resolution - 24, shtinf.scry, WHITE);
    WriteString(fbc, 8, 34, "KeyboardInputOut", WHITE);
    WriteString(fbc, 332, 34, "PointerInputOut", WHITE);
    WriteString(fbc, 316, 480, "MapKey:", WHITE);
    WriteHex(fbc, 316, 496, BootConfig->MemoryMap.MapKey, WHITE);
    // 别问为什么这么多-40，问就是因为改起来太麻烦
    WriteString(fbc, 440, 284, "PrintTestColor", WHITE);
    rect(fbc, 480 - 40, 300, 500 - 40, 320, WHITE);
    rect(fbc, 500 - 40, 300, 520 - 40, 320, BLACK);
    rect(fbc, 520 - 40, 300, 540 - 40, 320, XINGJI_BLUE);
    rect(fbc, 540 - 40, 300, 560 - 40, 320, XINGJI_YELLOW);
    rect(fbc, 560 - 40, 300, 580 - 40, 320, WIN_BLUE);
    rect(fbc, 580 - 40, 300, 600 - 40, 320, GRAY);
    rect(fbc, 600 - 40, 300, 620 - 40, 320, BGRAY);
    rect(fbc, 620 - 40, 300, 640 - 40, 320, PURPLE);
    rect(fbc, 640 - 40, 300, 660 - 40, 320, PINK);
    rect(fbc, 660 - 40, 300, 680 - 40, 320, RED);
    rect(fbc, 680 - 40, 300, 700 - 40, 320, CHINA_RED);
    rect(fbc, 700 - 40, 300, 720 - 40, 320, GOLD);
    rect(fbc, 720 - 40, 300, 740 - 40, 320, GREEN);
    rect(fbc, 740 - 40, 300, 760 - 40, 320, LIGHT_GREEN);
    rect(fbc, 760 - 40, 300, 780 - 40, 320, BLUE);
    rect(fbc, 780 - 40, 300, 800 - 40, 320, SKY_BLUE);

    WriteString(fbc, 440, 332, "MemoryMapTypeSheet", WHITE);
    WriteString(fbc, 440, 348, "0x0: Free", WHITE);
    WriteString(fbc, 440, 364, "0xB: EFI Code & Data", WHITE);

    WriteString(fbc, 440, 400, "SheetManagerTest", WHITE);
    WriteDec(fbc, 440, 416, (UINT64)desktop_sheet_id, WHITE);
    WriteDec(fbc, 580, 416, (UINT64)*shtinf.sheet[0].number, WHITE);
    
    WriteHex(fbc, 440, 432, (UINT64)shtinf.temp_buffer, WHITE);
  }

  while (1) {
    spp_status = SPP.GetState(&SPP, &spp_state);
    if (!SystemTable.ConIn->ReadKeyStroke(SystemTable.ConIn, &key)) {
      if (key.UnicodeChar != L'\r') { // 是字符
        str[0] = key.UnicodeChar;     // 第一个字符就输出
        str[1] = '\0';                // 第二个字符是\0，结束符
        WriteString(fbc, fontx, fonty, str, WHITE);
        fontx += 8;
      } else if (key.UnicodeChar == L'\r') { // 是换行
        fontx = 8;
        fonty += 16;
      } else {                        // 是不可打印字符
        // pass
        fontx += 8;
      }
      if(fontx > 304){
        fontx = 8;
        fonty += 16;
      }
    }
    if (!spp_status) {
      // out
      rect(fbc, 316, 50, 600, 82, WIN_BLUE);

      WriteHex(fbc, 332, 50, spp_state.RelativeMovementX, WHITE);
      WriteHex(fbc, 332, 66, spp_state.RelativeMovementY, WHITE);
      WriteHex(fbc, 332, 82, spp_state.RelativeMovementZ, WHITE);
    }
    SystemTable.RuntimeServices->GetTime(&time, &time_capab);
    int scdx = fbc.horizontal_resolution * 0.8125;
    rect(fbc, scdx - 44, 0, scdx - 4, 24, GRAY);
    WriteDec(fbc, scdx - 44, 4, (time.Hour + 8) % 24, BLACK);
    WriteAscii(fbc, scdx - 28, 4, ':', BLACK);
    WriteDec(fbc, scdx - 20, 4, time.Minute, BLACK);
  }
  // 按理来说不可能到这儿
}
