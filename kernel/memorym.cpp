#include "../include/efi.h"
#include "../include/include.h"
#include "./build_settings.h"
typedef struct {
  UINT32               Type;
  EFI_PHYSICAL_ADDRESS PhysicalStart;
  EFI_VIRTUAL_ADDRESS  VirtualStart;
  UINT64               NumberOfPages;
  UINT64               Attribute;
} __attribute__((__aligned__(16))) EfiMemoryDesc;

extern "C" void memory_init(void *ptr, size_t size);

void memory_test(const FrameBufferConfig &fbc) {
  /*
  void *ptr1 = malloc(16);
  void *ptr2 = malloc(16);
  void *ptr3 = malloc(16);
  WriteString(fbc, 440, 132, "MemoryManagerTest:", WHITE);
  WriteHex(fbc, 440, 148, (size_t)ptr1, WHITE);
  WriteHex(fbc, 440, 164, (size_t)ptr2, WHITE);
  WriteHex(fbc, 440, 180, (size_t)ptr3, WHITE);
  free(ptr2);
  void *ptr4 = malloc(16);
  WriteHex(fbc, 440, 196, (size_t)ptr4, WHITE);
  free(ptr3);
  free(ptr4);
  void *ptr5 = malloc(8192);
  WriteHex(fbc, 440, 212, (size_t)ptr5, WHITE);
  free(ptr1);
  free(ptr5);
  */
}

void init_MemoryManager(const FrameBufferConfig &fbc, BOOT_CONFIG *BootConfig) {
  // 初始化
  write_serial_string("Initializing Memory Manager...\n");
  EfiMemoryDesc        *EfiMemory = (EfiMemoryDesc *)(BootConfig->MemoryMap.Buffer);
#if BUILD_EDITION == DEBUG_VERSION 
  write_serial_string("MemoryMap:\n");
  for (int i = 0; i < (BootConfig->MemoryMap.MapSize / BootConfig->MemoryMap.DescriptorSize); i++) {
    write_serial_hex(EfiMemory->Type);
	  write_serial_string(" ");
    write_serial_hex(EfiMemory->PhysicalStart);
	  write_serial_string(" ");
    write_serial_hex(EfiMemory->VirtualStart);
	  write_serial_string(" ");
    write_serial_hex(EfiMemory->NumberOfPages);
	  write_serial_string(" ");
    write_serial_hex(EfiMemory->Attribute);
	  write_serial_string("\n");
    EfiMemory++;
  }
  EfiMemory = (EfiMemoryDesc *)(BootConfig->MemoryMap.Buffer);
#endif
  
  OS_MEMORY_DESCRIPTOR *OsMemory          = NULL;
  int                   EfiMemoryPageSize = (BootConfig->MemoryMap.MapSize >> 12) + 1;
  EFI_PHYSICAL_ADDRESS  OsMemoryAddress   = 0;
  int EfiDescriptorCount = BootConfig->MemoryMap.MapSize / BootConfig->MemoryMap.DescriptorSize;
  for (int i = 0; i < EfiDescriptorCount; i++) {
    if (EfiMemoryPageSize < EfiMemory[i].NumberOfPages && EfiMemory[i].Type == 7) {
      OsMemoryAddress = EfiMemory[i].PhysicalStart;
      OsMemory        = (OS_MEMORY_DESCRIPTOR *)OsMemoryAddress;
      break;
    }
  }

  /*
  if (BUILD_EDITION == DEBUG_VERSION) {
    WriteString(fbc, 480, 480, "FirstFreeMemoryAddress:", WHITE);
    WriteHex(fbc, 480, 496, (UINT64)OsMemory, WHITE);
  }
  */

  size_t maxpageaddr = 0;
  size_t maxpagesize = 0;

  int OsMemDescNum = 0;
  // 内存合并与分类
  for (int i = 0; i < EfiDescriptorCount; i++) {
    if (EfiMemory[i].Type == EfiConventionalMemory || EfiMemory[i].Type == EfiBootServicesCode || EfiMemory[i].Type == EfiBootServicesData) {
      OsMemory->Type = FREE_MEMORY;
    } else if (EfiMemory[i].Type == EfiMemoryMappedIO) {
      OsMemory->Type = MMIO_MEMORY;
    } else {
      OsMemory->Type = UEFI_MEMORY;
    }
    OsMemory->PhysicalStart = EfiMemory[i].PhysicalStart;
    OsMemory->PageSize      = EfiMemory[i].NumberOfPages;

    if (OsMemory->Type == FREE_MEMORY && OsMemory->PageSize > maxpagesize) {
      maxpageaddr = OsMemory->PhysicalStart;
      maxpagesize = OsMemory->PageSize;
    }

    if (i > 0 && OsMemory->Type == (OsMemory - 1)->Type) {
      if (OsMemory->PhysicalStart ==
          (OsMemory - 1)->PhysicalStart + ((OsMemory - 1)->PageSize << 12)) {
        (OsMemory - 1)->PageSize += OsMemory->PageSize;
        continue;
      }
    }
    OsMemory++;
    OsMemDescNum++;
  }

  memory_init((void *)maxpageaddr, maxpagesize * PAGE_SIZE);

  //输出合并后的
  OsMemory           = (OS_MEMORY_DESCRIPTOR *)OsMemoryAddress;
  UINT64 TotalMemory = 0;

#if BUILD_EDITION == DEBUG_VERSION 
    write_serial_string("MemoryMap2:\n");
    write_serial_string("Type | PhysicalStart | PageSize\n");
    for (int i = 0; i < OsMemDescNum; i++) {
      	write_serial_hex(OsMemory->Type);
    	  write_serial_string("  | ");
      	write_serial_hex(OsMemory->PhysicalStart);
    	  write_serial_string("       | ");
      	write_serial_hex(OsMemory->PageSize);
	  	  write_serial_string("\n");
      	if (OsMemory->Type != MMIO_MEMORY) { TotalMemory += OsMemory->PageSize; }
      	OsMemory++;
    }
    write_serial_string("TotalMemory: ");
    write_serial_dec(TotalMemory * 4); // 一个Page=4KB
    write_serial_string("KB\n");
    //memory_test(fbc);
#endif

  write_serial_string("Memory Manager Initialize Success.\n");
}
