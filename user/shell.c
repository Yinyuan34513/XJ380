#include "./xlibc/include/unistd.h"

int main(int argc, char *argv[], char *envp[])
{
    write(1, "shell process is running!!!\n", 0);

    return 0;
}
