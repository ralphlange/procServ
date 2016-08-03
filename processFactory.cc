// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange <ralph.lange@gmx.de> 2007-2016
// GNU Public License (GPLv3) applies - see www.gnu.org

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <strings.h>

// forkpty()
#ifdef HAVE_LIBUTIL_H      // FreeBSD
#include <libutil.h>
#endif
#ifdef HAVE_UTIL_H         // Mac OS X
#include <util.h>
#endif
#ifdef HAVE_PTY_H          // Linux
#include <pty.h>
#endif
#ifndef HAVE_FORKPTY       // Solaris, use our own implementation
extern "C" int forkpty(int*, char*, void*, void*);
#endif

#include "procServ.h"
#include "processClass.h"

#define LINEBUF_LENGTH 1024

processClass * processClass::_runningItem=NULL;
time_t processClass::_restartTime=0;

bool processFactoryNeedsRestart()
{
    time_t now = time(0);
    if ( ( autoRestart == false && processClass::_restartTime ) || 
         processClass::_runningItem ||
         now < processClass::_restartTime ||
         waitForManualStart ) return false;
    return true;
}

connectionItem * processFactory(char *exe, char *argv[])
{
    const size_t BUFLEN = 512;
    char buf[BUFLEN];
    time(&IOCStart); // Remember when we did this

    if (processFactoryNeedsRestart())
    {
    snprintf(buf, BUFLEN, "@@@ Restarting child \"%s\"" NL, childName);
	SendToAll( buf, strlen(buf), 0 );

        if ( strcmp( childName, argv[0] ) != 0 ) {
            snprintf(buf, BUFLEN, "@@@    (as %s)" NL, argv[0]);
            SendToAll( buf, strlen(buf), 0 );
        }

        connectionItem *ci = new processClass(exe, argv);
        PRINTF("Created new child connection (processClass %p)\n", ci);
	return ci;
    }
    else
	return NULL;
}

processClass::~processClass()
{

    struct tm now_tm;
    time_t now;
    size_t result;
    const size_t NOWLEN = 128;
    char now_buf[NOWLEN] = "@@@ Current time: ";
    const size_t BYELEN = 128;
    char goodbye[BYELEN];

    time( &now );
    localtime_r( &now, &now_tm );
    result = strftime( &now_buf[strlen(now_buf)], sizeof(now_buf) - strlen(now_buf) - 1,
                       timeFormat, &now_tm );
    if (result && (sizeof(now_buf) - strlen(now_buf) > 2)) {
        strncat(now_buf, NL, NOWLEN-strlen(now_buf)-1);
    } else {
        strncpy(now_buf, "@@@ Current time: N/A", NOWLEN);
        now_buf[NOWLEN-1] = '\0';
    }
    snprintf (goodbye, BYELEN, "@@@ Child process is shutting down, %s" NL,
              autoRestart ? "a new one will be restarted shortly" :
              "auto restart is disabled");

    // Update client connect message
    snprintf(infoMessage2, INFO2LEN, "@@@ Child \"%s\" is SHUT DOWN" NL, childName);

    SendToAll( now_buf, strlen(now_buf), this );
    SendToAll( goodbye, strlen(goodbye), this );
    SendToAll( infoMessage3, strlen(infoMessage3), this );

                                // Negative PID sends signal to all members of process group
    if ( _pid > 0 ) kill( -_pid, SIGKILL );
    if ( _fd > 0 ) close( _fd );
    _runningItem = NULL;
}


// Process class constructor
// This forks and
//    parent: sets the minimum time for the next restart
//    child:  sets the coresize, becomes a process group leader,
//            and does an execvp() with the command
processClass::processClass(char *exe, char *argv[])
{
    _runningItem=this;
    struct rlimit corelimit;
    const size_t BUFLEN = 128;
    char buf[BUFLEN];

    _pid = forkpty(&_fd, factoryName, NULL, NULL);

    _markedForDeletion = _pid <= 0;
    if (_pid)                               // I am the parent
    {
	if(_pid < 0) {
            fprintf(stderr, "Fork failed: %s\n", errno == ENOENT ? "No pty" : strerror(errno));
        } else {
            PRINTF("Created process %ld on %s\n", (long) _pid, factoryName);
        }

        // Don't start a new one before this time:
        _restartTime = holdoffTime + time(0);

        // Update client connect message
        snprintf(infoMessage2, INFO2LEN, "@@@ Child \"%s\" PID: %ld" NL, childName, (long) _pid);

        snprintf(buf, BUFLEN, "@@@ The PID of new child \"%s\" is: %ld" NL, childName, (long) _pid);
        SendToAll( buf, strlen(buf), this );
        strcpy(buf, "@@@ @@@ @@@ @@@ @@@" NL);
        SendToAll( buf, strlen(buf), this );
    }
    else                                    // I am the child
    {
        setsid();                                  // Become process group leader
        if ( setCoreSize ) {                       // Set core size limit?
            getrlimit( RLIMIT_CORE, &corelimit );
            corelimit.rlim_cur = coreSize;
            setrlimit( RLIMIT_CORE, &corelimit );
        }
        if ( chDir && chdir( chDir ) ) {
            fprintf( stderr, "%s: child could not chdir to %s, %s\n",
                     procservName, chDir, strerror(errno) );
        } else {
            execvp(exe, argv);              // execvp()
        }

	// This shouldn't return, but did...
	fprintf( stderr, "%s: child could not execute: %s, %s\n",
                 procservName, *argv, strerror(errno) );
	exit( -1 );
    }
}

// processClass::readFromFd
// Reads, checks for EOF/Error, and sends to the other connections
void processClass::readFromFd(void)
{
    char  buf[1600];

    int len = read(_fd, buf, sizeof(buf)-1);
    if (len < 1) {
        PRINTF("processItem: Got error reading input connection\n");
        _markedForDeletion = true;
    } else if (len == 0) {
        PRINTF("processItem: Got EOF reading input connection\n");
        _markedForDeletion = true;
    } else {
        buf[len]='\0';
        SendToAll(&buf[0], len, this);
    }
}

// Sanitize buffer, then send characters to child
int processClass::Send( const char * buf, int count )
{
    int status = 0;
    int i, j, ign = 0;
    char buf3[LINEBUF_LENGTH+1];
    char *buf2 = buf3;

                                // Create working copy of buffer
    if ( count > LINEBUF_LENGTH ) buf2 = (char*) calloc (count + 1, 1);
    buf2[0] = '\0';

    if ( ignChars ) {           // Throw out ignored chars
        for ( i = j = 0; i < count; i++ ) {
            if ( index( ignChars, (int) buf[i] ) == NULL ) {
                buf2[j++] = buf[i];
            } else ign++;
        }
    } else {                    // Plain buffer copy
        strncpy (buf2, buf, count);
    }
        buf2[count - ign] = '\0';

    if ( count > 0 )
    {
	status = write( _fd, buf2, count - ign );
	if ( status < 0 ) _markedForDeletion = true;
    }

    if ( count > LINEBUF_LENGTH ) free( buf2 );
    return status;
}

// The telnet state machine can call this to blast a running
// client IOC
void processFactorySendSignal(int signal)
{
    if (processClass::_runningItem)
    {
	PRINTF("Sending signal %d to pid %ld\n",
		signal, (long) processClass::_runningItem->_pid);
	kill(-processClass::_runningItem->_pid,signal);
    }
}

void processClass::restartOnce ()
{
    _restartTime = 0;
}
