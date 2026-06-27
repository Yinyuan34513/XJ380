#include "./xapi/include/x3api.h"

static volatile UINT64 g_last_notification_id = 0;
static volatile UINT64 g_last_action_id       = 0;
static volatile UINT64 g_callback_count       = 0;

static void log_line(const char *message)
{
    xapi_OutputSerial((char *)message);
}

static void log_u64(const char *prefix, UINT64 value)
{
    char buf[160];
    snprintf(buf, sizeof(buf), "%s%llu\n", prefix, value);
    log_line(buf);
}

static void notify_callback(UINT64 notification_id, UINT64 action_id)
{
    g_last_notification_id = notification_id;
    g_last_action_id       = action_id;
    g_callback_count++;

    char buf[192];
    snprintf(buf, sizeof(buf), "msgtest: callback notification=%llu action=%llu count=%llu\n",
             notification_id, action_id, g_callback_count);
    log_line(buf);
}

static UINT64 send_notification(const char *name,
                                UINT64 key,
                                const char *title,
                                const char *text,
                                UINT64 icon,
                                const char *icon_path,
                                const char *action0_text,
                                UINT64 action0_id,
                                const char *action1_text,
                                UINT64 action1_id,
                                UINT64 delay_ms)
{
    XApiNotificationRequest req;
    memset(&req, 0, sizeof(req));
    req.key          = key;
    req.title        = (char *)title;
    req.text         = (char *)text;
    req.builtin_icon = icon;
    req.icon_path    = (char *)icon_path;
    req.action0_text = (char *)action0_text;
    req.action0_id   = action0_id;
    req.action1_text = (char *)action1_text;
    req.action1_id   = action1_id;

    char logbuf[192];
    snprintf(logbuf, sizeof(logbuf), "msgtest: send %s\n", name);
    log_line(logbuf);

    UINT64 ret = xapi_SendNotification(&req);
    snprintf(logbuf, sizeof(logbuf), "msgtest: ret %s = %lld\n", name, (long long)ret);
    log_line(logbuf);

    if (delay_ms > 0) xapi_Sleep(delay_ms);
    return ret;
}

static int msgtest_main_impl(int argc, char *argv[], char *envp[])
{
    (void)envp;

    if (argc > 1 && strcmp(argv[1], "--expect-nondesktop") == 0)
    {
        UINT64 ret = send_notification("nondesktop",
                                       0,
                                       "Non-desktop notification",
                                       "Expected result is 0 outside desktop state.",
                                       XNOTIFY_ICON_WARNING,
                                       NULL,
                                       NULL,
                                       0,
                                       NULL,
                                       0,
                                       0);
        log_u64("msgtest: nondesktop ret=", ret);
        return ret == 0 ? 0 : 1;
    }

    log_line("msgtest: start XAPI 3.6.2 toast notification test\n");
    UINT64 set_ret = xapi_SetNotifyPrcor(notify_callback);
    log_u64("msgtest: set notify prcor ret=", set_ret);

    xapi_SendAppMessage((char *)"Compat SendAppMessage",
                        (char *)"This old API should return immediately as an info toast.");
    log_line("msgtest: compat call returned\n");
    xapi_Sleep(500);

    send_notification("overflow-1", 0, "Overflow 1", "Oldest visible toast.", XNOTIFY_ICON_INFO, NULL, NULL, 0, NULL, 0, 250);
    send_notification("overflow-2", 0, "Overflow 2", "Second visible toast.", XNOTIFY_ICON_SUCCESS, NULL, NULL, 0, NULL, 0, 250);
    send_notification("overflow-3", 0, "Overflow 3", "Third visible toast.", XNOTIFY_ICON_WARNING, NULL, NULL, 0, NULL, 0, 250);
    send_notification("overflow-4", 0, "Overflow 4", "This should evict Overflow 1.", XNOTIFY_ICON_ERROR, NULL, NULL, 0, NULL, 0, 600);

    UINT64 key_first = send_notification("key-first",
                                         362,
                                         "Keyed Toast",
                                         "First version. The id should be reused.",
                                         XNOTIFY_ICON_APP,
                                         NULL,
                                         NULL,
                                         0,
                                         NULL,
                                         0,
                                         700);
    UINT64 key_second = send_notification("key-update",
                                          362,
                                          "Keyed Toast Updated",
                                          "Updated in place. Timer resets and id is stable.",
                                          XNOTIFY_ICON_APP,
                                          NULL,
                                          NULL,
                                          0,
                                          NULL,
                                          0,
                                          700);
    log_u64("msgtest: keyed first id=", key_first);
    log_u64("msgtest: keyed second id=", key_second);

    send_notification("path-icon",
                      0,
                      "Path Icon",
                      "Uses a real resource path when available.",
                      XNOTIFY_ICON_INFO,
                      "/system/icon/computer.png",
                      NULL,
                      0,
                      NULL,
                      0,
                      600);
    send_notification("bad-path-icon",
                      0,
                      "Icon Fallback",
                      "Bad icon path should fall back to warning icon.",
                      XNOTIFY_ICON_WARNING,
                      "/system/icon/does-not-exist.png",
                      NULL,
                      0,
                      NULL,
                      0,
                      600);

    send_notification("actions",
                      0,
                      "Action Toast",
                      "Click either action button; serial log should show distinct action ids.",
                      XNOTIFY_ICON_INFO,
                      NULL,
                      "Action A",
                      1001,
                      "Action B",
                      1002,
                      0);

    send_notification("close-only",
                      0,
                      "Close Button Test",
                      "Click X: it should close without callback.",
                      XNOTIFY_ICON_SUCCESS,
                      NULL,
                      NULL,
                      0,
                      NULL,
                      0,
                      0);

    log_line("msgtest: manual checks: click action buttons and close button, then watch serial output\n");
    xapi_Sleep(9000);
    log_u64("msgtest: callback count=", g_callback_count);
    log_u64("msgtest: last action=", g_last_action_id);
    log_line("msgtest: done\n");
    return 0;
}

extern "C" int msgtest_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int msgtest_main_cpp(int argc, char *argv[], char *envp[])
{
    return msgtest_main_impl(argc, argv, envp);
}
