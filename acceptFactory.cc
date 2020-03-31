// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange <ralph.lange@gmx.de> 2007-2016
// Freddie Akeroyd 2016
// Michael Davidsaver 2017
// GNU Public License (GPLv3) applies - see www.gnu.org

#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <sstream>

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
    virtual ~acceptItemTCP() {}

    sockaddr_in addr;

    virtual void remakeConnection();

    virtual void writeAddress(std::ostream& fp) {
        char buf[40] = "";
        inet_ntop(addr.sin_family, &addr.sin_addr, buf, sizeof(buf));
        buf[sizeof(buf)-1] = '\0';
        fp<<"tcp:"<<buf<<":"<<ntohs(addr.sin_port)<<"\n";
    }

    virtual void writeAddressEnv(std::ostringstream& env_var) {
        char buf[40] = "";
        inet_ntop(addr.sin_family, &addr.sin_addr, buf, sizeof(buf));
        buf[sizeof(buf)-1] = '\0';
        if(_readonly) {
            env_var<<"LOG=";
        } else {
            env_var<<"CTL=";
        }
        env_var<<"tcp:"<<buf<<":"<<ntohs(addr.sin_port)<<";";
    }
};

#ifdef USOCKS
struct acceptItemUNIX : public acceptItem
{
    acceptItemUNIX(const char* path, bool readonly);
    virtual ~acceptItemUNIX();

    sockaddr_un addr;
    socklen_t addrlen;
    uid_t uid;
    gid_t gid;
    unsigned perms;
    bool abstract;

    virtual void remakeConnection();

    virtual void writeAddress(std::ostream& fp) {
        if(abstract) {
            fp<<"unix:@"<<&addr.sun_path[1]<<"\n";
        } else {
            fp<<"unix:"<<addr.sun_path<<"\n";
        }
    }

    virtual void writeAddressEnv(std::ostringstream& env_var) {
        if(_readonly) {
            env_var<<"LOG=";
        } else {
            env_var<<"CTL=";
        }
        if(abstract) {
            env_var<<"unix:@"<<&addr.sun_path[1]<<";";
        } else {
            env_var<<"unix:"<<addr.sun_path<<";";
        }
    }
};
#endif

// service and calls clientFactory when clients are accepted
connectionItem * acceptFactory (const char *spec, bool local, bool readonly)
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
        if(port>0xffff) {
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
        inet_addr.sin_addr.s_addr = htonl(local ? INADDR_LOOPBACK : (A[0]<<24 | A[1]<<16 | A[2]<<8 | A[3]));
        inet_addr.sin_port = htons(port);
        if(port>0xffff) {
            fprintf( stderr, "%s: invalid control port %d (<1024)\n",
                     procservName, port );
            exit(1);
        }
        connectionItem *ci = new acceptItemTCP(inet_addr, readonly);
        return ci;
    } else if(strncmp(spec, "unix:", 5)==0) {
#ifdef USOCKS
        connectionItem *ci = new acceptItemUNIX(spec+5, readonly);
        return ci;
#else
        fprintf(stderr, "Unix sockets not supported on this host\n");
        exit(1);
#endif
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

    remakeConnection();
    PRINTF("Created new telnet TCP listener (acceptItem %p) at %s:%d (read%s)\n",
           this, myname, ntohs(addr.sin_port), readonly?"only":"/write");
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

    socklen_t slen = sizeof(addr);
    getsockname(_fd, (struct sockaddr *) &addr, &slen);

    PRINTF("Listening on fd %d\n", _fd);
    return; 
}

