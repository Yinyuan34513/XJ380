// main.cpp  
#include "../include/include.h" 
#include "../include/efi.h" 

extern "C" void KernelMain(const FrameBufferConfig &fbc, struct EFI_SYSTEM_TABLE *SystemTable) {  
    uint64_t str[3]; // 打印用的字符串
    struct EFI_INPUT_KEY key; // 用于接收按键
    init_desktop(fbc);
    while (1) {
        WriteString(fbc, 100, 100, (char*)SystemTable, {255, 255, 255});  
        if (!SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key)) { // nmd就这行卡住了焯

            WriteString(fbc, 100, 300, "b", {255, 255, 255}); 

            if (key.UnicodeChar != L'\r') { // 不是换行
                str[0] = key.UnicodeChar; // 第一个字符就输出
                str[1] = L'\0'; // 第二个字符是\0，结束符
            } else { // 是换行或不可打印字符
                str[0] = L'\r'; // \r
                str[1] = L'\n'; // 给EFI擦屁股，\r\n才是一个换行，而enter只能接收到\r
                str[2] = L'\0'; // 结束符
            }
            WriteString(fbc, 100, 100, (char*)str, {255, 255, 255}); 
        }
    }
    // 按理来说不可能到这儿
    // 如果真因为什么B原因到这了就帮忙清理一下吧

}