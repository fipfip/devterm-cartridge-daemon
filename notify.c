#include "notify.h"

#include <errno.h>
#include <libnotify/notify.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utmp.h>

static bool get_user_id(char const *const p_name, int *p_uid, int *p_gid);

static int notify_as(int uid, int gid, const char *const p_title, const char *const p_text,
                     const char *const p_iconpath);

int notify_send_to_all(const char *const p_title, const char *const p_text, const char *const p_iconpath)
{
    // Holds a list of users notified to not send to the same user twice
    char userlist[255] = {0};
    struct utmp *data;
    char aux[50] = {0};
    char userlookup[64] = {0};
    int uid = 0;
    int gid = 0;
    int rc = 0;
    setutent();
    data = getutent();
    while (data != NULL)
    {

        // Make sure to use strncpy
        // because most data fiels in the utmp struct
        // have ___nonstring___ atribute
        strncpy(aux, data->ut_user, 32);
        aux[32] = '\0';
        sprintf(userlookup, "^%s$", aux);
        // Only send if the user hasn't been added to userlist
        if (strstr(userlist, userlookup) == NULL)
        {
            get_user_id(aux, &uid, &gid);
            rc |= notify_as(uid, gid, p_title, p_text, p_iconpath);

            strcat(userlist, "^");
            strcat(userlist, aux);
            strcat(userlist, "$");
        }
        data = getutent();
    }

    return rc;
}

static bool get_user_id(char const *const p_name, int *p_uid, int *p_gid)
{
    bool rc = true;
    struct passwd *p;
    if ((p = getpwnam(p_name)) == NULL)
    {
        rc = false;
    }
    else
    {
        *p_uid = p->pw_uid;
        *p_gid = p->pw_gid;
    }
    return rc;
}

static int notify_as(int uid, int gid, const char *const p_title, const char *const p_text,
                     const char *const p_iconpath)
{
    int rc = 0;
    int status = 0;
    char buf[255] = {0};
    int child = fork();

    if (child == 0)
    {
        setgid(gid);
        setuid(uid);

        putenv("DISPLAY=:0");
        sprintf(buf, "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/%d/bus", uid);
        putenv(buf);

        notify_init("DevTerm Cartridge Daemon");
        NotifyNotification *n = notify_notification_new(p_title, p_text, p_iconpath);
        notify_notification_set_timeout(n, 10000); // 10 seconds
        if (!notify_notification_show(n, 0))
        {
            rc = -1;
        }
        exit(0);
    }
    else if (child > 0)
    {
        waitpid(child, &status, WEXITED);
        putenv("DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/0/bus");
    }
    else
    {
        rc = -1;
    }

    return rc;
}
