// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange 02/24/2012
// GNU Public License (GPLv3) applies - see www.gnu.org


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

    void readFromFd(void);
    int Send(const char *, int count);

private:
    telnetStateMachine _telnet;
    static int _users;
    static int _loggers;
};

// service and calls clientFactory when clients are accepted
connectionItem * clientFactory(int socketIn, bool readonly)
{
    connectionItem *ci = new clientItem(socketIn, readonly);
    PRINTF("Created new client connection (clientItem %p; read%s)\n",
           ci, readonly?"only":"/write");
    return ci;
}

clientItem::~clientItem()
{
    if ( _fd >= 0 ) close( _fd );
    PRINTF("~clientItem()\n");
    if (_readonly) _loggers--;
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
    char buf1[512], buf2[512];
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
              timeFormat, &procServStart_tm );

    localtime_r( &IOCStart, &IOCStart_tm );
    strftime( IOCStart_buf, sizeof(IOCStart_buf)-1,
              timeFormat, &IOCStart_tm );

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
    _fd = socketIn;
    _readonly = readonly;

    if ( _readonly ) {          // Logging client
        _loggers++;
    } else {                    // Regular (user) client
        _users++;
        write( _fd, greeting1, strlen(greeting1) );
        write( _fd, greeting2, strlen(greeting2) );
    }

    write( _fd, infoMessage1, strlen(infoMessage1) );
    write( _fd, infoMessage2, strlen(infoMessage2) );
    write( _fd, buf1, strlen(buf1) );
    if ( ! _readonly )
        write( _fd, buf2, strlen(buf2) );
    if ( ! processClass::exists() )
        write( _fd, infoMessage3, strlen(infoMessage3) );

    _telnet.SetConnectionItem( this );
}

// clientItem::readFromFd
// Reads from the FD, scans for restart / quit char if in child shut down mode,
// else sends the characters to the other connections
void clientItem::readFromFd(void)
{
    char buf[1600];
    int  len;

    len = read(_fd, buf, sizeof(buf)-1);
    if (len < 1) {
        PRINTF("clientItem:: Got error reading input connection\n");
        _markedForDeletion = true;
    } else if (len == 0) {
        PRINTF("clientItem:: Got EOF reading input connection\n");
        _markedForDeletion=true;
    } else if (len > 0 && _readonly == false ) {
        len = _telnet.OnReceive(buf,len);

        if (processClass::exists() == false) {  // We're in child shut down mode
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
        SendToAll(&buf[0], len, this);
    }
}

// Send characters to client
int clientItem::Send(const char * buf,int count)
{
    int status = 0;

    if (!_markedForDeletion)
    {
	while ( (status=write(_fd,buf,count)) == -1 && errno == EINTR);
    }
    if (status==-1) _markedForDeletion=true;
    return status;
}

int clientItem::_users;
int clientItem::_loggers;
