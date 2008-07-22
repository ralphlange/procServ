// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange 04/25/2008
// GNU Public License applies - see www.gnu.org


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pty.h>  /* for openpty and forkpty */ 
#include <utmp.h> /* for login_tty */ 
#include <time.h>
#include <string.h>

#include "procServ.h"
#include "processClass.h"

#define LINEBUF_LENGTH 1024

#ifndef MIN_TIME_BETWEEN_RESTARTS
#define MIN_TIME_BETWEEN_RESTARTS 15
#endif


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

connectionItem * processFactory(int argc, char *argv[])
{
    char buf[100];
    time(&IOCStart); // When did we do this?
    
    
    if (processFactoryNeedsRestart())
    {
	sprintf( buf, "@@@ Restarting child \"%s\"" NL, childName );
	SendToAll( buf, strlen(buf), 0 );

        if ( strcmp( childName, argv[0] ) != 0 ) {
            sprintf( buf, "@@@    (as %s)" NL, argv[0] );
            SendToAll( buf, strlen(buf), 0 );
        }

	return new processClass( argc, argv );
    }
    else
	return NULL;
}

processClass::~processClass()
{

    struct tm now_tm;
    time_t now;
    char now_buf[128];
    char goodbye[128];

    time( &now );
    localtime_r( &now, &now_tm );
    strftime( now_buf, sizeof(now_buf) - 1, 
              "@@@ Current time: " STRFTIME_FORMAT NL, &now_tm );
    sprintf ( goodbye, "@@@ Child process is shutting down, %s" NL,
              autoRestart ? "a new one will be restarted shortly" :
              "auto restart is disabled" );

    // Update client connect message
    sprintf( infoMessage2, "@@@ Child \"%s\" is SHUT DOWN" NL, childName );

    SendToAll( now_buf, strlen(now_buf), this );
    SendToAll( goodbye, strlen(goodbye), this );
    if ( ! autoRestart )
        SendToAll( infoMessage3, strlen(infoMessage3), this );

                                // Negative PID sends signal to all members of process group
    if ( _pid > 0 ) kill( -_pid, SIGKILL );
    if ( _ioHandle > 0 ) close( _ioHandle );
    _runningItem = NULL;
}


// Process class constructor
// This forks and
//    parent: sets the minimum time for the next restart
//    child:  sets the coresize, becomes a process group leader,
//            and does an execv() with the command
processClass::processClass(int argc,char * argv[])
{
    _runningItem=this;
    struct termios tio;
    struct rlimit corelimit;
    SetupTio(&tio);
    _pid = forkpty( &_ioHandle, factoryName, &tio, NULL );
    char buf[128];

    _markedForDeletion = _pid <= 0;
    if (_pid) // I am the parent
    {
	PRINTF("Created process %d on %s\n", _pid, factoryName);

	// Don't start a new one before this time:
	_restartTime = MIN_TIME_BETWEEN_RESTARTS + time(0);

        // Update client connect message
	sprintf( infoMessage2, "@@@ Child \"%s\" PID: %d" NL, childName, _pid );

	sprintf( buf, "@@@ The PID of new child \"%s\" is: %d" NL, childName, _pid );
	strcat( buf, "@@@ @@@ @@@ @@@ @@@" NL );
	SendToAll( buf, strlen(buf), this );
    }
    else // I am the child 
    {
	setpgrp();                                 // Become process group leader
        if ( coreSize >= 0 ) {                     // Set core size limit
            getrlimit( RLIMIT_CORE, &corelimit );
            corelimit.rlim_cur = coreSize;
            setrlimit( RLIMIT_CORE, &corelimit );
        }
        if ( chDir && chdir( chDir ) ) {
            fprintf( stderr, "%s: child could not chdir to %s, %s\n",
                     procservName, chDir, strerror(errno) );
        } else {
            execv(*argv,argv);                         // execv()
        }

	// This shouldn't return, but did...
	fprintf( stderr, "%s: child could not execute: %s, %s\n",
                 procservName, *argv, strerror(errno) );
	exit( -1 );
    }
}

