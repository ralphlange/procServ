// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange <ralph.lange@gmx.de> 2007-2019
// Ambroz Bizjak 02/29/2016
// Freddie Akeroyd 2016
// Michael Davidsaver 2017
// Hinko Kocevar 2018
// Klemen Vodopivec 2019

// GNU Public License (GPLv3) applies - see www.gnu.org

#include <vector>
#include <string>
#include <fstream>
#include <sstream>

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/wait.h> 
#include <signal.h>
#include <unistd.h> 
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <string.h>

#ifdef __CYGWIN__
    #include <windows.h>
#endif /* __CYGWIN__ */

#include "procServ.h"

// Wrapper to ignore return values
template<typename T>
inline void ignore_result(T /* unused result */) {}

#ifdef ALLOW_FROM_ANYWHERE
const bool enableAllow = true;   // Enable --allow option
#else
const bool enableAllow = false;  // Default: NO
#endif

bool   inDebugMode;              // This enables a lot of printfs
bool   inFgMode = false;         // This keeps child in the foreground, tty connected
bool   logPortLocal;             // This restricts log port access to localhost
bool   ctlPortLocal = true;      // Restrict control connections to localhost
bool   waitForManualStart = false;  // Waits for telnet cmd to manually start child
volatile bool shutdownServer = false;   // To keep the server from shutting down
bool   quiet = false;            // Suppress info output (server)
bool   setCoreSize = false;      // Set core size for child
bool   singleEndpointStyle = true;  // Compatibility style: first non-option is endpoint
RestartMode restartMode = restart;  // Child restart mode (restart/norestart/oneshot)
char   *procservName;            // The name of this beast (server)
char   *childName;               // The name of that beast (child)
char   *childExec;               // Exec to run as child
char   **childArgv;              // Argv for child process
int    connectionNo;             // Total number of connections
char   *ignChars = NULL;         // Characters to ignore
char   killChar = 0x18;          // Kill command character (default: ^X)
char   toggleRestartChar = 0x14; // Toggle autorestart character (default: ^T)
char   restartChar = 0x12;       // Restart character (default: ^R)
char   quitChar = 0x11;          // Quit character (default: ^Q)
char   logoutChar = 0x00;        // Logout client connection character (default: none)
int    killSig = SIGKILL;        // Kill signal (default: SIGKILL)
rlim_t coreSize;                 // Max core size for child
char   *chDir;                   // Directory to change to before starting child
char   *myDir;                   // Directory where server was started
time_t holdoffTime = 15;         // Holdoff time between child restarts (in seconds)
int    childExitCode = 0;        // Child's exit code

pid_t  procservPid;              // PID of server (daemon if not in debug mode)
char   *pidFile;                 // File name for server PID
const char *timeFormat = "%c";       // Time format string
char   defaulttimeFormat[] = "%c";    // default
bool   stampLog = false;         // Prefix log lines with time stamp
const char *stampFormat;             // Log time stamp format string

const size_t INFO1LEN = 512;
const size_t INFO2LEN = 128;
const size_t INFO3LEN = 128;

char   infoMessage1[INFO1LEN];   // Sign on message: server PID, child pwd and command line
char   infoMessage2[INFO2LEN];   // Sign on message: child PID
char   infoMessage3[INFO3LEN];   // Sign on message: available server commands

char   *logFile = NULL;          // File name for log
int    logFileFD=-1;             // FD for log file
char  *logPort;                  // address for logger connections
int    debugFD=-1;               // FD for debug output

#define MAX_CONNECTIONS 64

// mLoop runs the program
void mLoop();
// Handles houskeeping
void OnPollTimeout();
// Daemonizes the program
void forkAndGo();
void openLogFile();
void setEnvVar();
void writeInfoFile(const std::string& infofile);
void ttySetCharNoEcho(bool save);

// Signal handlers
static void OnSigPipe(int);
static void OnSigTerm(int);
static void OnSigHup(int);

// Flags used for communication between sig handler and main()
static volatile sig_atomic_t sigPipeSet;
static volatile sig_atomic_t sigTermSet;
static volatile sig_atomic_t sigHupSet;

