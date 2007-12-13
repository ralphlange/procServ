// Process server for soft ioc
// David H. Thompson 8/29/2003
// GNU Public License applies - see www.gnu.org

#include "procServ.h"
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
#include "telnetStateMachine.h"


class clientItem : public connectionItem
{
public:
    clientItem(int port);
    bool OnPoll();
    int Send( const char *,int count);
    static int _users;

public:
    virtual ~clientItem();
private:
    telnetStateMachine _telnet;
};
//
// service and calls clientFactory when clients are accepted
connectionItem * clientFactory(int socketIn)
{
    return new clientItem(socketIn);
}

clientItem::~clientItem()
{
    if (_ioHandle>=0) close(_ioHandle);
    PRINTF("~clientItem()\n");
    _users--;
}


// Client item constructor
// This sets KEEPALIVE on the socket and displays the greeting
clientItem::clientItem(int socketIn)
{
    assert(socketIn>=0);
    int optval=1;
    //unsigned char opt[]={IAC,WONT,TELOPT_LINEMODE};

    setsockopt(socketIn,SOL_SOCKET,SO_KEEPALIVE,&optval,sizeof(optval));

    _ioHandle=socketIn;
    char buf[128];
    char * greeting="Welcome to the epics process server! (procServ $Name: R1-5 $)" NL
    	"Use ^] to quit telnet, and ^X<CR> to reboot the IOC." NL ;

    struct tm procServStart_tm; // Time when this IOC started
    char procServStart_buf[32]; // Time when this IOC started
    struct tm IOCStart_tm; // Time when the current IOC was started
    char IOCStart_buf[32]; // Time when the current IOC was started

    localtime_r(&procServStart,&procServStart_tm);
    strftime(procServStart_buf,sizeof(procServStart_buf)-1,"%b %d, %Y %r",&procServStart_tm);
    localtime_r(&IOCStart,&IOCStart_tm);
    strftime(IOCStart_buf,sizeof(IOCStart_buf)-1,"%b %d, %Y %r",&IOCStart_tm);

    _users++;

    //write(_ioHandle,opt,sizeof(opt));
    write(_ioHandle,greeting,strlen(greeting));
    write(_ioHandle,infoMessage1,strlen(infoMessage1));
    write(_ioHandle,infoMessage2,strlen(infoMessage2));
    sprintf(buf,"Restarts: ProcServ:%s IOC:%s" NL,procServStart_buf, IOCStart_buf);
    write(_ioHandle,buf,strlen(buf));
    sprintf(buf,"Connected users=%d" NL NL ,_users);
    write(_ioHandle,buf,strlen(buf));
    _telnet.SetConnectionItem(this);
}

// OnPoll is called after a poll returns non-zero in events
// return false if the object should be deleted.
// return true normally
// may modify pfd-revents as needed
bool clientItem::OnPoll()
{
    if (_pfd==NULL || _pfd->revents==0 ) return false;
    // Otherwise process the revents and return true;
    
    int newSocket;
    struct sockaddr addr;
    socklen_t len=sizeof addr;
    struct sockaddr_in  * inaddr  =(sockaddr_in *) & addr;
    char  buf[1600];

    if (_pfd->revents&(POLLPRI))
    {
	int len=recv(_ioHandle,buf,sizeof( buf)-1,0);
	if (len<1)
	{
	    _markedForDeletion=true;
	}
	else
	{
	    len=_telnet.OnReceive(buf,len);
	}

	if (len>0)
	{
	    buf[len]='\0';
	    SendToAll(&buf[0],len,this);
	}
    }
    else if (_pfd->revents&(POLLIN))
    {
	int len=recv(_ioHandle,buf,sizeof( buf)-1,0);
	if (len<1)
	{
	    _markedForDeletion=true;
	}
	else
	{
	    len=_telnet.OnReceive(buf,len);
	}
	if (len>0)
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


// Send characters to clients
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
