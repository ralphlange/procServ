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
#include <string.h>
#include <signal.h>

#include "procServ.h"
#include "processClass.h"
#include "telnetStateMachine.h"


class clientItem : public connectionItem
{
public:
    clientItem(int port, bool readonly);
    ~clientItem();

    bool OnPoll();
    int Send( const char *,int count);

private:
    telnetStateMachine _telnet;
    static int _users;
    static int _loggers;
};

// service and calls clientFactory when clients are accepted
connectionItem * clientFactory(int socketIn, bool readonly)
{
    return new clientItem(socketIn, readonly);
}

clientItem::~clientItem()
{
    if ( _ioHandle >= 0 ) close( _ioHandle );
    PRINTF("~clientItem()\n");
    if ( _readonly ) _loggers--;
    else _users--;
}


// Client item constructor
// This sets KEEPALIVE on the socket and displays the greeting
clientItem::clientItem(int socketIn, bool readonly)
{
    assert(socketIn>=0);
    int optval = 1;
    struct tm procServStart_tm; // Time when this procServ started
    char procServStart_buf[32]; // Time when this procServ started - as string
    struct tm IOCStart_tm;      // Time when the current IOC was started
    char IOCStart_buf[32];      // Time when the current IOC was started - as string
    char buf1[128], buf2[128];
    char greeting1[] = "@@@ Welcome to the procServ process server ("
        PROCSERV_VERSION_STRING ")" NL;
    char greeting2[256] = "";

    if ( killChar ) {
        sprintf( greeting2, "@@@ Use %s%c to kill the child, ",
                 killChar < 32 ? "^" : "",
                 killChar < 32 ? killChar + 64 : killChar );
    } else {
        sprintf( greeting2, "@@@ Kill command disabled, " );
    }
    sprintf( buf1, "auto restart is %s, ", autoRestart ? "ON" : "OFF" );
    if ( toggleRestartChar ) {
        sprintf( buf2, "use %s%c to toggle auto restart" NL,
                 toggleRestartChar < 32 ? "^" : "",
                 toggleRestartChar < 32 ? toggleRestartChar + 64 : toggleRestartChar );
    } else {
        sprintf( buf2, "auto restart toggle disabled" NL );
    }
    strcat ( greeting2, buf1 );
    strcat ( greeting2, buf2 );

    localtime_r( &procServStart, &procServStart_tm );
    strftime( procServStart_buf, sizeof(procServStart_buf)-1,
              STRFTIME_FORMAT, &procServStart_tm );

    localtime_r( &IOCStart, &IOCStart_tm );
    strftime( IOCStart_buf, sizeof(IOCStart_buf)-1,
              STRFTIME_FORMAT, &IOCStart_tm );

    sprintf( buf1, "@@@ procServ server started at: %s" NL,
             procServStart_buf);
    
    if ( processClass::exists() ) {
        sprintf( buf2, "@@@ Child \"%s\" started at: %s" NL,
                 childName, IOCStart_buf );
        strcat( buf1, buf2 );
    }

    sprintf( buf2, "@@@ %d user(s) and %d logger(s) connected (plus you)" NL,
             _users, _loggers );

    setsockopt( socketIn, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval) );
    _ioHandle = socketIn;
    _readonly = readonly;

    if ( _readonly ) {          // Logging client
        _loggers++;
    } else {                    // Regular (user) client
        _users++;
        write( _ioHandle, greeting1, strlen(greeting1) );
        write( _ioHandle, greeting2, strlen(greeting2) );
    }

    write( _ioHandle, infoMessage1, strlen(infoMessage1) );
    write( _ioHandle, infoMessage2, strlen(infoMessage2) );
    write( _ioHandle, buf1, strlen(buf1) );
    if ( ! _readonly )
        write( _ioHandle, buf2, strlen(buf2) );
    if ( ! processClass::exists() )
        write( _ioHandle, infoMessage3, strlen(infoMessage3) );

    _telnet.SetConnectionItem( this );
}

// OnPoll is called after a poll returns non-zero in events
// return false if there are no events to be serviced
// return true normally
// may modify pfd-revents as needed
bool clientItem::OnPoll()
{
    char buf[1600];
    int  len;

    if (_pfd==NULL || _pfd->revents==0 ) return false;

    // Otherwise process the revents and return true;

    if (_pfd->revents&(POLLPRI|POLLIN))
    {
	len = read( _ioHandle, buf, sizeof(buf)-1 );
	if (len<1)
	{
	    _markedForDeletion=true;
	}
	else
	{
	    len=_telnet.OnReceive(buf,len);
	}

	if (len>0 && _readonly==false )
	{
            if ( ! processClass::exists() )
            {
                buf[len]='\0';
                int i;

                // Scan input for commands
                for ( i = 0; i < len; i++ ) {
                    if ( restartChar && buf[i] == restartChar ) {
                        PRINTF ("Got a restart command\n");
                        waitForManualStart = false;
                        processClass::restartOnce();
                    }
                    if ( quitChar && buf[i] == quitChar ) {
                        PRINTF ("Got a shutdown command\n");
                        shutdownServer = true;
                    }
                }
            }
	    SendToAll(&buf[0],len,this);
	}
    }
    if (_pfd->revents&(POLLHUP|POLLERR))
    {
	PRINTF("ClientItem:: Got hangup or error \n");
	_markedForDeletion=true;
    }
    if (_pfd->revents&POLLNVAL)
    {
	_markedForDeletion=true;
	_ioHandle=-1;
    }
    return true;
}


// Send characters to client
int clientItem::Send(const char * buf,int count)
{
    int status;

    if (!_markedForDeletion)
    {
	while ( (status=write(_ioHandle,buf,count))==-1 && errno==EINTR);
    }
    if (status==-1) _markedForDeletion=true;
    return status;
}

int clientItem::_users;
int clientItem::_loggers;