void writePidFile()
{
    int pid = getpid();
    FILE * fp;

    if ( !pidFile || strlen(pidFile) == 0 )
        return;

    PRINTF("Writing PID file %s\n", pidFile);

    fp = fopen( pidFile, "w" );
    // Don't stop here - just go without
    if (fp == NULL) {
        fprintf( stderr,
                 "%s: unable to open PID file %s\n",
                 procservName, pidFile );
        return;
    }
    fprintf(fp, "%d\n", pid);
    fclose(fp);
}

char getOptionChar ( const char* buf ) 
{
    if ( buf == NULL || buf[0] == 0 ) return 0;
    if ( buf[0] == '^' && buf[1] == '^' ) {
        return '^';
    } else if ( buf[0] == '^' && buf[1] >= 'A' && buf[1] <= 'Z' ) {
        return buf[1] - 64;
    } else {
        return buf[0];
    }
}

void printUsage()
{
    printf("Usage: %s [options] -P <endpoint>... <command args ...>    (-h for help)\n"
           "       %s [options] <endpoint> <command args ...>\n",
           procservName, procservName);
}

void printHelp()
{
    printUsage();
    printf("<endpoint>:          endpoint to use for control connections\n"
           "    <port>           TCP <port> on local/all interfaces (see --allow/--restrict)\n"
           "    <iface>:<port>   TCP <port> on specific IP <iface> (numeric)\n"
           "    unix:<path>      UNIX domain socket at <path> (@... for abstract)\n"
           "<command args ...>   command line to start child process\n"
           "Options:\n"
           "    --allow               allow control connections from anywhere\n"
           "    --autorestartcmd      command to toggle auto restart flag (^ for ctrl)\n"
           "    --coresize <n>        set maximum core size for child to <n>\n"
           " -c --chdir <dir>         change directory to <dir> before starting child\n"
           " -d --debug               debug mode (keeps child in foreground)\n"
           " -e --exec <str>          specify child executable (default: arg0 of <command>)\n"
           " -f --foreground          keep child in foreground (interactive)\n"
           " -h --help                print this message\n"
           "    --holdoff <n>         set holdoff time [sec] between child restarts\n"
           " -i --ignore <str>        ignore all chars in <str> (^ for ctrl)\n"
           " -I --info-file <file>    write instance information to this file\n"
           " -k --killcmd <str>       command to kill (reboot) the child (^ for ctrl)\n"
           "    --killsig <n>         signal to send to child when killing\n"
           " -l --logport <endpoint>  allow log connections through telnet <endpoint>\n"
           " -L --logfile <file>      write log to <file>, '-' logs to stdout\n"
           "    --logstamp [<str>]    prefix log lines with timestamp [strftime format]\n"
           " -n --name <str>          set child's name (default: arg0 of <command>)\n"
           "    --noautorestart       do not restart child on exit by default\n"
           " -o --oneshot             after child exits, exit the server\n"
           " -p --pidfile <str>       write PID file (for server PID)\n"
           " -P --port <endpoint>     allow control connections through telnet <endpoint>\n"
           " -q --quiet               suppress informational output (server)\n"
           "    --restrict            restrict log access to connections from localhost\n"
           "    --timefmt <str>       set time format (strftime) to <str>\n"
           " -V --version             print program version\n"
           " -w --wait                wait for cmd on control connection to start child\n"
           " -x --logoutcmd <str>     command to logout client connection (^ for ctrl)\n"
        );
}

void printVersion()
{
    printf(PROCSERV_VERSION_STRING "\n");
}

