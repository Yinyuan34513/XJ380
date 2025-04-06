#include <unistd.h>

int main(int argc, char *argv[], char *envp[])
{
    write(1, "Shell process is running!!!\n", 29);

    while (1)
        __asm__ __volatile__("pause");
}
