// 版权所有©XINGJI Studios 2017-2025 保留所有权利。
// XJ380UEFI引导程序（\EFI\BOOT\BOOTX64.efi）
#include "../include/cinclude.h"
#include "./bootlib.c"
#define NULL 0
struct EFI_SYSTEM_TABLE *ST;                  // 系统表
struct EFI_BOOT_SERVICES *BS;                 // 启动信息
struct EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP;     // GOP
struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SFSP; // 文件
struct EFI_LOADED_IMAGE_PROTOCOL *LIP;        // 文件
struct EFI_SIMPLE_POINTER_PROTOCOL *SPP;      // 鼠标
struct EFI_BATTERY_CHARGING_PROTOCOL *BAT;    // 电池
EFI_HANDLE IM;

void write_serial_string(char *str);
char *strcpy(char *dest, const char *src);
UINT64 strcmp(char *from_str, char *cmp_str);
UINT64 part_strcmp(char *from_str, char *cmp_str, UINT64 size);

/**
 * @brief 将无符号64位整数转换为16位字符串
 *
 * @param buf 目标缓冲区，用于存储转换后的字符串
 * @param len 目标缓冲区的长度
 * @param n 要转换的无符号64位整数
 * @return 返回指向转换后字符串的指针，如果缓冲区长度不足，则返回空指针
 */
static inline __attribute__((always_inline)) UINT16 *u64tostrb16(UINT16 *buf, size_t len,
                                                                 UINT64 n)
{
  UINT16 *s = buf + len;
  *--s = '\0';
  if (n == 0)
    return (*--s = '0', s);
  for (; n; n >>= 4)
    *--s = L"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"[n & 15];
  return s;
}

// 图形输出协议 GUID
EFI_GUID gop_guid = {
    0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};
