#include "../include/efi.h"
#include "../include/include.h"
typedef struct {
  UINT32 Type;
  EFI_PHYSICAL_ADDRESS PhysicalStart;
  EFI_VIRTUAL_ADDRESS VirtualStart;
  UINT64 NumberOfPages;
  UINT64 Attribute;
} __attribute__((__aligned__(16))) EfiMemoryDesc;
void init_MemoryManager(const FrameBufferConfig &fbc, BOOT_CONFIG *BootConfig) {
  // 初始化
  EfiMemoryDesc *EfiMemory = (EfiMemoryDesc *)(BootConfig->MemoryMap.Buffer);
    /*
    int printy = 164;
    for (int i = 0; i < 10; i++) {
        WriteHex(fbc, 8, printy, EfiMemory->Type, WHITE);
        WriteHex(fbc, 96, printy, EfiMemory->PhysicalStart, WHITE);
        WriteHex(fbc, 248, printy, EfiMemory->VirtualStart, WHITE);
        WriteHex(fbc, 8, printy + 16, EfiMemory->NumberOfPages, WHITE);
        WriteHex(fbc, 160, printy + 16, EfiMemory->Attribute, WHITE);
        printy += 32;
        EfiMemory++;
    }
    */
  OS_MEMORY_DESCRIPTOR *OsMemory = NULL;
  int EfiMemoryPageSize = (BootConfig->MemoryMap.MapSize >> 12) + 1;
  EFI_PHYSICAL_ADDRESS OsMemoryAddress = 0;
  int EfiDescriptorCount = BootConfig->MemoryMap.MapSize / BootConfig->MemoryMap.DescriptorSize;
  for(int i = 0; i < EfiDescriptorCount; i++) {
    if(EfiMemoryPageSize < EfiMemory[i].NumberOfPages && EfiMemory[i].Type == 7) {
    	OsMemoryAddress = EfiMemory[i].PhysicalStart;
    	OsMemory = (OS_MEMORY_DESCRIPTOR *)OsMemoryAddress;
    	break;
		}
	}
	WriteHex(fbc, 0, 0, (UINT64)OsMemory, WHITE);
  int OsMemDescNum = 0;
  // 内存合并与分类
  for(int i = 0; i < EfiDescriptorCount; i++) {
    if(EfiMemory[i].Type == EfiBootServicesCode || EfiMemory[i].Type == EfiBootServicesData || EfiMemory[i].Type == EfiConventionalMemory) {
      OsMemory->Type = FREE_MEMORY;
    }
    else if(EfiMemory[i].Type == EfiMemoryMappedIO) {
      OsMemory->Type = MMIO_MEMORY;
    }
    else {
      OsMemory->Type = UEFI_MEMORY;
    }
    OsMemory->PhysicalStart = EfiMemory[i].PhysicalStart;
    OsMemory->PageSize = EfiMemory[i].NumberOfPages;

    if(i > 0 && OsMemory->Type == (OsMemory - 1)->Type) {
      if(OsMemory->PhysicalStart == (OsMemory - 1)->PhysicalStart + ((OsMemory - 1)->PageSize << 12)) {
        (OsMemory - 1)->PageSize += OsMemory->PageSize;
        continue;
      }
    }
    OsMemory++;
    OsMemDescNum++;
  }
  //输出合并后的
  OsMemory = (OS_MEMORY_DESCRIPTOR *)OsMemoryAddress;
  UINT64 TotalMemory = 0;
  int printy = 164;
  for (int i = 0; i < OsMemDescNum; i++) {
      WriteHex(fbc, 8, printy, OsMemory->Type, WHITE);
      WriteHex(fbc, 96, printy, OsMemory->PhysicalStart, WHITE);
      WriteHex(fbc, 248, printy, OsMemory->PageSize, WHITE);
      printy += 16;
      if(OsMemory->Type != MMIO_MEMORY) {
        TotalMemory += OsMemory->PageSize;
      }
      OsMemory++;
  }
  WriteString(fbc, 8, printy, "TotalMemory:", WHITE);
  WriteHex(fbc, 8, printy + 16, TotalMemory, WHITE);
}
