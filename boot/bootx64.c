// 版权所有©XINGJI Studios 2017-2024 保留所有权利。
// XJ380UEFI引导程序（\EFI\BOOT\BOOTX64.efi）
#include "../include/cinclude.h"
#include "../include/global/memory.h"
#define NULL 0
struct EFI_SYSTEM_TABLE                *ST;  // 系统表
struct EFI_BOOT_SERVICES               *BS;  // 启动信息
struct EFI_GRAPHICS_OUTPUT_PROTOCOL    *GOP; // GOP
struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SFSP;// 文件
struct EFI_LOADED_IMAGE_PROTOCOL       *LIP; // e?
struct EFI_SIMPLE_POINTER_PROTOCOL     *SPP; // 鼠标
EFI_HANDLE                              IM;

static inline __attribute__((always_inline)) uint16_t *u64tostrb16(uint16_t *buf, size_t len,
                                                                   uint64_t n) {
  uint16_t *s = buf + len;
  *--s        = '\0';
  if (n == 0) return (*--s = '0', s);
  for (; n; n >>= 4)
    *--s = L"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"[n & 15];
  return s;
}

struct EFI_GUID gop_guid = {
    0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}
};
struct EFI_GUID lip_guid = {
    0x5b1b31a1, 0x9562, 0x11d2, {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};
struct EFI_GUID sfsp_guid = {
    0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};
struct EFI_GUID spp_guid = {
    0x31878c87, 0xb75, 0x11d5, {0x9a, 0x4f, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d}
};

void putc(unsigned short c) {
  unsigned short str[2] = L" ";
  str[0]                = c;
  ST->ConOut->OutputString(ST->ConOut, str);
}

void puts(unsigned short *s) {
  ST->ConOut->OutputString(ST->ConOut, s);
}

void puth(unsigned long long hex) {
  unsigned short  buf[20];
  unsigned short *p = buf;
  unsigned short  ch;
  int             i, flag = 0;

  *p++ = L'0';
  *p++ = L'x'; // 先存一个0x

  if (hex == 0)
    *p++ = L'0'; // 如果是0，直接0x0趋势
  else {
    for (i = 28; i >= 0; i -= 4) { // 每次4位
      ch = (hex >> i) & 0xF;       // 0~9, A~F
      // 28（冗余）
      if (flag || ch > 0) { // 跳过前导0
        flag  = 1;          // 没有前导0就把flag设为1，这样后面再有0也不会忽略
        ch   += L'0';       // 0~9 => '0'~'9'
        if (ch > L'9') {
          ch += 7; // 'A' - '9' = 7
        }
        *p++ = ch; // 写入
      }
    }
  }
  *p = L'\0'; // 结束符
  puts(buf);  // 打印
}

void *malloc(int buf_size) {
  void              *res;
  unsigned long long status;

  status = BS->AllocatePool(LIP ? LIP->ImageDataType : EfiLoaderData, buf_size,
                            &res); // 分配内存，第一个参数是判断LIP是否存在，如果存在就直接拿过来用
  if (status < 0) return NULL;

  return res;
}

void free(void *buf) {
  BS->FreePool(buf);
}

EFI_STATUS mallocAt(EFI_PHYSICAL_ADDRESS addr, UINTN size) {
  EFI_STATUS           status;
  EFI_PHYSICAL_ADDRESS allocated_addr;
  status = BS->AllocatePages(AllocateAnyPages, LIP ? LIP->ImageDataType : EfiLoaderData,
                             (size + 0xfff) / 0x1000, &allocated_addr);
  return status;
}

void freeAt(EFI_PHYSICAL_ADDRESS addr, UINTN size) {
  BS->FreePages(addr, (size + 0xfff) / 0x1000);
}

void efi_init(EFI_HANDLE ImageHandle, struct EFI_SYSTEM_TABLE *SystemTable) {
  ST = SystemTable;
  BS = SystemTable->BootServices;
  IM = ImageHandle;

  // 初始化
  BS->SetWatchdogTimer(0, 0, 0, NULL); // 别删，不然UEFI会自动重启
  BS->LocateProtocol(&gop_guid, NULL, (void **)&GOP);
  BS->LocateProtocol(&spp_guid, NULL, (void **)&SPP);

  BS->OpenProtocol(ImageHandle, &lip_guid, (void **)&LIP, ImageHandle, NULL,
                   EFI_OPEN_PROTOCOL_GET_PROTOCOL); // 获取LIP
  BS->OpenProtocol(LIP->DeviceHandle, &sfsp_guid, (void **)&SFSP, ImageHandle, NULL,
                   EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL); // 获取SFSP
}

EFI_STATUS GetMMP(MEMORY_MAP *MemoryMap) {
  EFI_STATUS GetMemoryMapStatus = EFI_SUCCESS;
  // GetMemoryMapStatus = BS->AllocatePool(EfiLoaderData, MemoryMap->MapSize, &MemoryMap->Buffer); // 开辟内存空间存放内存表
  // if(EFI_ERROR(GetMemoryMapStatus)) {
  //   // 无法开辟内存空间
  //   puts(L"XJ380 does not boot properly. Please visit "
  //        L"https://xingjisoft.top/product/os/xj380/error.html. Error code: CANNOT_ALLOC_MEMORYMAP");
  //   while (1); // 寄
  // }

  // 获取内存表
  while( BS->GetMemoryMap(
    &MemoryMap->MapSize,
    (EFI_MEMORY_DESCRIPTOR*)MemoryMap->Buffer,
    &MemoryMap->MapKey,
    &MemoryMap->DescriptorSize,
    &MemoryMap->DescriptorVersion
  ) == EFI_BUFFER_TOO_SMALL) {
    if (MemoryMap->Buffer) {
      BS->FreePool(MemoryMap->Buffer);
      MemoryMap->Buffer = NULL;
    }
    BS->AllocatePool(EfiLoaderData, MemoryMap->MapSize * sizeof(EFI_MEMORY_DESCRIPTOR), &MemoryMap->Buffer);

  } 

  return GetMemoryMapStatus;
}

typedef void (*__attribute__((sysv_abi)) Kernel)(const struct FrameBufferConfig *,
                                                 struct EFI_SYSTEM_TABLE *, struct EFI_SIMPLE_POINTER_PROTOCOL *, struct BOOT_CONFIG *);

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, struct EFI_SYSTEM_TABLE *SystemTable) {
  efi_init(ImageHandle, SystemTable);

  ST->ConOut->ClearScreen(ST->ConOut);

  EFI_STATUS                status;
  EFI_PHYSICAL_ADDRESS      entry_addr;
  struct EFI_FILE_PROTOCOL *root, *kernel_file;
  UINTN                     kernel_size   = 4194304;
  void                     *kernel_buffer = malloc(kernel_size);

  status = SFSP->OpenVolume(SFSP, &root);
  if (EFI_ERROR(status)) {
    puts(L"XJ380 does not boot properly. Please visit "
         L"https://xingjisoft.top/product/os/xj380/error.html. Error code: OPEN_ROOT_DIR_ERROR");
    while (1);
  }
  status = root->Open(root, &kernel_file, L"\\kernel.elf", EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR(status)) {
    puts(L"XJ380 does not boot properly. Please visit "
         L"https://xingjisoft.top/product/os/xj380/error.html. Error code: KERNEL_NOT_FOUND");
    while (1);
  }
  status = kernel_file->Read(kernel_file, &kernel_size,
                             kernel_buffer); // 读取内核，直接读到kernel_buffer
  if (EFI_ERROR(status)) {
    puts(L"XJ380 does not boot properly. Please visit "
         L"https://xingjisoft.top/product/os/xj380/error.html. Error code: CANNOT_READ_KERNEL");
    while (1);
  }

  Elf64_Ehdr *ehdr = (Elf64_Ehdr *)kernel_buffer;
  UINT64      kernel_first_addr, kernel_last_addr;                   // 计算的首尾
  CalcLoadAddressRange(ehdr, &kernel_first_addr, &kernel_last_addr); // 计算范围

  status = mallocAt(kernel_first_addr, kernel_last_addr - kernel_first_addr); // 分配内存
  if (EFI_ERROR(status)) {
    puts(L"XJ380 does not boot properly. Please visit "
         L"https://xingjisoft.top/product/os/xj380/error.html. Error code: CANNOT_ALLOC_MEMORY");
    while (1);
  }

  CopyLoadSegments(ehdr);     // CV
  entry_addr = ehdr->e_entry; // 获取入口点
  free(kernel_buffer);        // 释放内核文件，丢掉！

  struct FrameBufferConfig config = {
      (UINT8 *)GOP->Mode->FrameBufferBase, GOP->Mode->Info->PixelsPerScanLine,
      GOP->Mode->Info->HorizontalResolution, GOP->Mode->Info->VerticalResolution};

  switch (GOP->Mode->Info->PixelFormat) {
  case PixelRedGreenBlueReserved8BitPerColor: config.pixel_format = kRGBR; break;
  case PixelBlueGreenRedReserved8BitPerColor: config.pixel_format = kBGRR; break;
  default:
    
    puts(L"XJ380 does not boot properly. Please visit "
         L"https://xingjisoft.top/product/os/xj380/error.html. Error code: UNSUPPORTED_PIXEL_FORMAT");
    while (1);
  }

  BOOT_CONFIG BootConfig;
  BootConfig.MemoryMap.MapSize = 4096;
  BootConfig.MemoryMap.Buffer = NULL;
  BootConfig.MemoryMap.MapKey = 0;
  BootConfig.MemoryMap.DescriptorSize = 0;
  BootConfig.MemoryMap.DescriptorVersion = 0;

  GetMMP(&BootConfig.MemoryMap);
  
  Kernel kernel = (Kernel)entry_addr;
  kernel(&config, SystemTable, SPP, &BootConfig); // 滚进去！

  //此处=kernel.elf

  while (1); // 趋势
}
