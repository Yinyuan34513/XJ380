#include <x3api.h>
#include <krlibc.h>
#include "cm_proto.h"

HDLE handle;

UINT64 win_width;
UINT64 win_height;

bool exit_cm = false;

void ctrlmenu_MessagePrcor(UINT64 Type, UINT64 hData, UINT64 lData)
{
    switch (Type)
    {
    case MSG_LBUTTON:
        process_left_key(hData, lData);
        break;
    case MSG_CRL:
        if (hData <= 1999) { change_setting_apps(hData); } // cm_graph.cpp:145
        else if (hData == 2001) { change_clock_hour_offset(+1); }
        else if (hData == 2002) { change_clock_hour_offset(23); }
        else if (hData == 3000) { change_setting_background(); }
        else if (hData == 114514) { delete_input_box(); charbox_input_complete = -1; }
        else if (hData == 114515) { delete_input_box(); charbox_input_complete = 1; }
        break;
    case MSG_CHAR:
        put_char((char)lData);
        break;
    case MSG_SPCHAR:
        if (lData == '\b') { backspace(); }
    }
    return;
}

static int ctrlmenu_main_impl(int argc, char *argv[], char *envp[])
{
    (void)envp;

    if (argc == 1)
    {
        if (strcmp(argv[0], "shortdock-open-settings") == 0)
        {
            cindex = 3;
        }
        else if (strcmp(argv[0], "shortdock-open-about") == 0)
        {
            cindex = 3;
        }
        else if (strcmp(argv[0], "shortdock-open-graphics") == 0)
        {
            cindex = 3;
            setting_cindex = 3;
        }
    }

    XWINDOW Winfo;
    Winfo.title = (char *)"控制中心";
    Winfo.sets  = XWIN_FULL_SCR;  
    xapi_CreateWindow(&handle, &Winfo);
    xapi_GetWindowSize(handle, &win_width, &win_height);
    SetMsgPrcor(handle, ctrlmenu_MessagePrcor);

    draw_background();

    xapi_RefreshWindow(handle);

    int old_min = 60;
    int old_sec = -1;

    while (1)
    {
        if (exit_cm)
            xapi_Exit(0);
            
        __asm__ __volatile__("pause");

        TimeType tm;
        xapi_GetTimeX(&tm);

        if (tm.tm_min != old_min)
        {
            draw_background();
            old_min = tm.tm_min;
        }
        else if (cindex == 3 && setting_cindex == 2 && tm.tm_sec != old_sec)
        {
            draw_background();
        }
        old_sec = tm.tm_sec;

        if (waiting_change_setting_apps)
        {
            while (true)
            {
                if (charbox_input_complete != 0) break;
            }

            if (charbox_input_complete != -1)
            {
                UserInfo usif;
                xapi_GetCurrentUser(&usif);
                char runfile_path[256];
                memset((void *)runfile_path, 0, 256);
                strcat(runfile_path, "/users/");
                strcat(runfile_path, usif.name);
                strcat(runfile_path, "/runfile.dat");
                XFILE *f = xapi_OpenFile(runfile_path);
                if (f) 
                {
                    RunfileSettings_Format *rff = (RunfileSettings_Format *)f->buffer;

                    strcpy(rff->items[change_setting_apps_id].runpath, charbox_buf);

                    xapi_CloseFile(f);

                    draw_background();
                }
            }
            waiting_change_setting_apps = false;
        }
        if (waiting_change_setting_background)
        {
            
            while (true)
            {
                if (charbox_input_complete != 0) break;
            }

            if (charbox_input_complete != -1)
            {
                UserInfo usif;
                xapi_GetCurrentUser(&usif);
                char runfile_path[256];
                memset((void *)runfile_path, 0, 256);
                strcat(runfile_path, "/users/");
                strcat(runfile_path, usif.name);
                strcat(runfile_path, "/settings.dat");
                XFILE *f = xapi_OpenFile(runfile_path);
                if (f) 
                {
                    SettingsDataFileFormat *sdff = (SettingsDataFileFormat *)f->buffer;

                    strcpy(sdff->BackgroundFilePath, charbox_buf);

                    xapi_CloseFile(f);

                    draw_background();
                }
            }
            waiting_change_setting_background = false;
        }
        
        xapi_Sleep(1000);
    }
}

extern "C" int ctrlmenu_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int ctrlmenu_main_cpp(int argc, char *argv[], char *envp[])
{
    return ctrlmenu_main_impl(argc, argv, envp);
}
