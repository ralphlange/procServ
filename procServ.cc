// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange 03/05/2012
// GNU Public License (GPLv3) applies - see www.gnu.org


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

#include "procServ.h"

#ifdef ALLOW_FROM_ANYWHERE
const bool enableAllow = true;   // Enable --allow option
#else
const bool enableAllow = false;  // Default: NO
#endif

bool   inDebugMode;              // This enables a lot of printfs
bool   inFgMode = false;         // This keeps child in the foreground, tty connected
bool   logPortLocal;             // This restricts log port access to localhost
bool   ctlPortLocal = true;      // Restrict control connections to localhost
bool   autoRestart = true;       // Enables instant restart of exiting child
bool   waitForManualStart = false;  // Waits for telnet cmd to manually start child
bool   shutdownServer = false;   // To keep the server from shutting down
bool   quiet = false;            // Suppress info output (server)
bool   setCoreSize = false;      // Set core size for child
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

pid_t  procservPid;              // PID of server (daemon if not in debug mode)
char   *pidFile;                 // File name for server PID
char   defaultpidFile[] = "pid.txt";  // default
char   *timeFormat;              // Time format string
char   defaulttimeFormat[] = "%c";    // default
bool   stampLog = false;         // Prefix log lines with time stamp
char   *stampFormat;             // Log time stamp format string

char   infoMessage1[512];        // Sign on message: server PID, child pwd and command line
char   infoMessage2[128];        // Sign on message: child PID
char   infoMessage3[128];        // Sign on message: available server commands

char   *logFile = NULL;          // File name for log
int    logFileFD=-1;             // FD for log file
int    logPort;                  // Port no. for logger connections
int    debugFD=-1;               // FD for debug output

#define MAX_CONNECTIONS 64

// mLoop runs the program
void mLoop();
// Handles houskeeping
void OnPollTimeout();
// Daemonizes the program
void forkAndGo();
// Checks the command file (existence and access rights)
int checkCommandFile(const char *command);
void openLogFile();
void ttySetCharNoEcho(bool save);

// Signal handlers
void OnSigPipe(int);
void OnSigTerm(int);
void OnSigHup(int);
// Counter used for communication between sig handler and main()
int sigPipeSet;

void writePidFile()
{
    int pid = getpid();
    FILE * fp;

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
    printf("Usage: %s [options] <port> <command args ...>    (-h for help)\n",
           procservName);
}

