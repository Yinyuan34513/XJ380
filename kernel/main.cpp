//将编译为kernel.elf
extern "C" void KernelMain(unsigned long long fbb, unsigned long long fbs) {
    unsigned char *frame_buffer = reinterpret_cast<unsigned char *>(fbb); // 都进C++了，用一下C++特性不过分吧
    for (unsigned long long i = 0; i < fbs; i++) frame_buffer[i] = 240; // #F0F0F0，颜色与QEMU的菜单栏相近
    while (1) asm("hlt");
}
