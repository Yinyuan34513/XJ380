#include "./xapi/include/x3api.h"

static int init_main_impl(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    xapi_OutputSerial((char *)"init: start waitpid selftest\n");

    int      status = -1;
    uint64_t pid    = fork();
    if ((int64_t)pid < 0)
    {
        char logbuf[128];
        snprintf(logbuf, sizeof(logbuf), "waitpid-selftest: fork failed ret=%lld\n", (long long)(int64_t)pid);
        xapi_OutputSerial(logbuf);
    }
    else if (pid == 0)
    {
        xapi_OutputSerial((char *)"waitpid-selftest: child exit 42\n");
        exit(42);
    }
    else
    {
        int  waited = waitpid((int)pid, &status, 0);
        char logbuf[160];
        snprintf(logbuf,
                 sizeof(logbuf),
                 "waitpid-selftest: waited=%d expect=%llu status=0x%x exited=%d code=%d\n",
                 waited,
                 (unsigned long long)pid,
                 (unsigned int)status,
                 WIFEXITED(status),
                 WEXITSTATUS(status));
        xapi_OutputSerial(logbuf);

        if (waited == (int)pid && WIFEXITED(status) && WEXITSTATUS(status) == 42)
        {
            xapi_OutputSerial((char *)"waitpid-selftest: PASS\n");
        }
        else
        {
            xapi_OutputSerial((char *)"waitpid-selftest: FAIL\n");
        }
    }
    
    while(1)__asm__ __volatile__("pause");
    return -1;
}

extern "C" int init_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int init_main_cpp(int argc, char *argv[], char *envp[])
{
    return init_main_impl(argc, argv, envp);
}
