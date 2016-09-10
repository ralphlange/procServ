// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange <ralph.lange@gmx.de> 2007-2016
// Freddie Akeroyd 2016
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

#include "procServ.h"

struct acceptItem : public connectionItem
{
    acceptItem(bool readonly) :connectionItem(-1, readonly) {}
    virtual ~acceptItem();

    void readFromFd(void);
    int Send(const char *, int);

    virtual void remakeConnection()=0;
};

struct acceptItemTCP : public acceptItem
{
    acceptItemTCP(const sockaddr_in& addr, bool readonly);

    sockaddr_in addr;

    virtual void remakeConnection();
};

// service and calls clientFactory when clients are accepted
connectionItem * acceptFactory (const char *spec, bool local, bool readonly )
{
    char junk;
    unsigned port = 0;
    unsigned A[4];
    sockaddr_in inet_addr;

    memset(&inet_addr, 0, sizeof(inet_addr));

    if(sscanf(spec, "%u %c", &port, &junk)==1) {
        // simple integer is TCP port number
        inet_addr.sin_family = AF_INET;
        inet_addr.sin_addr.s_addr = htonl(local ? INADDR_LOOPBACK : INADDR_ANY);
        inet_addr.sin_port = htons(port);
        if(port<1024 || port>0xffff) {
            fprintf( stderr, "%s: invalid control port %d (<1024)\n",
                     procservName, port );
            exit(1);
        }
        connectionItem *ci = new acceptItemTCP(inet_addr, readonly);
        return ci;
    } else if(sscanf(spec, "%u . %u . %u . %u : %u %c",
                     &A[0], &A[1], &A[2], &A[3], &port, &junk)==5) {
        // bind to specific interface and port
        inet_addr.sin_family = AF_INET;
        inet_addr.sin_addr.s_addr = htonl(A[0]<<24 | A[1]<<16 | A[2]<<8 | A[3]);
        inet_addr.sin_port = htons(port);
        if(port<1024 || port>0xffff) {
            fprintf( stderr, "%s: invalid control port %d (<1024)\n",
                     procservName, port );
            exit(1);
        }
        connectionItem *ci = new acceptItemTCP(inet_addr, readonly);
        return ci;
    } else {
        fprintf(stderr, "Invalid socket spec '%s'\n", spec);
        exit(1);
    }
}

acceptItem::~acceptItem()
{
    if (_fd >= 0) close(_fd);
    PRINTF("~acceptItem()\n");
}


// Accept item constructor
// This opens a socket, binds it to the decided port,
// and sets it to listen mode
acceptItemTCP::acceptItemTCP(const sockaddr_in &addr, bool readonly)
    :acceptItem(readonly)
    ,addr(addr)
{
    char myname[128] = "<unknown>\0";

    inet_ntop(AF_INET, &addr.sin_addr, myname, sizeof(myname)-1);
    myname[sizeof(myname)-1] = '\0';

    PRINTF("Created new telnet TCP listener (acceptItem %p) at %s:%d (read%s)\n",
           this, myname, ntohs(addr.sin_port), readonly?"only":"/write");
    remakeConnection();
}

void acceptItemTCP::remakeConnection()
{
    int optval = 1;
    int bindStatus;

    _fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (_fd < 0) {
        PRINTF("Socket error: %s\n", strerror(errno));
        throw errno;
    }

    setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
#ifdef SOLARIS
    setsockopt(_fd, SOL_SOCKET, SO_EXCLBIND, &optval, sizeof(optval));
#endif
#ifdef _WIN32
    setsockopt(_fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, &optval, sizeof(optval));
#endif

    bindStatus = bind(_fd, (struct sockaddr *) &addr, sizeof(addr));
    if (bindStatus < 0)
    {
        PRINTF("Bind error: %s\n", strerror(errno));
        throw errno;
    }
    else
        PRINTF("Bind returned %d\n", bindStatus);

    if (listen(_fd, 5) < 0) {
        PRINTF("Listen error: %s\n", strerror(errno));
        throw errno;
    }

    PRINTF("Listening on fd %d\n", _fd);
    return; 
}

// Accept connection and create a new connectionItem for it.
void acceptItem::readFromFd(void)
{
    int newFd;
    struct sockaddr addr;
    socklen_t len = sizeof(addr);

    newFd = accept( _fd, &addr, &len );
    if (newFd >= 0) {
        PRINTF("acceptItem: Accepted connection on handle %d\n", newFd);
        AddConnection(clientFactory(newFd, _readonly));
    } else {
        PRINTF("Accept error: %s\n", strerror(errno)); // on Cygwin got error EINVAL
        remakeConnection();
    }
}

// Send characters to client
int acceptItem::Send (const char * buf, int count)
{
    // Makes no sense to send to the listening socket
    return true;
}
