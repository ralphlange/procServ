// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange <ralph.lange@gmx.de> 2007-2019
// GNU Public License (GPLv3) applies - see www.gnu.org

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>

#include "procServ.h"
#include "processClass.h"
#include "libtelnet.h"

// Wrapper to ignore return values
template<typename T>
inline void ignore_result(T /* unused result */) {}

static const telnet_telopt_t my_telopts[] = {
  { TELNET_TELOPT_ECHO,      TELNET_WILL,           0 },
  { TELNET_TELOPT_LINEMODE,            0, TELNET_DO   },
//  { TELNET_TELOPT_NAOCRD,    TELNET_WILL, 0           },
  { -1, 0, 0 }
};

const char *restartModeString()
{
    switch (restartMode) {
    case restart:   return "ON";
    case norestart: return "OFF";
    case oneshot:   return "ONESHOT";
    }
    return "-"; // make compiler happy
}

class clientItem : public connectionItem
{
public:
    clientItem(int port, bool readonly);
    ~clientItem();

    void readFromFd(void);
    int Send(const char *buf, int len);
    int Send(const char * stamp, int stamp_len,
             const char * message, int count);

private:
    static void telnet_eh(telnet_t *telnet, telnet_event_t *event, void *user_data);
    void processInput(const char *buf, int len);
    void writeToFd(const char *buf, int len);

    telnet_t *_telnet;
    static int _users;
    static int _loggers;
    static int _status;
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
    if (_fd >= 0) {
        shutdown(_fd, SHUT_RDWR);
        close(_fd);
    }
    if (_telnet) telnet_free(_telnet);
    PRINTF("~clientItem(); handle %d closed\n", _fd);
    if (_readonly) _loggers--;
    else _users--;
}

// Client item constructor
// This sets KEEPALIVE on the socket and displays the greeting
// Also sets the socket SNDTIMEO
clientItem::clientItem(int socketIn, bool readonly) :
    connectionItem(socketIn, readonly)
{
    assert(socketIn>=0);
    int optval = 1;
    int i;
    struct tm procServStart_tm; // Time when this procServ started
    char procServStart_buf[32]; // Time when this procServ started - as string
    struct tm IOCStart_tm;      // Time when the current IOC was started
    char IOCStart_buf[32];      // Time when the current IOC was started - as string
#define BUFLEN 512
    char buf1[BUFLEN], buf2[BUFLEN];
    char greeting1[] = "@@@ Welcome to procServ (" PROCSERV_VERSION_STRING ")" NL;
#define GREETLEN 256
    char greeting2[GREETLEN] = "";
    struct timeval send_timeout;
    send_timeout.tv_sec = 10;
    send_timeout.tv_usec = 0;

    PRINTF("New clientItem %p\n", this);
    if ( killChar ) {
        snprintf(greeting2, GREETLEN, "@@@ Use %s%c to kill the child, ", CTL_SC(killChar));
    } else {
        snprintf(greeting2, GREETLEN, "@@@ Kill command disabled, ");
    }
    snprintf(buf1, BUFLEN, "auto restart mode is %s, ", restartModeString());
    if ( toggleRestartChar ) {
        snprintf(buf2, BUFLEN, "use %s%c to toggle auto restart" NL, CTL_SC(toggleRestartChar));
    } else {
        snprintf(buf2, BUFLEN, "auto restart toggle disabled" NL);
    }
    strncat(greeting2, buf1, GREETLEN-strlen(greeting2)-1);
    strncat(greeting2, buf2, GREETLEN-strlen(greeting2)-1);
    if (logoutChar) {
        snprintf(buf2, BUFLEN, "@@@ Use %s%c to logout from procServ server" NL, CTL_SC(logoutChar));
        strncat(greeting2, buf2, GREETLEN-strlen(greeting2)-1);
    }

    localtime_r( &procServStart, &procServStart_tm );
    strftime( procServStart_buf, sizeof(procServStart_buf)-1,
              timeFormat, &procServStart_tm );

    localtime_r( &IOCStart, &IOCStart_tm );
    strftime( IOCStart_buf, sizeof(IOCStart_buf)-1,
              timeFormat, &IOCStart_tm );

    snprintf(buf1, BUFLEN, "@@@ procServ server started at: %s" NL,
             procServStart_buf);

    if ( processClass::exists() ) {
        snprintf(buf2, BUFLEN, "@@@ Child \"%s\" started at: %s" NL,
                 childName, IOCStart_buf );
        strncat(buf1, buf2, BUFLEN-strlen(buf1)-1);
    }

    snprintf(buf2, BUFLEN, "@@@ %d user(s) and %d logger(s) connected (plus you)" NL,
             _users, _loggers);

    setsockopt( socketIn, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval) );
    setsockopt( socketIn, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout) );

    if ( _readonly ) {          // Logging client
        _loggers++;
    } else {                    // Regular (user) client
        _users++;
        ignore_result( write(_fd, greeting1, strlen(greeting1)) );
        ignore_result( write(_fd, greeting2, strlen(greeting2)) );
    }

    ignore_result( write(_fd, infoMessage1, strlen(infoMessage1)) );
    ignore_result( write( _fd, infoMessage2, strlen(infoMessage2)) );
    ignore_result( write( _fd, buf1, strlen(buf1)) );
    if ( ! _readonly )
        ignore_result( write(_fd, buf2, strlen(buf2)) );
    if ( ! processClass::exists() )
        ignore_result( write(_fd, infoMessage3, strlen(infoMessage3)) );

    _telnet = telnet_init(my_telopts, telnet_eh, 0, this);

    for (i = 0; my_telopts[i].telopt >= 0; i++) {
        if (my_telopts[i].him > 0) {
            telnet_negotiate(_telnet, my_telopts[i].him, my_telopts[i].telopt);
        }
        if (my_telopts[i].us > 0) {
            telnet_negotiate(_telnet, my_telopts[i].us, my_telopts[i].telopt);
        }
    }
}