int main(int argc,char * argv[])
{
    int c;
    unsigned int i, j;
    int k;
    long l;
    std::vector<std::string> ctlSpecs;
    char *command;
    bool bailout = false;
    const size_t BUFLEN = 512;
    char buff[BUFLEN];
    std::string infofile;
    bool firstRun;

    time(&procServStart);             // remember start time
    procservName = argv[0];
    myDir = getcwd(NULL, 512);
    chDir = myDir;
    timeFormat = defaulttimeFormat;

    pidFile = getenv( "PROCSERV_PID" );
    if ( getenv("PROCSERV_DEBUG") != NULL ) inDebugMode = true;

    const int ONE_CHAR_COMMANDS = 3;  // togglerestartcmd, killcmd, logoutcmd

    while (1) {
        static struct option long_options[] = {
            {"allow",          no_argument,       0, 'A'},
            {"autorestartcmd", required_argument, 0, 'T'},
            {"coresize",       required_argument, 0, 'C'},
            {"chdir",          required_argument, 0, 'c'},
            {"debug",          no_argument,       0, 'd'},
            {"exec",           required_argument, 0, 'e'},
            {"foreground",     no_argument,       0, 'f'},
            {"help",           no_argument,       0, 'h'},
            {"holdoff",        required_argument, 0, 'H'},
            {"ignore",         required_argument, 0, 'i'},
            {"info-file",      required_argument, 0, 'I'},
            {"killcmd",        required_argument, 0, 'k'},
            {"killsig",        required_argument, 0, 'K'},
            {"logport",        required_argument, 0, 'l'},
            {"logfile",        required_argument, 0, 'L'},
            {"logstamp",       optional_argument, 0, 'S'},
            {"name",           required_argument, 0, 'n'},
            {"noautorestart",  no_argument,       0, 'N'},
            {"oneshot",        no_argument,       0, 'o'},
            {"pidfile",        required_argument, 0, 'p'},
            {"port",           required_argument, 0, 'P'},
            {"quiet",          no_argument,       0, 'q'},
            {"restrict",       no_argument,       0, 'R'},
            {"timefmt",        required_argument, 0, 'F'},
            {"version",        no_argument,       0, 'V'},
            {"wait",           no_argument,       0, 'w'},
            {"logoutcmd",      required_argument, 0, 'x'},
            {0, 0, 0, 0}
        };

        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "+c:de:fhi:I:k:l:L:n:op:P:qVwx:",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1) break;

        switch (c)
        {
        case 'A':                                 // Allow connecting from anywhere
            if ( enableAllow )
                ctlPortLocal = false;
            else
                fprintf( stderr, "%s: --allow not supported\n", procservName );
            break;

        case 'C':                                 // Core size
            l = atol( optarg );
            if ( l >= 0 ) {
                coreSize = l;
                setCoreSize = true;
            }
            break;

        case 'c':                                 // Dir to change to
            chDir = strdup( optarg );
            break;

        case 'd':                                 // Debug mode
            inDebugMode = true;
            break;

        case 'e':                                 // Child executable
            childExec = strdup( optarg );
            break;

        case 'f':                                 // Foreground mode
            inFgMode = true;
            break;

        case 'F':                                 // Time string format
            timeFormat = strdup(optarg);
            break;

        case 'S':                                 // Log time stamp format
            stampLog = true;
            if (optarg)
                stampFormat = strdup(optarg);
            break;

        case 'h':                                 // Help
            printHelp();
            exit(0);

        case 'H':                                 // Holdoff time
            k = atoi( optarg );
            if ( k >= 0 ) holdoffTime = k;
            break;

        case 'i':                                 // Ignore characters
            ignChars = (char*) calloc( strlen(optarg) + 1 + ONE_CHAR_COMMANDS, 1);
            i = j = 0;          // ^ escapes (CTRL)
            while ( i <= strlen(optarg) ) {
                if ( optarg[i] == '^' && optarg[i+1] == '^' ) {
                    ignChars[j++] = '^';
                    i += 2 ;
                } else if ( optarg[i] == '^' && optarg[i+1] >= 'A' && optarg[i+1] <= 'Z' ) {
                    ignChars[j++] = optarg[i+1] - 64;
                    i += 2;
                } else {
                    ignChars[j++] = optarg[i++];
                }
            }
            break;

        case 'I':                                 // Info file
            infofile = optarg;
            break;

        case 'k':                                 // Kill command
            killChar = getOptionChar ( optarg );
            break;

        case 'K':                                 // Kill signal
            i = abs( atoi( optarg ) );
            if ( i < 32 ) {
                killSig = i;
            } else {
                fprintf( stderr,
                         "%s: invalid kill signal %d (>31) - using default (%d)\n",
                         procservName, i, killSig );
            }
            break;

        case 'l':                                 // Log port
            logPort = strdup ( optarg );
            break;

        case 'L':                                 // Log file
            logFile = strdup( optarg );
            break;

        case 'n':                                 // Name
            childName = strdup( optarg );
            break;

        case 'N':                                 // No restart of child
            restartMode = norestart;
            break;

        case 'o':                                 // Exit server when child exits
            restartMode = oneshot;
            break;

        case 'R':                                 // Restrict log
            logPortLocal = true;
            break;

        case 'p':                                 // PID file
            pidFile = strdup( optarg );
            break;

        case 'P':                                 // Control port
            ctlSpecs.push_back(optarg);
            singleEndpointStyle = false;
            break;

        case 'q':                                 // Quiet
            quiet = true;
            break;

        case 'V':                                 // Version
            printVersion();
            exit(0);

        case 'w':                                 // Wait for manual start
            waitForManualStart = true;
            break;

        case 'x':                                 // Logout command
            logoutChar = getOptionChar(optarg);
            break;

        case 'T':                                 // Toggle auto restart command
            toggleRestartChar = getOptionChar ( optarg );
            break;

        case '?':                                 // Error
            /* getopt_long already printed an error message */
            bailout = true;
            break;

        default:
            abort ();
        }
    }

    if ((argc - optind) < (singleEndpointStyle ? 2 : 1)) {
        fprintf(stderr, "%s: missing argument\n", procservName);
        bailout = true;
    }

    if (bailout) {
        printUsage();
        exit(1);
    }

    // Single command characters should be ignored, too
    if (ignChars == NULL && (killChar || toggleRestartChar || logoutChar))
        ignChars = (char*) calloc(1 + ONE_CHAR_COMMANDS, 1);
    if (killChar)
        strncat (ignChars, &killChar, 1);
    if (toggleRestartChar)
        strncat (ignChars, &toggleRestartChar, 1);
    if (logoutChar)
        strncat (ignChars, &logoutChar, 1);

    // Set up available server commands message
    PRINTF("Setting up messages\n");
    snprintf(infoMessage3, INFO3LEN,\
            "@@@ %s%c or %s%c restarts the child, %s%c quits the server",
            CTL_SC(restartChar), CTL_SC(killChar), CTL_SC(quitChar));
    if (logoutChar) {
        snprintf(buff, BUFLEN, ", %s%c closes this connection",
                CTL_SC(logoutChar));
        strncat(infoMessage3, buff, INFO3LEN-strlen(infoMessage3)-1);
    }
    strncat(infoMessage3, NL, INFO3LEN-strlen(infoMessage3)-1);

    if (singleEndpointStyle) {
        ctlSpecs.push_back(argv[optind++]);
    }
    command = argv[optind];

    if (childName == NULL) childName = command;
    childArgv = argv + optind - 1;
    if (childExec == NULL) {
        childArgv++;
        childExec = command;
    }

    if (!stampFormat) {
        char *tmp = (char*) calloc(strlen(timeFormat)+4, 1);
        if (tmp) {
            sprintf(tmp, "[%s] ", timeFormat);
            stampFormat = tmp;
        } else {
            stampFormat = timeFormat;
        }
    }

    struct sigaction sig;
    memset(&sig, 0, sizeof(sig));

    PRINTF("Installing signal handlers\n");
    
    // SIGPIPE, SIGTERM and SIGHUP will be handled in the main loop
    // with the assistance of pselect. This means that we have them
    // blocked outside of pselect call, but unblocked atomically
    // within pselect. Each time pselect returns, we safely check if
    // any of the signals were received.
    
    // Block the signals that we bill be handling in the main loop.
    // At the same time, retrieve the original signal mask before
    // blocking, to be passed to pselect.
    sigset_t sigset_block;
    sigset_t sigset_pselect;
    sigemptyset(&sigset_block);
    sigaddset(&sigset_block, SIGPIPE);
    sigaddset(&sigset_block, SIGTERM);
    sigaddset(&sigset_block, SIGHUP);
    sigprocmask(SIG_BLOCK, &sigset_block, &sigset_pselect);
    
    sig.sa_handler = &OnSigPipe;              // sigaction() needed for Solaris
    sigaction(SIGPIPE, &sig, NULL);
    sig.sa_handler = &OnSigTerm;
    sigaction(SIGTERM, &sig, NULL);
    sig.sa_handler = &OnSigHup;
    sigaction(SIGHUP, &sig, NULL);
    sig.sa_handler = SIG_IGN;
    sigaction(SIGXFSZ, &sig, NULL);
    if (inFgMode) {
        sig.sa_handler = SIG_IGN;
        sigaction(SIGINT, &sig, NULL);
        sig.sa_handler = SIG_IGN;
        sigaction(SIGQUIT, &sig, NULL);
    }

    // Make an accept item to listen for control connections
    PRINTF("Creating control listener\n");
    try
    {
        for(size_t i=0; i<ctlSpecs.size(); i++) {
            connectionItem *acceptItem = acceptFactory( ctlSpecs[i].c_str(), ctlPortLocal );
            AddConnection(acceptItem);
        }
    }
    catch (int error)
    {
        perror("Caught an exception creating the initial control telnet port");
        fprintf(stderr, "%s: Exiting with error code: %d\n",
                procservName, error);
        exit(error);
    }

    if ( logPort ) {
        // Make an accept item to listen for log connections
        PRINTF("Creating log listener\n");
        try
        {
            connectionItem *acceptItem = acceptFactory( logPort, logPortLocal, true );
            AddConnection(acceptItem);
        }
        catch (int error)
        {
            perror("Caught an exception creating the initial log telnet port");
            fprintf(stderr, "%s: Exiting with error code: %d\n",
                    procservName, error);
            exit(error);
        }
    }

    procservPid=getpid();

    openLogFile();

    if (false == inFgMode && false == inDebugMode)
    {
        forkAndGo();
    }
    else
    {
        debugFD = 1;          // Enable debug messages
    }
    writePidFile();

    setEnvVar();

    if (!infofile.empty()) {
        writeInfoFile(infofile);
    }

    if (inFgMode && !(logFile && strcmp(logFile, "-")==0)) {
        ttySetCharNoEcho(true);
        AddConnection(clientFactory(0));
    }

    // Record some useful data for managers 
    snprintf(infoMessage1, INFO1LEN,
             "@@@ procServ server PID: %ld" NL
             "@@@ Server startup directory: %s" NL
             "@@@ Child startup directory: %s" NL,
             (long) getpid(),
             myDir,
             chDir);
    if ( strcmp( childName, command ) )
        snprintf(buff, BUFLEN, "@@@ Child \"%s\" started as: %s" NL,
                 childName, command );
    else
        snprintf(buff, BUFLEN, "@@@ Child started as: %s" NL,
                 command );
    strncat(infoMessage1, buff, INFO1LEN-strlen(infoMessage1)-1);
    snprintf(infoMessage2, INFO2LEN, "@@@ Child \"%s\" is SHUT DOWN" NL, childName);
    if ( logFile ) {
	if ( -1 == logFileFD )
            snprintf(buff, BUFLEN, "@@@ Child log file: unable to open log file %s" NL,
                     logFile );
	else
            snprintf(buff, BUFLEN, "@@@ Child log file: %s" NL,
                     logFile );
        strncat(infoMessage1, buff, INFO1LEN-strlen(infoMessage1)-1);
    }

    firstRun = true;
    // Run here until something makes it die
    while ( ! shutdownServer )
    {
        const size_t BUFLEN = 100;
        char buf[BUFLEN];
        connectionItem * p;
        fd_set fdset;              // FD stuff for select()
        int fd, nFd;
        int ready;                 // select() return value
        struct timespec timeout;

        // Prepare FD set for select()
        p = connectionItem::head;
        nFd = -1;
        FD_ZERO(&fdset);
        while (p) {
            if ((fd = p->getFd()) > -1) {     // Connection needs to be watched
                if (fd > nFd) nFd = fd;
                FD_SET(fd, &fdset);
            }
            p = p->next;
        }
        nFd++;
        timeout.tv_sec = 0;                   // select() timeout: 0.5 sec
        timeout.tv_nsec = 500000000l;

        ready = pselect(nFd, &fdset, NULL, NULL, &timeout, &sigset_pselect);
        
        // Handle signals for which signal handlers were called while in pselect.
        
        if (sigPipeSet) {
            sigPipeSet = 0;
            sprintf( buf, "@@@ Got a sigPipe signal: Did the child close its tty?" NL);
            SendToAll( buf, strlen(buf), NULL );
        }
        
        if (sigTermSet) {
            sigTermSet = 0;
            PRINTF("SigTerm received\n");
            processFactorySendSignal(killSig);
            shutdownServer = true;
        }

        if (sigHupSet) {
            sigHupSet = 0;
            PRINTF("SigHup received\n");
            openLogFile();
        }
        
        if (0 == ready) {                     // Timeout
            // Go clean up dead connections
            OnPollTimeout();
            connectionItem * npi; 

            // Pick up the process item if it dies
            // This call returns NULL if the process item lives
            if (processFactoryNeedsRestart())
            {
                if ((restartMode == oneshot) && !firstRun) {
                  PRINTF("Option oneshot is set... exiting\n");
                  shutdownServer = true;
                } else {
                  npi= processFactory(childExec, childArgv);
                  if (npi) AddConnection(npi);
                  if (firstRun) {
                  	firstRun = false;
                  }
                }
            }
        } else if (-1 == ready) {             // Error
            if (EINTR != errno) {
                perror("Error in select() call");
            }
        } else {                              // Work to be done
            // Loop through all connections
            p = connectionItem::head;
            while (p) {
                if (FD_ISSET(p->getFd(), &fdset)) p->readFromFd();
                p = p->next;
            }
            OnPollTimeout();
        }
    }
    ttySetCharNoEcho(false);

    PRINTF("Close sockets\n");

    while(connectionItem::head) {
        connectionItem *p = connectionItem::head;
        connectionItem::head = p->next;
        delete p;
    }

    PRINTF("Cleanup pid and info files\n");

    if(!infofile.empty())
        unlink(infofile.c_str());
    if(pidFile && strlen(pidFile) > 0)
        unlink(pidFile);

    return childExitCode;
}

