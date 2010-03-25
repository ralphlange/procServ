/* Process server for soft ioc
 * Ralph Lange 03/25/2010
 * GNU Public License (GPLv3) applies - see www.gnu.org */

#include <unistd.h>
#include <stropts.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

int forkpty(int* fdm, char* name, void* x1, void* x2)
{
    /* From the Solaris manpage for pts(7D) */
    int   ptm, pts;
    pid_t pid;
    char* c;

    ptm = open("/dev/ptmx", O_RDWR);    /* open master */
    grantpt(ptm);                       /* change permission of slave */
    unlockpt(ptm);                      /* unlock slave */
    c = ptsname(ptm);                   /* get name of slave */
    if (c) strcpy(name, c);
    pts = open(name, O_RDWR);           /* open slave */
    ioctl(pts, I_PUSH, "ptem");         /* push ptem */
    ioctl(pts, I_PUSH, "ldterm");       /* push ldterm */

    /* From forums.sun.com */
    pid = fork();
    if (!pid) {                     /* child */
        close(ptm);
        dup2(pts, 0);               /* connect to slave fd */
        dup2(pts, 1);
        dup2(pts, 2);
    } else {
        *fdm = ptm;                 /* return fd to parent */
    }
    close(pts);
    return pid;
}
