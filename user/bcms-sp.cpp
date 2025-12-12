#include "./xapi/include/x3api.h"

int main(int argc, char *argv[], char *envp[])
{
    xapi_Output("你好世界\n");
    char t[114514];
    xapi_Input(t);
    xapi_Output("Hello World!");
    xapi_Output(t);
    while (1);
    return 0;
}