// Connection items call this to send messages to others
// // This is a party line system, messages go to everyone
// // the sender's this pointer keeps it from getting its own
// // messages.
// //
void SendToAll(const char * message,
               int count,
               const connectionItem * sender)
{
    connectionItem * p = connectionItem::head;
    char stamp[64];
    int len = 0;
    time_t now;
    struct tm now_tm;

    time(&now);
    localtime_r(&now, &now_tm);
    strftime(stamp, sizeof(stamp)-1, stampFormat, &now_tm);
    len = strlen(stamp);

    // Log the traffic to file / stdout (debug)
    if (sender==NULL || sender->isProcess())
    {
        if (logFileFD > 0) {
            if (stampLog) {
                // Some OSs (Windows) do not support line buffering, so we can get parts of lines,
                // hence need to track of when to send timestamp
                static bool log_stamp_sent = false;
                int i = 0, j = 0;
                for (i = 0; i < count; ++i) {
                    if (!log_stamp_sent) {
                        ignore_result( write(logFileFD, stamp, len) );
                        log_stamp_sent = true;
                    }
                    if (message[i] == '\n') {
                        ignore_result( write(logFileFD, message+j, i-j+1) );
                        j = i + 1;
                        log_stamp_sent = false;
                    }
                }
                ignore_result( write(logFileFD, message+j, count-j) );  // finish off rest of line with no newline at end
            } else {
                ignore_result( write(logFileFD, message, count) );
            }
            fsync(logFileFD);
        }
        if (inFgMode == false && debugFD > 0) ignore_result( write(debugFD, message, count) );
    }

    while (p) {
        if (p->isProcess()) {
            // Non-null senders that are not processes can send to processes
            if (sender && !sender->isProcess()) p->Send(message, count);
        } else {
            // Null senders and processes can send to connections, with time stamp
            if (!sender || sender->isProcess()) {
                if (stampLog)
                    p->Send(stamp, len, message, count);
                else
                    p->Send(message, count);
            }
        }
        p = p->next;
    }
}