// 文件加载协议 GUID
EFI_GUID lip_guid = {
    0x5b1b31a1, 0x9562, 0x11d2, {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
// 简单文件系统协议 GUID
EFI_GUID sfsp_guid = {
    0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
// 简单指针输入协议 GUID
EFI_GUID spp_guid = {
    0x31878c87, 0xb75, 0x11d5, {0x9a, 0x4f, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d}};
// 电池协议 GUID
EFI_GUID battery_guid = {
    0x840cb643, 0x8198, 0x428a, {0xa8, 0xb3, 0xa0, 0x72, 0xce, 0x57, 0xcd, 0xb9}};
// ACPI 1.x GUID
EFI_GUID acpi_table_guid = {
    0xeb9d2d30, 0x2d88, 0x11d3, {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}};
// ACPI 2.0或更新 GUID
EFI_GUID efi_acpi_table_guid = {
    0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81}};

/**
 * @brief 输出一个16位的字符到控制台
 *
 * @param c 要输出的16位字符
 */
void putc(unsigned short c)
{
  __WCHAR_TYPE__ str[2] = L" ";
  str[0] = c;
  ST->ConOut->OutputString(ST->ConOut, str);
}

void puts(unsigned short *s)
{
  ST->ConOut->OutputString(ST->ConOut, s);
}

/**
 * @brief 将无符号64位整数转换为16位字符串并输出到控制台
 *
 * @param hex 要转换的无符号64位整数
 */
void puth(unsigned long long hex)
{
  unsigned short buf[20];
  unsigned short *p = buf;
  unsigned short ch;
  int i, flag = 0;

  *p++ = L'0';
  *p++ = L'x'; // 先存一个0x

  if (hex == 0)
    *p++ = L'0'; // 如果是0，直接0x0趋势
  else
  {
    for (i = 28; i >= 0; i -= 4)
    {                        // 每次4位
      ch = (hex >> i) & 0xF; // 0~9, A~F
      // 28（冗余）
      if (flag || ch > 0)
      {             // 跳过前导0
        flag = 1;   // 没有前导0就把flag设为1，这样后面再有0也不会忽略
        ch += L'0'; // 0~9 => '0'~'9'
        if (ch > L'9')
        {
          ch += 7; // 'A' - '9' = 7
        }
        *p++ = ch; // 写入
      }
    }
  }
  *p = L'\0'; // 结束符
  puts(buf);  // 打印
}

void *malloc(size_t buf_size)
{
  void *res;
  unsigned long long status;

  status = BS->AllocatePool(EfiLoaderData, buf_size,
                            &res); // 分配内存
  if (status != EFI_SUCCESS)
    return NULL;

  return res;
}

void free(void *buf)
{
  BS->FreePool(buf);
}

EFI_STATUS mallocAt(EFI_PHYSICAL_ADDRESS addr, UINTN size)
{
  EFI_STATUS status;
  EFI_PHYSICAL_ADDRESS allocated_addr = addr;
  status =
      BS->AllocatePages(AllocateAnyPages, EfiLoaderData, (size + 0xfff) / 0x1000, &allocated_addr);
  return status;
}

void freeAt(EFI_PHYSICAL_ADDRESS addr, UINTN size)
{
  BS->FreePages(addr, (size + 0xfff) / 0x1000);
}

void efi_init(EFI_HANDLE ImageHandle, struct EFI_SYSTEM_TABLE *SystemTable)
{
  ST = SystemTable;
  BS = SystemTable->BootServices;
  IM = ImageHandle;

  // 初始化
  BS->SetWatchdogTimer(0, 0, 0, NULL); // 别删，不然UEFI会自动重启
  BS->LocateProtocol(&gop_guid, NULL, (void **)&GOP);
  BS->LocateProtocol(&spp_guid, NULL, (void **)&SPP);
  BS->LocateProtocol(&battery_guid, NULL, (void **)&BAT);

  BS->OpenProtocol(ImageHandle, &lip_guid, (void **)&LIP, ImageHandle, NULL,
                   EFI_OPEN_PROTOCOL_GET_PROTOCOL); // 获取LIP
  BS->OpenProtocol(LIP->DeviceHandle, &sfsp_guid, (void **)&SFSP, ImageHandle, NULL,
                   EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL); // 获取SFSP
}
EFI_STATUS GetMMP(MEMORY_MAP *MemoryMap)
{
  EFI_STATUS GetMemoryMapStatus = EFI_SUCCESS;
  // GetMemoryMapStatus = BS->AllocatePool(EfiLoaderData, MemoryMap->MapSize, &MemoryMap->Buffer); // 开辟内存空间存放内存表
  // if(EFI_ERROR(GetMemoryMapStatus)) {
  //   // 无法开辟内存空间
  //   puts(L"XJ380 does not boot properly. Please visit "
  //        L"https://xingjisoft.top/product/os/xj380/error.html. Error code: CANNOT_ALLOC_MEMORYMAP");
  //   while (1); // 寄
  // }

  // 获取内存表
  MemoryMap->Buffer = malloc(MemoryMap->MapSize);

  // 获取内存表
  while (BS->GetMemoryMap(&MemoryMap->MapSize, (EFI_MEMORY_DESCRIPTOR *)MemoryMap->Buffer,
                          &MemoryMap->MapKey, &MemoryMap->DescriptorSize,
                          &MemoryMap->DescriptorVersion) == EFI_BUFFER_TOO_SMALL)
  {
    if (MemoryMap->Buffer)
    {
      free(MemoryMap->Buffer);
      MemoryMap->Buffer = NULL;
    }

    // 重新分配更大的缓冲区
    MemoryMap->Buffer = malloc(MemoryMap->MapSize);
  }

  // 检查是否成功分配了内存
  if (!MemoryMap->Buffer)
  {
    GetMemoryMapStatus = EFI_OUT_OF_RESOURCES;
  }
  return GetMemoryMapStatus;
}

void SwitchToResolution(unsigned int x, unsigned int y)
{
  unsigned long long sizeofInfo = 0;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
  for (unsigned int i = 0; i < GOP->Mode->MaxMode; i++)
  {
    GOP->QueryMode(GOP, i, &sizeofInfo, &info);

    if (info->HorizontalResolution == x && info->VerticalResolution == y)
    {
      GOP->SetMode(GOP, i);
    }
  }
}

// 比较GUID
UINTN CompareGUID(EFI_GUID *src, EFI_GUID *obj)
{
  if (src->Data1 == obj->Data1 &&
      src->Data2 == obj->Data2 &&
      src->Data3 == obj->Data3 &&
      src->Data4[0] == obj->Data4[0] &&
      src->Data4[1] == obj->Data4[1] &&
      src->Data4[2] == obj->Data4[2] &&
      src->Data4[3] == obj->Data4[3] &&
      src->Data4[4] == obj->Data4[4] &&
      src->Data4[5] == obj->Data4[5] &&
      src->Data4[6] == obj->Data4[6] &&
      src->Data4[7] == obj->Data4[7])
  {
    return 1; // Yes!
  }
  return 0;
}

#define PAGE_TABLE_MAPPED_ADDRESS 0xFFFFFFFFFFFFF000

void CreateAndMapNewPageTable()
{
  uint64_t new_page_table_addr;
  BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &new_page_table_addr);

  uint64_t old_page_table_addr;
  __asm__ __volatile__("mov %%cr3, %0"
                       : "=r"(old_page_table_addr));

  xmemcpy((void *)new_page_table_addr, (void *)old_page_table_addr, PAGE_SIZE);

  uint64_t *pml4 = (uint64_t *)new_page_table_addr;
  uint64_t *pdp = (uint64_t *)(pml4[0] & ~0xFFF);
  pml4[0] = (uint64_t)pdp | 0x7;
  pml4[256] = (uint64_t)pdp | 0x3;

  // uint64_t pml4_id = (PAGE_TABLE_MAPPED_ADDRESS >> 39) & 0x1FF;
  // uint64_t pml4_addr;
  // BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &pml4_addr);
  // xmemset((void *)(pml4_addr), 0, PAGE_SIZE);
  // pml4[pml4_id] = pml4_addr | 0x3;
  // uint64_t *pdpt = (uint64_t *)(pml4[pml4_id] & ~(0xFFF));
  // uint64_t pdpt_id = (PAGE_TABLE_MAPPED_ADDRESS >> 30) & 0x1FF;
  // uint64_t pdpt_addr;
  // BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &pdpt_addr);
  // xmemset((void *)pdpt_addr, 0, PAGE_SIZE);
  // pdpt[pdpt_id] = pdpt_addr | 0x3;
  // uint64_t *pd = (uint64_t *)(pdpt[pdpt_id] & ~(0xFFF));
  // uint64_t pd_id = (PAGE_TABLE_MAPPED_ADDRESS >> 21) & 0x1FF;
  // uint64_t pd_addr;
  // BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &pd_addr);
  // xmemset((void *)pd_addr, 0, PAGE_SIZE);
  // pd[pd_id] = pd_addr | 0x3;
  // uint64_t *pt = (uint64_t *)(pd[pd_id] & ~(0xFFF));
  // uint64_t pt_id = (PAGE_TABLE_MAPPED_ADDRESS >> 12) & 0x1FF;
  // pt[pt_id] = new_page_table_addr | 0x3;

  __asm__ __volatile__("mov %0, %%cr3"
                       :
                       : "r"(new_page_table_addr));
}

void MapVirtToPhys(uint64_t virt, uint64_t size)
{
  uint64_t page_table_addr;
  __asm__ __volatile__("mov %%cr3, %0"
                       : "=r"(page_table_addr));

#define KERNEL_PERM 0x03

  for (uint64_t addr = virt; addr < virt + size + PAGE_SIZE - 1; addr += PAGE_SIZE)
  {
    // Map vaddr to paddr
    uint64_t *pml4 = (uint64_t *)page_table_addr;
    uint64_t pml4_id = (addr >> 39) & 0x1FF;
    if (pml4[pml4_id] == 0)
    {
      uint64_t pml4_addr;
      BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &pml4_addr);
      xmemset((void *)pml4_addr, 0, PAGE_SIZE);
      pml4[pml4_id] = pml4_addr | KERNEL_PERM;
    }
    uint64_t *pdpt = (uint64_t *)(pml4[pml4_id] & ~(0xFFF));
    uint64_t pdpt_id = (addr >> 30) & 0x1FF;
    if (pdpt[pdpt_id] == 0)
    {
      uint64_t pdpt_addr;
      BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &pdpt_addr);
      xmemset((void *)pdpt_addr, 0, PAGE_SIZE);
      pdpt[pdpt_id] = pdpt_addr | KERNEL_PERM;
    }
    uint64_t *pd = (uint64_t *)(pdpt[pdpt_id] & ~(0xFFF));
    uint64_t pd_id = (addr >> 21) & 0x1FF;
    if (pd[pd_id] == 0)
    {
      uint64_t pd_addr;
      BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &pd_addr);
      xmemset((void *)pd_addr, 0, PAGE_SIZE);
      pd[pd_id] = pd_addr | KERNEL_PERM;
    }
    uint64_t *pt = (uint64_t *)(pd[pd_id] & ~(0xFFF));
    uint64_t pt_id = (addr >> 12) & 0x1FF;
    if (pt[pt_id] == 0)
    {
      uint64_t paddr;
      BS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &paddr);
      xmemset((void *)paddr, 0, PAGE_SIZE);
      pt[pt_id] = paddr | KERNEL_PERM;
    }
  }
}

