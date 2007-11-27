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


class acceptItem : public connectionItem
{
public:
    acceptItem(int port);
    bool OnPoll();
    int Send( const char *,int);

public:
    virtual ~acceptItem();
};

// service and calls clientFactory when clients are accepted
connectionItem * acceptFactory(char * port)
{
    PRINTF("Creating new acceptItem %s\n",port);
    return new acceptItem(atoi(port));
}

acceptItem::~acceptItem()
{
    if (_ioHandle>=0) close(_ioHandle);
    PRINTF("~acceptItem()\n");
}


// Accept item constuctor
// This opens a socket and binds it to the decided port
acceptItem::acceptItem(int port)
{
    int optval;

    _ioHandle=socket(PF_INET,SOCK_STREAM,IPPROTO_TCP);
    assert(_ioHandle>0);

    optval=1;
    setsockopt(_ioHandle,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval));

    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    inet_aton("127.0.0.1", &addr.sin_addr);


    int bindStatus;
    bindStatus=bind(_ioHandle,(struct sockaddr *) &addr,sizeof addr);
    if (bindStatus<0)
    {
	PRINTF("Bind: %s\n",strerror(errno));
	// exit(-1);
	throw errno;
    }
    else
	PRINTF("Bind returned %d\n",bindStatus);


    listen(_ioHandle,5);
    return; 
}

// OnPoll is called after a poll returns non-zero in events
bool acceptItem::OnPoll()
{
    if (_pfd==NULL || _pfd->revents==0 ) return false;
    // Otherwise process the revents and return true;
    
    int newSocket;
    struct sockaddr addr;
    socklen_t len=sizeof addr;
    struct sockaddr_in  * inaddr  =(sockaddr_in *) & addr;

    

    if (_pfd->revents&(POLLIN|POLLPRI))
    {
	newSocket=accept(_ioHandle, &addr, & len);
	PRINTF("accepted connection on handle %d\n",newSocket);
	AddConnection(clientFactory(newSocket));
    }
    if (_pfd->revents&(POLLHUP|POLLERR))
    {
	PRINTF("acceptItem: Got hangup or error \n");
    }
    if (_pfd->revents&POLLNVAL)
    {
	_ioHandle=-1;
	_markedForDeletion=true;
    }
    return true;
}


// Send Send characters to clients
int acceptItem::Send( const char * buf,int count)
{
    // Makes no sense to send to the listening socket
    return 0; 

}