// Handles housekeeping
void OnPollTimeout()
{
    pid_t pid;
    int wstatus;
    connectionItem *pc, *pn;
    const size_t BUFLEN = 128;
    char buf[BUFLEN] = NL;

    pid = waitpid(-1, &wstatus, WNOHANG);
    if (pid > 0 ) {
        pc = connectionItem::head;
        while (pc) {
            pc->markDeadIfChildIs(pid);
            pc=pc->next;
        }

        SendToAll(buf, strlen(buf), NULL);
        strcpy(buf, "@@@ @@@ @@@ @@@ @@@" NL);
        SendToAll(buf, strlen(buf), NULL);

        snprintf(buf, BUFLEN, "@@@ Received a sigChild for process %ld.", (long) pid);

        if (WIFEXITED(wstatus)) {
            snprintf(buf+strlen(buf), BUFLEN-strlen(buf),
                     " Normal exit status = %d",
                     WEXITSTATUS(wstatus));
            childExitCode = WEXITSTATUS(wstatus);
        }

        if (WIFSIGNALED(wstatus)) {
            snprintf(buf+strlen(buf), BUFLEN-strlen(buf),
                     " The process was killed by signal %d",
                     WTERMSIG(wstatus));
        }
        strncat(buf, NL, BUFLEN-strlen(buf)-1);
        SendToAll(buf, strlen(buf), NULL);
    }

    // Clean up connections
    pc = connectionItem::head;
    while (pc)
    {
        pn = pc->next;
        if (pc->IsDead()) DeleteConnection(pc);
        pc = pn;
    }
}

