// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange 02/28/2012
// GNU Public License (GPLv3) applies - see www.gnu.org


#ifndef procServH
#define procServH

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <poll.h>

#include <assert.h>
#include <stdio.h>
#include <time.h>

#ifndef PRINTF
#define PRINTF if (inDebugMode) printf
#endif

#define PROCSERV_VERSION_STRING PACKAGE_STRING

extern bool   inDebugMode;
extern bool   logPortLocal;
extern bool   autoRestart;
extern bool   waitForManualStart;
extern bool   shutdownServer;
extern bool   setCoreSize;
extern char   *procservName;
extern char   *childName;
extern char   *ignChars;
extern char   *timeFormat;
extern char   killChar;
extern char   toggleRestartChar;
extern char   restartChar;
extern char   quitChar;
extern char   logoutChar;
extern int    killSig;
extern char   infoMessage1[];
extern char   infoMessage2[];
extern char   infoMessage3[];
extern pid_t  procservPid;
extern rlim_t coreSize;
extern char   *chDir;
extern time_t holdoffTime;

#define NL "\r\n"

#define CTL_SC(c) c > 0 && c < 32 ? "^" : "", c > 0 && c < 32 ? c + 64 : c

class connectionItem;

extern time_t procServStart; // Time when this IOC started
extern time_t IOCStart;      // Time when the current IOC was started

// Connection items call this to send messages to others
// This is a party line system, messages go to everyone
// the sender's this pointer keeps it from getting its own
// messages.
//
void SendToAll(const char * message,int count,const connectionItem * sender);
// Call this to add the item to the list of connections
void AddConnection(connectionItem *);
void DeleteConnection(connectionItem *ci);

// connectionItems are made in class factories so none of the
// constructors are public:

// processFactory creates the process that we are managing
connectionItem * processFactory(char *exe, char *argv[]);
bool processFactoryNeedsRestart(); // Call to test status of the server process
void processFactorySendSignal(int signal);

// clientFactory manages an open socket connected to a user
connectionItem * clientFactory(int ioSocket, bool readonly=false);

// acceptFactory opens a socket creating the inital listening
// service and calls clientFactory when clients are accepted
// local: restrict to localhost (127.0.0.1)
// readonly: discard any input from the client
connectionItem * acceptFactory( int port, bool local=true, bool readonly=false );

extern connectionItem * processItem; // Set if it exists
 

// connectionItem class definition
// This is an abstract class that all of the other classes use
// 
class connectionItem
{
public:
    virtual ~connectionItem();

    // Called from main() when input from this client is ready to be read.
    virtual void readFromFd(void) = 0;

    // Send characters to this client.
    virtual int Send(const char *, int count) = 0;

    virtual void markDeadIfChildIs(pid_t pid);   // called if parent receives sig child

    int getFd() const { return _fd; }
    bool IsDead() const { return _markedForDeletion; }

    // Return false unless you are the process item (processClass overloads)
    virtual bool isProcess() const { return false; }
    virtual bool isLogger() const { return _readonly; }

protected:
    connectionItem ( int fd = -1, bool readonly = false );
    int _fd;                 // File descriptor of this connection
    bool _markedForDeletion; // True if this connection is dead
    bool _readonly;          // True if input has to be ignored

public:
    connectionItem * next,*prev;
    static connectionItem *head;

private:
    // This should never happen
    connectionItem(const connectionItem & item)
    {
        assert(0);
    };
};

#endif /* #ifndef procServH */
