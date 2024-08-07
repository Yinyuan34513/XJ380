#include "efi.h"
#define NULL 0
struct EFI_SYSTEM_TABLE *ST;
struct EFI_BOOT_SERVICES *BS;
struct EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP;
struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SFSP;
struct EFI_LOADED_IMAGE_PROTOCOL *LIP;
EFI_HANDLE IM;

struct EFI_GUID gop_guid = {0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};
struct EFI_GUID lip_guid = {0x5b1b31a1, 0x9562, 0x11d2, {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
struct EFI_GUID sfsp_guid = {0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};

void putc(unsigned short c)
{
    unsigned short str[2] = L" ";
    str[0] = c;
    ST->ConOut->OutputString(ST->ConOut, str);
}

void puts(unsigned short *s)
{
    ST->ConOut->OutputString(ST->ConOut, s);
}

void puth(unsigned long long hex)
{
    unsigned short buf[20];
    unsigned short *p = buf;
    unsigned short ch;
    int i, flag = 0;

    *p++ = L'0';
    *p++ = L'x'; // 先存一个0x

    if (hex == 0) *p++ = L'0'; // 如果是0，直接0x0趋势
    else {
        for (i = 28; i >= 0; i -= 4) { // 每次4位
            ch = (hex >> i) & 0xF; // 0~9, A~F
            // 28（冗余）
            if (flag || ch > 0) { // 跳过前导0
                flag = 1; // 没有前导0就把flag设为1，这样后面再有0也不会忽略
                ch += L'0'; // 0~9 => '0'~'9'
                if (ch > L'9') {
                    ch += 7; // 'A' - '9' = 7
                }
                *p++ = ch; // 写入
            }
        }
    }
    *p = L'\0'; // 结束符
    puts(buf); // 打印
}

void *malloc(int buf_size)
{
    void *res;
    unsigned long long status;

    status = BS->AllocatePool(LIP ? LIP->ImageDataType : EfiLoaderData, buf_size, &res); // 分配内存，第一个参数是判断LIP是否存在，如果存在就直接拿过来用
    if (status < 0) return NULL;

    return res;
}

void free(void *buf)
{
    BS->FreePool(buf);
}

EFI_STATUS mallocAt(EFI_PHYSICAL_ADDRESS addr, UINTN size)
{
    return BS->AllocatePages(AllocateAddress, LIP ? LIP->ImageDataType : EfiLoaderData, (size + 0xfff) / 0x1000, &addr);
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

    BS->SetWatchdogTimer(0, 0, 0, NULL); // 别删，不然UEFI会自动重启
    BS->LocateProtocol(&gop_guid, NULL, (void **) &GOP);

    BS->OpenProtocol(ImageHandle, &lip_guid, (void **) &LIP, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL); // 获取LIP
    BS->OpenProtocol(LIP->DeviceHandle, &sfsp_guid, (void **) &SFSP, ImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL); // 获取SFSP
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, struct EFI_SYSTEM_TABLE *SystemTable)
{
    efi_init(ImageHandle, SystemTable);

    ST->ConOut->ClearScreen(ST->ConOut);

    EFI_STATUS status;
    EFI_PHYSICAL_ADDRESS kernel_base_addr = 0x100000, entry_addr;
    struct EFI_FILE_PROTOCOL *root, *kernel_file;
    UINTN kernel_size = 4194304;

    status = SFSP->OpenVolume(SFSP, &root);
    if (EFI_ERROR(status)) {
        puts(L"XJ380 does not boot properly. Please visit https://xingjisoft.top/product/os/xj380/error.html. Error code: OPEN_ROOT_DIR_ERROR");
        puts(L"XJ380 无法正常引导。 请访问 https://xingjisoft.top/product/os/xj380/error.html 以获取解决办法。 错误代码: OPEN_ROOT_DIR_ERROR");
        while (1);
    }
    status = root->Open(root, &kernel_file, L"\\kernel.elf", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        puts(L"XJ380 does not boot properly. Please visit https://xingjisoft.top/product/os/xj380/error.html. Error code: KERNEL_NOT_FOUND");
        puts(L"XJ380 无法正常引导。 请访问 https://xingjisoft.top/product/os/xj380/error.html 以获取解决办法。 错误代码: OPEN_ROOT_DIR_ERROR");
        while (1);
    }
    status = mallocAt(kernel_base_addr, kernel_size); // 提前分配内存
    if (EFI_ERROR(status)) {
        puts(L"XJ380 does not boot properly. Please visit https://xingjisoft.top/product/os/xj380/error.html. Error code: CANNOT_ALLOC_KERNEL_BUF");
        puts(L"XJ380 无法正常引导。 请访问 https://xingjisoft.top/product/os/xj380/error.html 以获取解决办法。 错误代码: OPEN_ROOT_DIR_ERROR");
        while (1);
    }
    status = kernel_file->Read(kernel_file, &kernel_size, (void *) kernel_base_addr); // 读取内核
    if (EFI_ERROR(status)) {
        puts(L"XJ380 does not boot properly. Please visit https://xingjisoft.top/product/os/xj380/error.html. Error code: CANNOT_READ_KERNEL");
        puts(L"XJ380 无法正常引导。 请访问 https://xingjisoft.top/product/os/xj380/error.html 以获取解决办法。 错误代码: OPEN_ROOT_DIR_ERROR");
        while (1);
    }

    //加载内核(kernel.elf)
    entry_addr = *(EFI_PHYSICAL_ADDRESS *) (kernel_base_addr + 24);
    typedef int Kernel(void);
    Kernel *kernel = (Kernel *) entry_addr;
    int retval = kernel();

    //此处=kernel.elf

    //测试输出（正常应该是7）
    puth(retval);
    while (1);
}
