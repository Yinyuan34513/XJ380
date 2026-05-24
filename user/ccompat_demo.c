#include "x3api.h"

int main(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    xapi_Output("XJ380 C compatibility layer demo\n");
    xapi_PrintLine("This app is written in C and calls xapi_* directly.");
    xapi_Printf("Current tick: %llu\n", (unsigned long long)xapi_GetTime());

    UserInfo user_info;
    xapi_GetCurrentUser(&user_info);
    xapi_Printf("Current user: %s\n", user_info.name);

    xapi_EndLine();
    xapi_PrintLine("Press any key to exit...");
    xapi_Getch();
    return 0;
}