typedef void (*__attribute__((sysv_abi))
              Kernel)(const struct FrameBufferConfig *, struct EFI_SYSTEM_TABLE *, BOOT_CONFIG *);

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, struct EFI_SYSTEM_TABLE *SystemTable)
{
  write_serial_string("\n");
  write_serial_string("XJ380 Operating System - Serial Log\n");
  write_serial_string("Copyright(C) XINGJI Interacitve Software 2017-2025 All rights reserved.\n");
  write_serial_string("OS Version: BuildVersion\n");
  efi_init(ImageHandle, SystemTable);
  write_serial_string("EFI Initialize Success.\n");

  write_serial_string("Creating new page table\n");
  CreateAndMapNewPageTable();
  write_serial_string("New page table created\n");

  ST->ConOut->ClearScreen(ST->ConOut);

  EFI_STATUS status;
  EFI_PHYSICAL_ADDRESS entry_addr;
  struct EFI_FILE_PROTOCOL *root, *kernel_file;
  UINTN kernel_size = 4194304;
  void *kernel_buffer = malloc(kernel_size);

  // 引导内核
  status = SFSP->OpenVolume(SFSP, &root);
  if (EFI_ERROR(status))
  {
    puts(L"XJ380 does not boot properly. Please visit "
         L"https://xingjisoft.top/product/os/xj380/error.html. Error code: OPEN_ROOT_DIR_ERROR");
    write_serial_string("[bootx64.c:293]Root Dir Open Failed. Stop.\n");
    while (1)
      ;
  }
  write_serial_string("Root Dir Open Success.\n");
  status = root->Open(root, &kernel_file, L"\\kernel.elf", EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR(status))
  {
    puts(L"XJ380 does not boot properly. Please visit "
         L"https://xingjisoft.top/product/os/xj380/error.html. Error code: KERNEL_NOT_FOUND");
    write_serial_string("[bootx64.c:303]Kernel Read Failed. Stop.\n");
    while (1)
      ;
  }
  status = kernel_file->Read(kernel_file, &kernel_size,
                             kernel_buffer); // 读取内核，直接读到kernel_buffer
  if (EFI_ERROR(status))
  {
    puts(L"XJ380 does not boot properly. Please visit "
         L"https://xingjisoft.top/product/os/xj380/error.html. Error code: CANNOT_READ_KERNEL");
    write_serial_string("[bootx64.c:313]Kernel Read Failed. Stop.\n");
    while (1)
      ;
  }
  write_serial_string("XSK2.0 Kernel Read Success.\n");

  Elf64_Ehdr *ehdr = (Elf64_Ehdr *)kernel_buffer;
  UINT64 kernel_first_addr, kernel_last_addr;                        // 计算的首尾
  CalcLoadAddressRange(ehdr, &kernel_first_addr, &kernel_last_addr); // 计算范围

  MapVirtToPhys(kernel_first_addr, kernel_last_addr - kernel_first_addr);

  xmemset((void *)kernel_first_addr, 0, kernel_last_addr - kernel_first_addr);

  CopyLoadSegments(ehdr);     // CV
  entry_addr = ehdr->e_entry; // 获取入口点
  // free(kernel_buffer);        // 释放内核文件，丢掉！

  SwitchToResolution(1440, 900); // 切换分辨率

  struct FrameBufferConfig config = {
      (UINT8 *)(GOP->Mode->FrameBufferBase + 0xFFFF800000000000), GOP->Mode->Info->PixelsPerScanLine,
      GOP->Mode->Info->HorizontalResolution, GOP->Mode->Info->VerticalResolution};

  switch (GOP->Mode->Info->PixelFormat)
  {
  case PixelRedGreenBlueReserved8BitPerColor:
    config.pixel_format = kRGBR;
    break;
  case PixelBlueGreenRedReserved8BitPerColor:
    config.pixel_format = kBGRR;
    break;
  default:

    puts(L"XJ380 does not boot properly. Please visit "
         L"https://xingjisoft.top/product/os/xj380/error.html. Error code: "
         L"UNSUPPORTED_PIXEL_FORMAT");
    write_serial_string("[bootx64.c:346]Vedio Initialize Failed. Stop.\n");
    while (1)
      ;
  }
  write_serial_string("Vedio Initialize Success.\n");

  BOOT_CONFIG *BootConfig;
  BootConfig = malloc(sizeof(BOOT_CONFIG));
  xmemset(BootConfig, 0, sizeof(BOOT_CONFIG));

  // 获取RSDP
  EFI_CONFIGURATION_TABLE *SystemConfigTable = NULL;
  UINT8 AcpiTableWasFound = 0; // 0=找不着 1=找到了
  SystemConfigTable = ST->ConfigurationTable;
  for (UINTN i = 0; i < ST->NumberOfTableEntries; i++)
  {
    if (CompareGUID(&SystemConfigTable->VendorGuid, &acpi_table_guid) == 1 ||
        CompareGUID(&SystemConfigTable->VendorGuid, &efi_acpi_table_guid) == 1)
    {
      // 找到了ACPI表
      // 修订版 >= ACPI 2.0
      RSDP_TYPE *AcpiRsdp = (RSDP_TYPE *)(SystemConfigTable->VendorTable); // 指向存储RSDP的表的指针
      // 修订版 >= ACPI 2.0
      if (AcpiRsdp->version >= 2)
      {
        write_serial_string("ACPI Table Found Success.\n");
        write_serial_string("ACPI Version: ");
        write_serial_string(Dec2Char(AcpiRsdp->version));
        write_serial_string("\n");
        write_serial_string("XSDT Address: ");
        write_serial_string(Hex2Char(AcpiRsdp->XsdtAddr));
        write_serial_string("\n");

        ACPI_TABLE_HEADER *XSDT = (ACPI_TABLE_HEADER *)(AcpiRsdp->XsdtAddr); // 指向XSDT的指针
        UINT64 *AcpiTableEntryPtr = (UINT64 *)(XSDT + 1);                    // 指向XSDT后面的其他SDT的地址的指针
        UINT64 Entries = (XSDT->length - sizeof(ACPI_TABLE_HEADER)) / 8;     // SDT数量
        AcpiTableWasFound = 1;
        UINT8 ObjTableWasFound = 0;
        // 遍历所有ACPI表
        for (UINT64 j = 0; j < Entries; j++, AcpiTableEntryPtr++)
        {
          write_serial_string("SDT Entry ");
          write_serial_string(Dec2Char(j));
          write_serial_string(":\n");

          ACPI_TABLE_HEADER *AcpiSdtPointer = (ACPI_TABLE_HEADER *)(*AcpiTableEntryPtr); // 指向其他SDT的表头的指针

          write_serial_string("SDT Address: ");
          write_serial_string(Hex2Char((UINT64)(AcpiTableEntryPtr)));
          write_serial_string("\n");
          write_serial_string(AcpiSdtPointer->sign);
          write_serial_string("\n");

          if (!part_strcmp(AcpiSdtPointer->sign, "APIC", 4))
          {
            write_serial_string("MADT Found Success.\n");
            // MADT!!!
            BootConfig->MADT = (UINT64)(AcpiSdtPointer);
            ObjTableWasFound++;
          }
          else if (!part_strcmp(AcpiSdtPointer->sign, "HPET", 4))
          {
            write_serial_string("HPET Found Success.\n");
            // HPET!!!
            BootConfig->HPET = (UINT64)(AcpiSdtPointer);
            ObjTableWasFound++;
          }
          else if (!part_strcmp(AcpiSdtPointer->sign, "MCFG", 4))
          {
            write_serial_string("MCFG Found Success.\n");
            // MCFG!!!
            BootConfig->MCFG = (UINT64)(AcpiSdtPointer);
            ObjTableWasFound++;
          }
          write_serial_string("\n");
        }
        break;
      }
    }
    SystemConfigTable++;
  }
  if (AcpiTableWasFound == 0)
  {
    puts(L"XJ380 does not boot properly. Please visit "
         L"https://xingjisoft.top/product/os/xj380/error.html. Error code: "
         L"ACPI_TABLE_NOT_FOUND\r\n");
    write_serial_string("[bootx64.c:432]ACPI Table Not Found. Stop.\n");
    while (1)
      ;
  }
  if (BootConfig->MADT == 0)
  {
    puts(L"XJ380 does not boot properly. Please visit "
         L"https://xingjisoft.top/product/os/xj380/error.html. Error code: "
         L"MADT_NOT_FOUND\r\n");
    write_serial_string("[bootx64.c:441]MADT Not Found. Stop.\n");
    while (1)
      ;
  }
  // if (BootConfig->MCFG == 0)
  // {
  //   puts(L"XJ380 does not boot properly. Please visit "
  //        L"https://xingjisoft.top/product/os/xj380/error.html. Error code: "
  //        L"MADT_NOT_FOUND\r\n");
  //   write_serial_string("[bootx64.c:450]MCFG Not Found. Stop.\n");
  //   while (1)
  //     ;
  // }

  // 获取memory map
  BootConfig->MemoryMap.MapSize = 4096;
  BootConfig->MemoryMap.Buffer = NULL;
  BootConfig->MemoryMap.MapKey = 0;
  BootConfig->MemoryMap.DescriptorSize = 0;
  BootConfig->MemoryMap.DescriptorVersion = 0;

  GetMMP(&BootConfig->MemoryMap);

  EFI_STATUS ExitBSStatus = EFI_SUCCESS;

  // say goodbye~
  ExitBSStatus = BS->ExitBootServices(ImageHandle, BootConfig->MemoryMap.MapKey);

  if (EFI_ERROR(ExitBSStatus))
  {
    puts(L"XJ380 does not boot properly. Please visit "
         L"https://xingjisoft.top/product/os/xj380/error.html. Error code: "
         L"CANNOT_BOOT_SYSTEM");
    write_serial_string("[bootx64.c:474]Boot Services Exit Failed. Stop.\n");
    while (1)
      ;
  }

  Kernel kernel = (Kernel)entry_addr;
  kernel((struct FrameBufferConfig *)((UINT64)&config + 0xFFFF800000000000), SystemTable, BootConfig); // 滚进去！

  // 此处=kernel.elf

  while (1)
    ; // 趋势
}