void printHelp()
{
    printUsage();
    printf("<port>              use telnet <port> for command connections\n"
           "<command args ...>  command line to start child process\n"
           "Options:\n"
           "    --allow             allow control connections from anywhere\n"
           "    --autorestartcmd    command to toggle auto restart flag (^ for ctrl)\n"
           "    --coresize <n>      set maximum core size for child to <n>\n"
           " -c --chdir <dir>       change directory to <dir> before starting child\n"
           " -d --debug             debug mode (keeps child in foreground)\n"
           " -e --exec <str>        specify child executable (default: arg0 of <command>)\n"
           " -f --foreground        keep child in foreground (interactive)\n"
           " -h --help              print this message\n"
           "    --holdoff <n>       set holdoff time between child restarts\n"
           " -i --ignore <str>      ignore all chars in <str> (^ for ctrl)\n"
           " -k --killcmd <str>     command to kill (reboot) the child (^ for ctrl)\n"
           "    --killsig <n>       signal to send to child when killing\n"
           " -l --logport <n>       allow log connections through telnet port <n>\n"
           " -L --logfile <file>    write log to <file>\n"
           "    --logstamp [<str>]  prefix log lines with timestamp [strftime format]\n"
           " -n --name <str>        set child's name (default: arg0 of <command>)\n"
           "    --noautorestart     do not restart child on exit by default\n"
           " -p --pidfile <str>     name of PID file (for server PID)\n"
           " -q --quiet             suppress informational output (server)\n"
           "    --restrict          restrict log connections to localhost\n"
           "    --timefmt <str>     set time format (strftime) to <str>\n"
           " -V --version           print program version\n"
           " -w --wait              wait for telnet cmd to manually start child\n"
           " -x --logoutcmd <str>   command to logout client connection (^ for ctrl)\n"
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
    int ctlPort = 0;
    char *command;
    bool wrongOption = false;
    char buff[512];

    time(&procServStart);             // remember start time
    procservName = argv[0];
    myDir = getcwd(NULL, 512);
    chDir = myDir;
    timeFormat = defaulttimeFormat;

    pidFile = getenv( "PROCSERV_PID" );
    if ( !pidFile || strlen(pidFile) == 0 ) pidFile = defaultpidFile;
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
            {"killcmd",        required_argument, 0, 'k'},
            {"killsig",        required_argument, 0, 'K'},
            {"logport",        required_argument, 0, 'l'},
            {"logfile",        required_argument, 0, 'L'},
            {"logstamp",       optional_argument, 0, 'S'},
            {"name",           required_argument, 0, 'n'},
            {"noautorestart",  no_argument,       0, 'N'},
            {"pidfile",        required_argument, 0, 'p'},
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

        c = getopt_long (argc, argv, "+c:de:fhi:k:l:L:n:p:qVwx:",
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
            k = atoi( optarg );
            if ( k >= 0 ) {
                coreSize = k;
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
            logPort = abs ( atoi( optarg ) );
            if ( logPort < 1024 ) {
                fprintf( stderr,
                         "%s: invalid log port %d (<1024) - disabling log port\n",
                         procservName, logPort );
                logPort = 0;
            }
            break;

        case 'L':                                 // Log file
            logFile = strdup( optarg );
            break;

        case 'n':                                 // Name
            childName = strdup( optarg );
            break;

        case 'N':                                 // No restart of child
            autoRestart = false;
            break;

        case 'R':                                 // Restrict log
            logPortLocal = true;
            break;

        case 'p':                                 // PID file
            pidFile = strdup( optarg );
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
            wrongOption = true;
            break;

        default:
            abort ();
        }
    }

    if ( (argc-optind) < 2 )
    {
        fprintf( stderr, "%s: missing argument\n", procservName );
    }

    if ( wrongOption || (argc-optind) < 2 )
    {
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
    sprintf(infoMessage3, "@@@ %s%c or %s%c restarts the child, %s%c quits the server",
            CTL_SC(restartChar), CTL_SC(killChar), CTL_SC(quitChar));
    if (logoutChar) {
        sprintf(buff, ", %s%c closes this connection",
                CTL_SC(logoutChar));
        strcat(infoMessage3, buff);
    }
    strcat(infoMessage3, NL);

    ctlPort = atoi(argv[optind]);
    command = argv[optind+1];

    if (childName == NULL) childName = command;
    childArgv = argv + optind;
    if (childExec == NULL) {
        childArgv++;
        childExec = command;
    }

    if ( ctlPort < 1024 )
    {
        fprintf( stderr,
                 "%s: invalid control port %d (<1024)\n",
                 procservName, ctlPort );
        exit(1);
    }

    if (stampLog && !stampFormat) {
        stampFormat = (char*) calloc(strlen(timeFormat)+4, 1);
        sprintf(stampFormat, "[%s] ", timeFormat);
    }

    if (checkCommandFile(childExec)) exit(errno);

    struct sigaction sig;
    memset(&sig, 0, sizeof(sig));

    PRINTF("Installing signal handlers\n");
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
	connectionItem *acceptItem = acceptFactory( ctlPort, ctlPortLocal );
	AddConnection(acceptItem);
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
        writePidFile();
    }
    else
    {
        debugFD = 1;          // Enable debug messages
    }

    if (inFgMode) {
        ttySetCharNoEcho(true);
        AddConnection(clientFactory(0));
    }

    // Record some useful data for managers 
    sprintf( infoMessage1, "@@@ procServ server PID: %ld" NL
             "@@@ Server startup directory: %s" NL
             "@@@ Child startup directory: %s" NL,
             (long) getpid(),
             myDir,
             chDir);
    if ( strcmp( childName, command ) )
        sprintf( buff, "@@@ Child \"%s\" started as: %s" NL,
                 childName, command );
    else
        sprintf( buff, "@@@ Child started as: %s" NL,
                 command );
    if ( strlen(infoMessage1) + strlen(buff) + 1 < sizeof(infoMessage1) )
        strcat( infoMessage1, buff);
    sprintf( infoMessage2, "@@@ Child \"%s\" is SHUT DOWN" NL, childName );

    // Run here until something makes it die
    while ( ! shutdownServer )
    {
        char buf[100];
        connectionItem * p;
        fd_set fdset;              // FD stuff for select()
        int fd, nFd;
        int ready;                 // select() return value
        struct timeval timeout;

        if (sigPipeSet > 0) {
            sprintf( buf, "@@@ Got a sigPipe signal: Did the child close its tty?" NL);
            SendToAll( buf, strlen(buf), NULL );
            sigPipeSet--;
        }

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
        timeout.tv_usec = 500000;

        ready = select(nFd, &fdset, NULL, NULL, &timeout);

        if (0 == ready) {                     // Timeout
            // Go clean up dead connections
            OnPollTimeout();
            connectionItem * npi; 

            // Pick up the process item if it dies
            // This call returns NULL if the process item lives
            if (processFactoryNeedsRestart())
            {
                npi= processFactory(childExec, childArgv);
                if (npi) AddConnection(npi);
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
}

// Connection items call this to send messages to others
// // This is a party line system, messages go to everyone
// // the sender's this pointer keeps it from getting its own
// // messages.
// //
void SendToAll(const char * message,int count,const connectionItem * sender)
{
    connectionItem * p = connectionItem::head;
    char stamp[64];
    int len = 0;
    time_t now;
    struct tm now_tm;

    if (stampLog) {
        time(&now);
        localtime_r(&now, &now_tm);
        strftime(stamp, sizeof(stamp)-1, stampFormat, &now_tm);
        len = strlen(stamp);
    }
    // Log the traffic to file / stdout (debug)
    if (sender==NULL || sender->isProcess())
    {
        if (logFileFD > 0) {
            if (stampLog) write(logFileFD, stamp, len);
            write(logFileFD, message, count);
        }
        if (false == inFgMode && debugFD > 0) write(debugFD, message, count);
    }

    while ( p ) {
	if ( p->isProcess() )
	{
	    // Non-null senders that are not processes can send to processes
        if (sender && !sender->isProcess()) p->Send(message, count);
	}
	else // Not a process
	{
	    // Null senders and processes can send to non-processes (ie connections)
        if (sender==NULL || sender->isProcess()) {
            if (stampLog && p->isLogger()) p->Send(stamp, len);
            p->Send(message, count);
        }
	}
	p=p->next;
    }
}


// Handles housekeeping
void OnPollTimeout()
{
    pid_t pid;
    int wstatus;
    connectionItem *pc, *pn;
    char buf[128] = NL;

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

        sprintf(buf, "@@@ Received a sigChild for process %ld.", (long) pid);

        if (WIFEXITED(wstatus)) {
            sprintf(buf + strlen(buf), " Normal exit status = %d",
                    WEXITSTATUS(wstatus));
        }

        if (WIFSIGNALED(wstatus)) {
            sprintf(buf + strlen(buf), " The process was killed by signal %d",
                    WTERMSIG(wstatus));
        }
        strcat(buf, NL);
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

void OnSigPipe(int)
{
    PRINTF("SigPipe received\n");
    sigPipeSet++;
}

void OnSigTerm(int)
{
    PRINTF("SigTerm received\n");
    processFactorySendSignal(killSig);
    shutdownServer = true;
}

void OnSigHup(int)
{
    PRINTF("SigHup received\n");
    openLogFile();
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
        dup(fh); dup(fh); dup(fh);
        close(fh);

        // Make sure we are not attached to a terminal
        setsid();
    }
}


// Check to see if the command is really executable:
// Return 0 if the file is executable
int checkCommandFile(const char * command)
{
    struct stat s;
    mode_t must_perm   =         S_IXUSR        |S_IXGRP        |S_IXOTH;
    mode_t should_perm = S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;

    PRINTF("Checking command file validity\n");

    // chdir if possible (to allow relative command)
    if (chDir && chdir(chDir)) perror(chDir);

    if (stat(command,&s)) {
        perror(command);
        return -1;
    }

    // Back to where we came from
    if (chdir(myDir)) perror(myDir);

    if (!S_ISREG(s.st_mode)) {
        fprintf(stderr, "%s: %s is not a regular file\n", procservName, command);
        return -1;
    }

    if (must_perm != (s.st_mode & must_perm)) {
        fprintf(stderr, "%s: Error - Please change permissions on %s to at least ---x--x--x\n"
                "procServ is not able to continue without execute permission\n",
                procservName, command);
        return -1;
    }

    if (should_perm != (s.st_mode & should_perm)) {
        fprintf(stderr, "%s: Warning - Please change permissions on %s to at least -r-xr-xr-x\n"
                "procServ may not be able to continue without read and execute permissions\n",
                procservName, command);
        return 0;
    }

    return 0;
}

void openLogFile()
{
    if (-1 != logFileFD) {
        close(logFileFD);
    }
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

void ttySetCharNoEcho(bool set) {
    static struct termios org_mode;
    static struct termios mode;
    static bool saved = false;

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
