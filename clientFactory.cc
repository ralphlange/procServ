// Process server for soft ioc
// David H. Thompson 8/29/2003
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
    char * greeting = "Welcome to the EPICS soft IOC process server! "
        "(procServ $Name: R1-5 $)" NL
    	"Use ^] to quit telnet, ^X<CR> to reboot the IOC." NL;

    localtime_r(&procServStart,&procServStart_tm);
    strftime(procServStart_buf,sizeof(procServStart_buf)-1,"%b %d, %Y %r",&procServStart_tm);
    localtime_r(&IOCStart,&IOCStart_tm);
    strftime(IOCStart_buf,sizeof(IOCStart_buf)-1,"%b %d, %Y %r",&IOCStart_tm);

    sprintf( buf1, "Restarted: ProcServ %s  IOC %s" NL, procServStart_buf, IOCStart_buf );
    sprintf( buf2, "Connected: %d user(s), %d logger(s)" NL NL, _users, _loggers );

    setsockopt(socketIn,SOL_SOCKET,SO_KEEPALIVE,&optval,sizeof(optval));
    _ioHandle = socketIn;
    _readonly = readonly;

    if ( _readonly ) {          // Logging client
        _loggers++;
    } else {                    // Regular (user) client
        _users++;
        write( _ioHandle, greeting,strlen(greeting));
    }

    write( _ioHandle, infoMessage1,strlen(infoMessage1));
    write( _ioHandle, infoMessage2,strlen(infoMessage2));
    write( _ioHandle, buf1, strlen(buf1));
    write( _ioHandle, buf2, strlen(buf2));

    _telnet.SetConnectionItem(this);
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
	    buf[len]='\0';
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