// Call this to add the item to the list of connections
void AddConnection(connectionItem * ci)
{
    PRINTF("Adding connection %p to list\n", ci);
    if (connectionItem::head )
	{
	    ci->next=connectionItem::head;
	    ci->next->prev=ci;
	}
	else ci->next=NULL;
	
	ci->prev=NULL;
	connectionItem::head=ci;
	connectionNo++;
}


void DeleteConnection(connectionItem *ci)
{
    PRINTF("Deleting connection %p\n", ci);
    if (ci->prev) // Not the head
	{
		ci->prev->next=ci->next;
	}
	else
	{
		connectionItem::head = ci->next;
	}
	if (ci->next) ci->next->prev=ci->prev;
        delete ci;
	connectionNo--;
	assert(connectionNo>=0);
}

static void OnSigPipe(int)
{
    sigPipeSet = 1;
}

static void OnSigTerm(int)
{
    sigTermSet = 1;
}

static void OnSigHup(int)
{
    sigHupSet = 1;
}

// Fork the daemon and exit the parent
void forkAndGo()
{
    pid_t p;
    int fh;

    if ((p = fork()) < 0) {  // Fork failed
        perror("Could not fork daemon process");
        exit(errno);

    } else if (p > 0) {      // I am the PARENT (foreground command)
        if (!quiet) {
            fprintf(stderr, "%s: spawning daemon process: %ld\n", procservName, (long) p);
            if (-1 == logFileFD) {
                fprintf(stderr, "Warning: No log file%s specified.\n",
                        logPort ? "" : " and no port for log connections");
            }
        }
        exit(0);

    } else {                 // I am the CHILD (background daemon)
        procservPid = getpid();

        // Redirect stdin, stdout, stderr to /dev/null
        char buf[] = "/dev/null";
        fh = open(buf, O_RDWR);
        if (fh < 0) { perror(buf); exit(-1); }
        close(0); close(1); close(2);
        ignore_result( dup(fh) ); ignore_result( dup(fh) ); ignore_result( dup(fh) );
        close(fh);

        // Make sure we are not attached to a terminal
        setsid();
    }
}


