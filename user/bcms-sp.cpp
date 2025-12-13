#include "./xapi/include/x3api.h"

int main(int argc, char *argv[], char *envp[])
{
    xapi_Printf("%s 哈机密\n","gulu");
    char t[114514];
    xapi_Input(t);
    xapi_Output("Hello World!");
    xapi_Output(t);
    while (1);
    return 0;
}