#ifdef USOCKS
acceptItemUNIX::acceptItemUNIX(const char *path, bool readonly)
    :acceptItem(readonly)
    ,uid(getuid())
    ,gid(getgid())
    ,perms(0666) // default permissions equivalent to tcp bind to localhost
    ,abstract(false)
{
    /* must be one of
     *  "/path/sock"
     *  "user:grp:perm:/path/sock"
     *  "@/path/of/sock"  # abstract socket
     */
    std::string spec(path);
    size_t sep(spec.find_first_of(':'));

    if(sep!=spec.npos) {
        size_t sep2(spec.find_first_of(':', sep+1)),
               sep3(spec.find_first_of(':', sep2+1));
        if(sep2==spec.npos || sep3==spec.npos) {
            fprintf(stderr, "Unix path+permissions spec unparsable '%s'\n", path);
            exit(1);
        }
        std::string user(spec.substr(0,sep)),
                    grp (spec.substr(sep+1, sep2-sep-1)),
                    perm(spec.substr(sep2+1, sep3-sep2-1));

        if(!user.empty()) {
            struct passwd *uinfo = getpwnam(user.c_str());
            if(!uinfo) {
                fprintf(stderr, "Unknown user '%s'\n", user.c_str());
                exit(1);
            }
            uid = uinfo->pw_uid;
            gid = uinfo->pw_gid;
        }

        if(!grp.empty()) {
            struct group *ginfo = getgrnam(grp.c_str());
            if(!ginfo) {
                fprintf(stderr, "Unknown group '%s'\n", grp.c_str());
                exit(1);
            }
            gid =ginfo->gr_gid;
        }

        if(!perm.empty()) {
            perms = strtoul(perm.c_str(), NULL, 8);
        }

        spec = spec.substr(sep3+1);
    }

    if(spec.empty()) {
        fprintf(stderr, "expected unix socket path spec.\n");
        exit(1);
    }

    /* Abstract unix sockets are a linux specific feature whereby
     * the socket has a "path" name associated with it, but no presence
     * in the filesystem (and no associated permission check).
     * Analogous to IP bound to localhost with a name string
     * instead of a port number.
     * We denote an abstract socket with a leading '@'.
     */
    abstract = spec[0]=='@';
#ifndef __linux__
    if(abstract) {
        fprintf(stderr, "Abstract unix sockets not supported by this host\n");
        exit(1);
    }
#endif

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if(spec.size()>=sizeof(addr.sun_path)) {
        fprintf(stderr, "Unix path is too long (must be <%zu)\n", sizeof(addr.sun_path));
        exit(1);
    }

    // see unix(7) man-page
    addrlen = offsetof(struct sockaddr_un, sun_path) + spec.size() + 1;

    memcpy(addr.sun_path, spec.c_str(), spec.size()+1);

    PRINTF("Created new telnet UNIX listener (acceptItem %p) at '%s' (read%s)\n",
           this, addr.sun_path, readonly?"only":"/write");

    /* signal an abstract socket with a *leading* nil.
     * We replace the '@'
     * and reduce addrlen to *not* include the trailing nil.
     * To quote unix(7) manpage
     *   "Null  bytes in the name have no special significance."
     */
    if(abstract) {
        addr.sun_path[0] = '\0';
        addrlen--;
    }

    remakeConnection();
}


acceptItemUNIX::~acceptItemUNIX()
{
    if(!abstract)
        unlink(addr.sun_path);
}

void acceptItemUNIX::remakeConnection()
{
    int bindStatus;

    if(!abstract && unlink(addr.sun_path) < 0) {
        if(errno!=ENOENT) {
            PRINTF("Failed to remove unix socket at '%s' : %s\n", addr.sun_path, strerror(errno));
            throw errno; // Nooooo!
        }
    }

    _fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (_fd < 0) {
        PRINTF("Socket error: %s\n", strerror(errno));
        throw errno;
    }

    bindStatus = bind(_fd, (struct sockaddr *) &addr, addrlen);
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

    if(!abstract) {
        if(chmod(addr.sun_path, 0)<0)
            PRINTF("Can't chmod %u unix socket : %s\n", 0, strerror(errno));

        if(chown(addr.sun_path, uid, gid))
            PRINTF("Can't chown %u:%u unix socket : %s\n", uid, gid, strerror(errno));

        if(chmod(addr.sun_path, perms)<0)
            PRINTF("Can't chmod %u unix socket : %s\n", perms, strerror(errno));
    }

    PRINTF("Listening on fd %d\n", _fd);
    return;
}

#endif

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
