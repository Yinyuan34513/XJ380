#include "../include/include.h"
#include "../include/efi.h"
#include "./build_settings.h"

#define NULL        0
const FrameBufferConfig *fbc_addr;

static inline __attribute__((always_inline)) char *u64tostrb16(char *buf, size_t len, UINT64 n) {
  char *s = buf + len;
  *--s    = '\0';
  if (n == 0) return (*--s = '0', s);
  for (; n; n >>= 4)
    *--s = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"[n & 15];
  return s;
}

extern "C" void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, BOOT_CONFIG *BootConfig) {
  fbc_addr = &fbc;
  disable_intr();
  init_gdt();
  init_idt();
  hpet_init(BootConfig->HPET);
  init_apic(BootConfig->MADT);
  //rect(fbc, 0, 0, fbc.horizontal_resolution, fbc.vertical_resolution, {0x00, 0xa2, 0xe8});
  //enable_intr();
  
  //unsigned long long spp_status;
  //struct EFI_SIMPLE_POINTER_STATE spp_state;

  //char                 str[3]; // 打印用的字符串
  //struct EFI_INPUT_KEY key;    // 用于接收按键

  EFI_TIME time;
  EFI_TIME_CAPABILITIES time_capab;
  
  init_MemoryManager(fbc, BootConfig);

  UINT16 desktop_sheet_id = 1145; // 如果SheetTest那行输出的是1145的话那么绝对有问题
  UINT16 dock_sheet_id = 1145;
  UINT16 mouse_sheet_id = 1145;
  SHEET_INFO          shtinf;
  init_sheet(fbc, &shtinf);

  enable_intr();

  int mouse_x = 128;
  int mouse_y = 128;

  *shtinf.sheet[0].number = 1145;
  create_sheet(&shtinf, 0, 0, fbc.horizontal_resolution, fbc.vertical_resolution, FixedSheetType, 0, &desktop_sheet_id);
  create_sheet(&shtinf, 0, 0, fbc.horizontal_resolution, fbc.vertical_resolution, FixedSheetType, 254, &dock_sheet_id);
  create_sheet(&shtinf, 128, 128, 10, 21, FixedSheetType, 255, &mouse_sheet_id);
  //delete_sheet(&shtinf, &desktop_sheet_id); // Test

  //draw_point(&shtinf, &desktop_sheet_id, 0, 0, {255, 255, 255, 255});
  //draw_rect(&shtinf, &desktop_sheet_id, 0, 0, 100, 100,{255, 255, 255, 255});

  init_desktop(&shtinf, &desktop_sheet_id, &dock_sheet_id);

  //SPP.Reset(&SPP, 0);
  int fontx = 8;
  int fonty = 50;

  // DEBUG
  if (BUILD_EDITION == DEBUG_VERSION) {
    PrintString(&shtinf, &desktop_sheet_id, fbc.horizontal_resolution - 64, fbc.vertical_resolution - 40, "X:", WHITE);
    PrintString(&shtinf, &desktop_sheet_id, fbc.horizontal_resolution - 64, fbc.vertical_resolution - 24, "Y:", WHITE);
    PrintDec(&shtinf, &desktop_sheet_id, fbc.horizontal_resolution - 48, fbc.vertical_resolution - 40, shtinf.scrx, WHITE);
    PrintDec(&shtinf, &desktop_sheet_id, fbc.horizontal_resolution - 48, fbc.vertical_resolution - 24, shtinf.scry, WHITE);
    PrintString(&shtinf, &desktop_sheet_id, 8, 34, "KeyboardInputOut", WHITE);
    //PrintString(&shtinf, &desktop_sheet_id, 332, 34, "PointerInputOut", WHITE);
    PrintString(&shtinf, &desktop_sheet_id, 8, 480, "MapKey:", WHITE);
    PrintHex(&shtinf, &desktop_sheet_id, 8, 496, BootConfig->MemoryMap.MapKey, WHITE);
    // 别问为什么这么多-40，问就是因为改起来太麻烦
    PrintString(&shtinf, &desktop_sheet_id, 8, 284, "PrintTestColor", WHITE);
    draw_rect(&shtinf, &desktop_sheet_id, 480 - 472, 300, 500 - 472, 320, WHITE);
    draw_rect(&shtinf, &desktop_sheet_id, 500 - 472, 300, 520 - 472, 320, BLACK);
    draw_rect(&shtinf, &desktop_sheet_id, 520 - 472, 300, 540 - 472, 320, XINGJI_BLUE);
    draw_rect(&shtinf, &desktop_sheet_id, 540 - 472, 300, 560 - 472, 320, XINGJI_YELLOW);
    draw_rect(&shtinf, &desktop_sheet_id, 560 - 472, 300, 580 - 472, 320, WIN_BLUE);
    draw_rect(&shtinf, &desktop_sheet_id, 580 - 472, 300, 600 - 472, 320, GRAY);
    draw_rect(&shtinf, &desktop_sheet_id, 600 - 472, 300, 620 - 472, 320, BGRAY);
    draw_rect(&shtinf, &desktop_sheet_id, 620 - 472, 300, 640 - 472, 320, PURPLE);
    draw_rect(&shtinf, &desktop_sheet_id, 640 - 472, 300, 660 - 472, 320, PINK);
    draw_rect(&shtinf, &desktop_sheet_id, 660 - 472, 300, 680 - 472, 320, RED);
    draw_rect(&shtinf, &desktop_sheet_id, 680 - 472, 300, 700 - 472, 320, CHINA_RED);
    draw_rect(&shtinf, &desktop_sheet_id, 700 - 472, 300, 720 - 472, 320, GOLD);
    draw_rect(&shtinf, &desktop_sheet_id, 720 - 472, 300, 740 - 472, 320, GREEN);
    draw_rect(&shtinf, &desktop_sheet_id, 740 - 472, 300, 760 - 472, 320, LIGHT_GREEN);
    draw_rect(&shtinf, &desktop_sheet_id, 760 - 472, 300, 780 - 472, 320, BLUE);
    draw_rect(&shtinf, &desktop_sheet_id, 780 - 472, 300, 800 - 472, 320, SKY_BLUE);

    PrintString(&shtinf, &desktop_sheet_id, 8, 332, "MemoryMapTypeSheet", WHITE);
    PrintString(&shtinf, &desktop_sheet_id, 8, 348, "0x0: Free", WHITE);
    PrintString(&shtinf, &desktop_sheet_id, 8, 364, "0xB: EFI Code & Data", WHITE);

    PrintString(&shtinf, &desktop_sheet_id, 8, 400, "SheetManagerTest", WHITE);
    PrintDec(&shtinf, &desktop_sheet_id, 8, 416, (UINT64)desktop_sheet_id, WHITE);
    PrintDec(&shtinf, &desktop_sheet_id, 164, 416, (UINT64)*shtinf.sheet[0].number, WHITE);
    
    PrintHex(&shtinf, &desktop_sheet_id, 8, 432, (UINT64)shtinf.temp_buffer, WHITE);
    PrintDec(&shtinf, &desktop_sheet_id, 164, 432, (UINT64)shtinf.sheet_num, WHITE);

    PrintHex(&shtinf, &desktop_sheet_id, 8, 448, 0xFEDCBA9876543210, WHITE);
    PrintHex(&shtinf, &desktop_sheet_id, 164, 448, (UINT64)shtinf.sheet[255].buffer, WHITE);

    // CPU型号
    char CPU_Model[48];
    get_cpu_name(CPU_Model);
    PrintString(&shtinf, &desktop_sheet_id, 8, 528, "CPU Model:", WHITE);
    PrintString(&shtinf, &desktop_sheet_id, 96, 528, CPU_Model, WHITE);

    draw_rect(&shtinf, &desktop_sheet_id, 440, fbc.vertical_resolution - 70, 480, fbc.vertical_resolution - 20, {0x00, 0xa2, 0xe8, 0xff});
  }
  //wsod_debug();

  draw_mouse(&shtinf, &mouse_sheet_id);

  refresh_sheet(&shtinf);   // 刷新

  write_serial_string("Operating System Initialize Success.\n");
  while (1) {
    /*
    spp_status = SPP.GetState(&SPP, &spp_state);
    if (!SystemTable.ConIn->ReadKeyStroke(SystemTable.ConIn, &key)) {
      if (key.UnicodeChar != L'\r') { // 是字符
        str[0] = key.UnicodeChar;     // 第一个字符就输出
        str[1] = '\0';                // 第二个字符是\0，结束符
        PrintString(&shtinf, &desktop_sheet_id, fontx, fonty, str, WHITE);
        refresh_part_sheet(&shtinf, fontx, fonty, fontx + 8, fonty + 16);   // 刷新
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
      if (spp_state.LeftButton == 1) {
        int tx1 = shtinf.scrx * 0.1875;
        if (mouse_x > tx1 && mouse_x < tx1 + 16 && 
                mouse_y > 4 && mouse_y < 20) {
          SystemTable.RuntimeServices->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL);
        }
      }
      int tbx = shtinf.sheet[mouse_sheet_id].bx;
      int tby = shtinf.sheet[mouse_sheet_id].by;
      int sppx = spp_state.RelativeMovementX;
      int sppy = spp_state.RelativeMovementY;
      sppx /= 0x2000;
      sppy /= 0x2000;
      mouse_x += sppx;
      mouse_y += sppy;
      
      if (mouse_x < 0) { mouse_x = 0; }
      if (mouse_y < 0) { mouse_y = 0; }
      if (mouse_x > shtinf.scrx - 1) { mouse_x = shtinf.scrx - 1; }
      if (mouse_y > shtinf.scry - 1) { mouse_y = shtinf.scry - 1; }

      move_mouse(&shtinf, &mouse_sheet_id, mouse_x, mouse_y);
      SPP.Reset(&SPP, 0);
      refresh_part_sheet(&shtinf, tbx, tby, 
                                tbx + shtinf.sheet[mouse_sheet_id].width, 
                                tby + shtinf.sheet[mouse_sheet_id].height);
      refresh_part_sheet(&shtinf, shtinf.sheet[mouse_sheet_id].bx, shtinf.sheet[mouse_sheet_id].by, 
                                shtinf.sheet[mouse_sheet_id].bx + shtinf.sheet[mouse_sheet_id].width, 
                                shtinf.sheet[mouse_sheet_id].by + shtinf.sheet[mouse_sheet_id].height);
    }
    */
    SystemTable.RuntimeServices->GetTime(&time, &time_capab);
    int scdx = fbc.horizontal_resolution * 0.8125;
    draw_rect(&shtinf, &dock_sheet_id, scdx - 44, 0, scdx - 4, 24, DOCK_COL);
    PrintDec(&shtinf, &dock_sheet_id, scdx - 44, 4, (time.Hour + 8) % 24, BLACK);
    PrintString(&shtinf, &dock_sheet_id, scdx - 28, 4, ":", BLACK);
    PrintDec(&shtinf, &dock_sheet_id, scdx - 20, 4, time.Minute, BLACK);
    refresh_part_sheet(&shtinf, scdx - 44, 0, scdx, 24);
  }
  // 按理来说不可能到这儿
}