// clientItem::readFromFd
// Reads from the FD, forwards to telnet state machine
void clientItem::readFromFd(void)
{
    char buf[1600];
    int  len;

    len = read(_fd, buf, sizeof(buf)-1);

    if (len == 0) {
        PRINTF("clientItem:: Got EOF reading input connection\n");
        _markedForDeletion = true;
    } else if (len < 0) {
        PRINTF("clientItem:: Got error reading input connection: %s\n", strerror(errno));
        _markedForDeletion = true;
    } else if (!_readonly) {
        buf[len] = '\0';
        telnet_recv(_telnet, buf, len);
    }
}

// clientItem::processInput
// Scans for restart / quit char if in child shut down mode,
// else sends the characters to the other connections
void clientItem::processInput(const char *buf, int len)
{
    int i;
    if (len > 0) {
        // Scan input for commands
        for (i = 0; i < len; i++) {
            if (false == processClass::exists()) {  // We're in child shut down mode
                if ((restartChar && buf[i] == restartChar)
                        || (killChar && buf[i] == killChar)) {
                    PRINTF ("Got a restart command\n");
                    waitForManualStart = false;
                    processClass::restartOnce();
                }
                if (quitChar && buf[i] == quitChar) {
                    PRINTF ("Got a shutdown command\n");
                    shutdownServer = true;
                }
            }
            if (logoutChar && buf[i] == logoutChar) {
                PRINTF ("Got a logout command\n");
                _markedForDeletion = true;
            }
            if (toggleRestartChar && buf[i] == toggleRestartChar) {
                if (restartMode == restart) restartMode = norestart;
                else if (restartMode == norestart) {
                    restartMode = oneshot;
                    firstRun    = true;	// Allow process to run once AFTER selecting oneshot
                }
                else restartMode = restart;
                char msg[128] = NL;
                PRINTF ("Got a toggleAutoRestart command\n");
                SendToAll(msg, strlen(msg), NULL);
                snprintf(msg, 128, "@@@ Toggled auto restart mode to %s" NL, restartModeString());
                SendToAll(msg, strlen(msg), NULL);
            }
            if (killChar && buf[i] == killChar) {
                const char *msg = NL "@@@ Got a kill command" NL;
                PRINTF ("Got a kill command\n");
                SendToAll(msg, strlen(msg), NULL);
                processFactorySendSignal(killSig);
            }
        }
        SendToAll(buf, len, this);
    }
}

// Send characters to telnet state machine
int clientItem::Send(const char * buf, int len)
{
    if (!_markedForDeletion) {
        _status = 0;
        telnet_send(_telnet, buf, len);
    }
    return _status;
}

// Send characters, printing time stamps at every new line
int clientItem::Send(const char * stamp, int stamp_len,
                     const char * message, int count)
{
    if (isLogger()) {
        // Some OSs (Windows) do not support line buffering, so we can get parts of lines,
        // hence need to track of when to send timestamp
        int i = 0, j = 0;
        for (i = 0; i < count; ++i) {
            if (!_log_stamp_sent) {
                Send(stamp, stamp_len);
                _log_stamp_sent = true;
            }
            if (message[i] == '\n') {
                Send(message+j, i-j+1);
                j = i + 1;
                _log_stamp_sent = false;
            }
        }
        return Send(message+j, count-j);  // finish off rest of line with no newline at end
    } else {
        return Send(message, count);
    }
}

// Write characters to client FD
void clientItem::writeToFd(const char * buf, int len)
{
    int status = 0;
    while (-1 == (status = write(_fd, buf, len)) && errno == EINTR);
    if (-1 == status) {
        _markedForDeletion = true;
        _status = status;
    }
}

// Event handler for libtelnet
// this is being called when libtelnet process an input buffer
void clientItem::telnet_eh(telnet_t *telnet, telnet_event_t *event, void *user_data)
{
    clientItem *client = (clientItem *)user_data;

    switch (event->type) {
    case TELNET_EV_DATA:
        client->processInput(event->data.buffer, event->data.size);
        break;
    case TELNET_EV_SEND:
        client->writeToFd(event->data.buffer, event->data.size);
        break;
    case TELNET_EV_ERROR:
        fprintf(stderr, "TELNET error: %s", event->error.msg);
        break;
    default:
        break;
    }
}

int clientItem::_users;
int clientItem::_loggers;
int clientItem::_status;
