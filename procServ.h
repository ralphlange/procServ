// Process server for soft ioc
// David H. Thompson 8/29/2003
// GNU Public License applies - see www.gnu.org

#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>

#include <assert.h>
#include <stdio.h>
#include <time.h>

#ifndef PRINTF
#define PRINTF if (inDebugMode) printf
#endif 
#ifndef XPRINTF
#define XPRINTF
#endif 

#define PROCSERV_VERSION       2
#define PROCSERV_REVISION      0
#define PROCSERV_MODIFICATION  0
#define PROCSERV_VERSION_STRING "procServ Version 2.0.0"

extern bool inDebugMode;
extern bool logPortLocal;
extern char *procservName;
extern char *childName;
extern char *ignChars;
extern char infoMessage1[];
extern char infoMessage2[];
extern pid_t procservPid;

#define NL "\r\n"

#ifndef STRFTIME_FORMAT
#define STRFTIME_FORMAT "%c"
#endif

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
connectionItem * processFactory(int argc,char * argv[]);
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
    // Virtual functions:
    virtual ~connectionItem();

   // Functions inplemented in the base class
    //
    // SetPoll is called to fill in a poll prior to calling and OnPoll() is used to
    // process events from sockets and file handles.  
    // SetPoll() called to fill in a pollfd prior to calling poll.
    // the connectionItem must fill in fd and events and return 1 to be polled or
    // set its own _pfd to NULL and return 0
    virtual bool SetPoll(struct pollfd * pfd);
    
    // OnPoll is called after a poll returns non-zero in events
    // return true if the item accepted an event, false otherwise
    // OnPoll will be called regardless of how SetPoll responded and must return true
    // only if _pfd is not null and _pfd->revents!=0
    virtual bool OnPoll() = 0;
    
    // Send characters to clients
    virtual int Send(const char *, int count) = 0;
 
    virtual void OnWait(int pid); // called if sig child received

    int GetHandle() const { return _ioHandle; }
    bool IsDead() const { return _markedForDeletion; }

    // Return false unless you are the process item (processClass overloads)
    virtual bool isProcess() const { return false; }

protected:
    connectionItem ( int fd = -1, bool readonly = false );

    // These two members fill in the pollfd structure
    int _ioHandle; // my file handle
    short _events;
    
    // Keep a copy, poll() fills in revents;
    struct pollfd * _pfd;
    
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
