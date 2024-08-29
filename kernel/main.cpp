// main.cpp
extern "C" {
#include "../include/efi.h"
}
#include "../include/include.h"

struct EFI_LOADED_IMAGE_PROTOCOL       *LIP;

void GetMMP(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, MEMORY_MAP *MemoryMap);

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

  // 初始化GetMemoryMap传入参数结构体（include\global\memory.h > struct MEMORY_MAP）
  MEMORY_MAP MemoryMap;
  MemoryMap.MapSize = 4096;
  MemoryMap.Buffer = NULL;
  MemoryMap.MapKey = 0;
  MemoryMap.DescriptorSize = 0;
  MemoryMap.DescriptorVersion = 0;

  GetMMP(fbc, SystemTable, &MemoryMap);

  init_desktop(fbc);

  SPP.Reset(&SPP, 0);
  int fontx = 100;
  int fonty = 100;

  WriteString(fbc, 300, 284, "PointerInputOut", WHITE);
  WriteHex(fbc, 500, 500, MemoryMap.MapKey, WHITE);

  init_MemoryManager(fbc, MemoryMap);

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
      if(fontx>500){
        fontx = 100;
        fonty += 16;
      }
    }
    if (!spp_status) {
      // out
      rect(fbc, 300, 300, 820, 348, WIN_BLUE);

      WriteHex(fbc, 300, 300, spp_state.RelativeMovementX, WHITE);
      WriteHex(fbc, 300, 316, spp_state.RelativeMovementY, WHITE);
      WriteHex(fbc, 300, 332, spp_state.RelativeMovementZ, WHITE);
    }
  }
  // 按理来说不可能到这儿
}

void GetMMP(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, MEMORY_MAP *MemoryMap) {
  EFI_STATUS GetMemoryMapStatus = EFI_SUCCESS;
  GetMemoryMapStatus = SystemTable.BootServices->AllocatePool(LIP ? LIP->ImageDataType : EfiLoaderData,
   MemoryMap->MapSize, &MemoryMap->Buffer); // 开辟内存空间存放内存表，有bug
  if(EFI_ERROR(GetMemoryMapStatus)) {
    // 无法开辟内存空间
    WSOD_Cannot_Alloc_MemoryMap(fbc);
    while (1) { __asm__("hlt"); } // 寄
  }

  // 获取内存表
  GetMemoryMapStatus = SystemTable.BootServices->GetMemoryMap(
    &MemoryMap->MapSize,
    (EFI_MEMORY_DESCRIPTOR*)MemoryMap->Buffer,
    &MemoryMap->MapKey,
    &MemoryMap->DescriptorSize,
    &MemoryMap->DescriptorVersion
  );
}
