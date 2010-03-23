// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange 03/23/2010
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

#ifndef SO_REUSEPORT               // Linux doesn't know SO_REUSEPORT
#define SO_REUSEPORT SO_REUSEADDR
#endif

#include "procServ.h"

class acceptItem : public connectionItem
{
public:
    acceptItem ( int port, bool local, bool readonly );
    void readFromFd(void);
    int Send ( const char *, int );

public:
    virtual ~acceptItem();
};

// service and calls clientFactory when clients are accepted
connectionItem * acceptFactory ( int port, bool local, bool readonly )
{
    connectionItem *ci = new acceptItem(port, local, readonly);
    PRINTF("Created new telnet listener (acceptItem %p) on port %d (read%s)\n",
           ci, port, readonly?"only":"/write");
    return ci;
}

acceptItem::~acceptItem()
{
    if (_fd >= 0) close(_fd);
    PRINTF("~acceptItem()\n");
}


// Accept item constructor
// This opens a socket, binds it to the decided port,
// and sets it to listen mode
acceptItem::acceptItem(int port, bool local, bool readonly)
{
    int optval = 1;
    struct sockaddr_in addr;
    int bindStatus;

    _readonly = readonly;

    _fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(_fd>0);

    setsockopt(_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if ( local )
        inet_aton( "127.0.0.1", &addr.sin_addr );
    else 
        addr.sin_addr.s_addr = htonl( INADDR_ANY );

    bindStatus = bind(_fd, (struct sockaddr *) &addr, sizeof(addr));
    if (bindStatus < 0)
    {
        PRINTF("Bind error: %s\n", strerror(errno));
        throw errno;
    }
    else
        PRINTF("Bind returned %d\n", bindStatus);

    PRINTF("Listening on fd %d\n", _fd);
    listen(_fd, 5);
    return; 
}

// Accept connection and create a new connectionItem for it.
void acceptItem::readFromFd(void)
{
    int newFd;
    struct sockaddr addr;
    socklen_t len = sizeof(addr);

    newFd = accept( _fd, &addr, &len );
    PRINTF( "acceptItem: Accepted connection on handle %d\n", newFd );
    AddConnection( clientFactory(newFd, _readonly) );
}

// Send characters to client
int acceptItem::Send (const char * buf, int count)
{
    // Makes no sense to send to the listening socket
    return true;
}