void openLogFile()
{
    if (-1 != logFileFD && 1 != logFileFD) {
        close(logFileFD);
    }
    if (logFile && strcmp(logFile, "-")==0) {
        logFileFD = 1;
    } else
    if (logFile) {
        logFileFD = open(logFile, O_CREAT|O_WRONLY|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        if (-1 == logFileFD) {         // Don't stop here - just go without
            fprintf(stderr,
                    "%s: unable to open log file %s\n",
                    procservName, logFile);
        } else {
            PRINTF("Opened file %s for logging\n", logFile);
        }
    }
}

void writeInfoFile(const std::string& infofile)
{
    std::ofstream info(infofile.c_str());
    info<<"pid:"<<getpid()<<"\n";
    for(connectionItem *it = connectionItem::head; it; it=it->next)
        it->writeAddress(info);
}

void setEnvVar()
{
    std::ostringstream env_var;
    env_var<<"PID="<<getpid()<<";";
    for(connectionItem *it = connectionItem::head; it; it=it->next)
        it->writeAddressEnv(env_var);
    std::string env_str = env_var.str();
    // Remove the extra semicolon
    env_str = env_str.substr(0, env_str.size()-1);
    setenv("PROCSERV_INFO", env_str.c_str(), 1);
}

void ttySetCharNoEcho(bool set) {
    static struct termios org_mode;
    static struct termios mode;
    static bool saved = false;

    if(isatty(0)!=1) return;

    if (set && !saved) {
        tcgetattr(0, &mode);
        org_mode = mode;
        saved = true;
        mode.c_iflag &= ~IXON;
        mode.c_lflag &= ~ICANON;
        mode.c_lflag &= ~ECHO;
        mode.c_cc[VMIN] = 1;
        tcsetattr(0, TCSANOW, &mode);
    } else if (saved) {
        tcsetattr(0, TCSANOW, &org_mode);
    }
}

connectionItem * connectionItem::head;
// Globals:
time_t procServStart; // Time when this IOC started
time_t IOCStart; // Time when the current IOC was started
