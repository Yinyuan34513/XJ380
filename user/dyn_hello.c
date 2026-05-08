#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/auxv.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    printf("[dyn-hello] argc=%d argv0=%s\n", argc, argc > 0 ? argv[0] : "(null)");
    printf("[dyn-hello] AT_BASE=0x%lx AT_ENTRY=0x%lx AT_PHDR=0x%lx\n",
           getauxval(AT_BASE),
           getauxval(AT_ENTRY),
           getauxval(AT_PHDR));

    errno = 0;
    long pagesz = sysconf(_SC_PAGESIZE);
    printf("[dyn-hello] sysconf(_SC_PAGESIZE)=%ld errno=%d\n", pagesz, errno);

    write(STDOUT_FILENO, "[dyn-hello] write syscall works\n", 32);
    return 0;
}