// OnPoll is called after a poll returns non-zero in events
// return 0 normally
// may modify pfd-revents as needed
bool processClass::OnPoll()
{
    if (_pfd==NULL || _pfd->revents==0 ) return false;
    // Otherwise process the revents and return true;

    char  buf[1600];

    if (_pfd->revents&(POLLIN|POLLPRI))
    {
	int len=read(_ioHandle,buf,sizeof( buf)-1);
	if (len>=0)
	{
	    buf[len]='\0';
	}
	if (len<1)
	{
	    _markedForDeletion=true;
	}
	else
	{
	    buf[len]='\0';
	    SendToAll(&buf[0],len,this);
	}

    }
    if (_pfd->revents&(POLLHUP|POLLERR))
    {
	PRINTF("ProcessItem:: Got hangup or error \n");
	_markedForDeletion=true;
    }
    if (_pfd->revents&POLLNVAL)
    {
	_markedForDeletion=true;
    }
    return true;
}


// Send characters to clients
int processClass::Send( const char * buf, int count )
{
    int status = 0;
    int i, j, ign = 0;
    char buf3[LINEBUF_LENGTH+1];
    char *buf2 = buf3;

    // Scan input for commands
    for ( i = 0; i < count; i++ ) {
        if ( toggleRestartChar && buf[i] == toggleRestartChar ) {
            autoRestart = ! autoRestart;
            char msg[128];
            sprintf ( msg, NL "@@@ Toggled auto restart to %s" NL,
                      ( autoRestart ? "ON" : "OFF" ) );
            SendToAll ( msg, strlen ( msg ), this );
        }
        if ( killChar && buf[i] == killChar ) {
            PRINTF ("Got a kill command\n");
            processFactorySendSignal( killSig );
        }
    }
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
	status = write( _ioHandle, buf2, count - ign );
	if ( status < 0 ) _markedForDeletion = true;
    }

    if ( count > LINEBUF_LENGTH ) free( buf2 );
    return status;
}

// This gets called if a SIGCHLD was received by the main thread
void processClass::OnWait(pid_t pid)
{

    if (pid==_pid)
    {
	_markedForDeletion=true;
    }
}

// The telnet state machine can call this to blast a running
// client IOC
void processFactorySendSignal(int signal)
{
    if (processClass::_runningItem)
    {
	PRINTF("Sending signal %d to pid %d\n",
		signal,processClass::_runningItem->_pid);
	kill(-processClass::_runningItem->_pid,signal);
    }
}

#define CC(c) (c&0x1f)

void processClass::SetupTio(struct termios *tio)
{
    tio->c_iflag=   IXON|ICRNL ; 
    tio->c_oflag=OPOST|ONLCR|NL0|CR0|TAB0|BS0|FF0|VT0 ;
    tio->c_cflag=   B38400|CS8|CREAD   ;        
    tio->c_lflag= ISIG|ICANON|IEXTEN |ECHO|ECHONL;
    tio->c_line=0; // Line dicipline 0 
    tio->c_cc[VINTR ]=CC('C');
    tio->c_cc[VQUIT ]=CC('\\');
    tio->c_cc[VERASE ]=0x7f;
    tio->c_cc[VKILL ]=CC('U');
    tio->c_cc[VEOF ]=CC('D');
    tio->c_cc[VTIME ]=0;
    tio->c_cc[VMIN ]=1;
    tio->c_cc[VSWTC ]=0;
    tio->c_cc[VSTART ]=CC('Q');
    tio->c_cc[VSTOP ]=CC('S');
    tio->c_cc[VSUSP ]=CC('Z');
    tio->c_cc[VEOL ]=0;
    tio->c_cc[VREPRINT ]=CC('R');
    tio->c_cc[VDISCARD ]=0; // CC('');
    tio->c_cc[VWERASE ]=CC('W');
    tio->c_cc[VLNEXT ]=CC('V');
    tio->c_cc[VEOL2 ]=0;
    tio->c_ispeed=B38400;
    tio->c_ospeed=B38400;
}


void processClass::restartOnce ()
{
    _restartTime = 0;
}
