void _start(int argc, char *argv[])
{
    while (1)
    {
        __asm__ __volatile__("pause");
    }
}
