// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange 04/22/2008
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
#include <string.h>

#include "procServ.h"

#ifdef ALLOW_FROM_ANYWHERE
const bool enableAllow = true;   // Enable --allow option
#else
const bool enableAllow = false;  // Default: NO
#endif

bool   inDebugMode;              // This enables a lot of printfs
bool   logPortLocal;             // This restricts log port access to localhost
bool   ctlPortLocal = true;      // Restrict control connections to localhost
bool   autoRestart = true;       // Enables instant restart of exiting child
bool   waitForManualStart = false;  // Waits for telnet cmd to manually start child
bool   shutdownServer = false;   // To keep the server from shutting down
bool   quiet = false;            // Suppress info output (server)
char   *procservName;            // The name of this beast (server)
char   *childName;               // The name of that beast (child)
int    connectionNo;             // Total number of connections
char   *ignChars = NULL;         // Characters to ignore
char   killChar = 0x18;          // Kill command character (default: ^X)
char   toggleRestartChar = 0x14; // Toggle autorestart character (default: ^T)
char   restartChar = 0x12;       // Restart character (default: ^R)
char   quitChar = 0x11;          // Quit character (default: ^Q)
int    killSig = SIGKILL;        // Kill signal (default: SIGKILL)
rlim_t coreSize = -1;            // Max core size for child
char   *chDir = NULL;            // Directory to change to before starting child
char   *myDir = NULL;            // Directory where server was started

pid_t  procservPid;              // PID of server (daemon if not in debug mode)
char   *pidFile;                 // File name for server PID
char   defaultpidFile[] = "pid.txt";  // default

char   infoMessage1[512];        // Sign on message: server PID, child pwd and command line
char   infoMessage2[128];        // Sign on message: child PID
char   infoMessage3[128];        // Sign on message: available server commands

char   *logFile = NULL;          // File name for log
int    logFileFD=-1;;            // FD for log file
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

void OnSigChild(int);
int sigChildSet;

void OnSigPipe(int);
int sigPipeSet;

struct sigaction sig;

void writePidFile()
{
    int pid=getpid();
    FILE * fp=NULL;

    fp = fopen( pidFile, "w" );
    // Don't stop here - just go without
    if ( fp==NULL ) {
        fprintf( stderr,
                 "%s: unable to open PID file %s\n",
                 procservName, pidFile );
        return;
    }
    fprintf( fp, "%d\n", pid );
    fclose( fp );
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
    printf("Usage: %s [options] <port> <command args ... >    (-h for help)\n",
           procservName);
}

void printHelp()
{
    printUsage();
    printf("<port>              use telnet <port> for command connections\n"
           "<command args ...>  command line to start child process\n"
           "Options:\n"
           "    --allow           allow control connections from anywhere\n"
           "    --autorestartcmd  command to toggle auto restart flag (^ for ctrl)\n"
           "    --coresize <n>    sets maximum core size for child to <n>\n"
           " -c --chdir <dir>     change directory to <dir> before starting child\n"
           " -d --debug           enable debug mode (keeps child in foreground)\n"
           " -h --help            print this message\n"
           " -i --ignore <str>    ignore all chars in <str> (^ for ctrl)\n"
           " -k --killcmd <str>   command to kill (reboot) the child (^ for ctrl)\n"
           "    --killsig <n>     signal to send to child when killing\n"
           " -l --logport <n>     allow log connections through telnet port <n>\n"
           " -L --logfile <file>  write log to <file>\n"
           " -n --name <str>      set child's name (defaults to command line)\n"
           "    --noautorestart   do not restart child on exit by default\n"
           " -p --pidfile <str>   name of PID file (for server PID)\n"
           " -q --quiet           suppress informational output (server)\n"
           "    --restrict        restrict log connections to localhost\n"
           " -w --wait            wait for telnet cmd to manually start child\n"
        );
}

int main(int argc,char * argv[])
{
    time(&procServStart); // What time is it now
    struct pollfd * pollList=NULL,* ppoll; // Allocate as much space as needed
    int c;
    unsigned int i, j;
    int ctlPort, logPort=0;
    char *command;
    bool wrongOption = false;
    char buff[512];

    procservName = argv[0];
    myDir = get_current_dir_name();
    chDir = myDir;

    pidFile = getenv( "PROCSERV_PID" );
    if ( pidFile && ! strcmp( pidFile, "" ) ) pidFile = defaultpidFile;

    const int ONE_CHAR_COMMANDS = 2;  // togglerestartcmd, killcmd

    while (1) {
        static struct option long_options[] = {
            {"allow",          no_argument,       0, 'A'},
            {"autorestartcmd", required_argument, 0, 'T'},
            {"coresize",       required_argument, 0, 'C'},
            {"chdir",          required_argument, 0, 'c'},
            {"debug",          no_argument,       0, 'd'},
            {"help",           no_argument,       0, 'h'},
            {"ignore",         required_argument, 0, 'i'},
            {"killcmd",        required_argument, 0, 'k'},
            {"killsig",        required_argument, 0, 'K'},
            {"logport",        required_argument, 0, 'l'},
            {"logfile",        required_argument, 0, 'L'},
            {"name",           required_argument, 0, 'n'},
            {"noautorestart",  no_argument,       0, 'N'},
            {"pidfile",        required_argument, 0, 'p'},
            {"quiet",          no_argument,       0, 'q'},
            {"restrict",       no_argument,       0, 'R'},
            {"wait",           no_argument,       0, 'w'},
            {0, 0, 0, 0}
        };

        /* getopt_long stores the option index here. */
        int option_index = 0;
     
        c = getopt_long (argc, argv, "+c:dhi:k:l:L:n:p:qw",
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
            i = atoi( optarg );
            if ( i >= 0 ) coreSize = i;
            break;

        case 'c':                                 // Dir to change to
            chDir = strdup( optarg );
            break;

        case 'd':                                 // Debug mode
            inDebugMode = true;
            break;

        case 'h':                                 // Help
            printHelp();
            exit(0);

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

        case 'w':                                 // Wait for manual start
            waitForManualStart = true;
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
    if ( ignChars == NULL && ( killChar || toggleRestartChar ) )
        ignChars = (char*) calloc( 1 + ONE_CHAR_COMMANDS, 1);
    if ( killChar )
        strcat ( ignChars, &killChar );
    if ( toggleRestartChar )
        strcat ( ignChars, &toggleRestartChar );

    // set up available server commands message
    sprintf( infoMessage3, "@@@ Use %s%c to restart the child, %s%c to quit the server" NL,
             restartChar < 32 ? "^" : "",
             restartChar < 32 ? restartChar + 64 : restartChar,
             quitChar < 32 ? "^" : "",
             quitChar < 32 ? quitChar + 64 : quitChar);

    ctlPort = atoi(argv[optind]);
    command = argv[optind+1];
    if ( childName == NULL ) childName = command;

    if ( ctlPort < 1024 )
    {
        fprintf( stderr,
                 "%s: invalid control port %d (<1024)\n",
                 procservName, ctlPort );
	exit(1);
    }

    if ( checkCommandFile( command ) ) exit( errno );

    sig.sa_handler=&OnSigChild;
    sigaction(SIGCHLD,&sig,NULL);
    sig.sa_handler=&OnSigPipe;
    sigaction(SIGPIPE,&sig,NULL);

    // Make an accept item to listen for control connections
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

    if ( getenv("PROCSERV_DEBUG") != NULL ) inDebugMode = true;

    if ( inDebugMode == false )
    {
	forkAndGo();
	writePidFile();
    }
    else
    {
	debugFD = 1;          // Enable debug messages
    }

    // Open log file
    if ( logFile ) {
        logFileFD = open( logFile, O_CREAT|O_WRONLY|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH );
        if ( logFileFD == -1 ) {         // Don't stop here - just go without
            fprintf( stderr,
                     "%s: unable to open log file %s\n",
                     procservName, logFile );
        } else {
            PRINTF( "Opened file %s for logging\n", logFile );
        }
    }

    // Record some useful data for managers 
    sprintf( infoMessage1, "@@@ procServ server PID: %d" NL
             "@@@ Server startup directory: %s" NL
             "@@@ Child startup directory: %s" NL,
             getpid(),
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
	int nPoll=0; // local copy of the # items to poll
	int nPollAlloc; // How big is pollList right now
	int pollStatus; // What poll returns

	connectionItem * p ;

	if (sigPipeSet>0)
	{
	    PRINTF("Got a sigPipe\n");
	    sprintf( buf, "@@@ Got a sigPipe signal: Did the IOC close its tty?" NL);
	    SendToAll( buf, strlen(buf), NULL );
	    sigPipeSet--;
	}
	// Adjust the poll data array
	if (nPollAlloc != connectionNo)
	{
	    if (pollList) free(pollList);
	    pollList = (pollfd*) malloc( connectionNo * sizeof(struct pollfd) );
	    nPollAlloc = connectionNo;
	}

	// Load the socket number and flags
	ppoll=pollList;
	nPoll=0;
	p = connectionItem::head;
	while(p)
	{
	    if (p->SetPoll(ppoll))
	    {
	    	ppoll++; // Move to the nexr fd slot
		nPoll++; // How many to poll
	    }
	    p=p->next;
	}
	
	// Now do the poll to find new data
	pollStatus=poll(pollList,nPoll,500);

	// handle what poll returns
        if (pollStatus==0)
	{
	    // Go clean up dead connections
	    OnPollTimeout();
	    connectionItem * npi; 
	    
	    // Pick up the process item if it dies
	    // This call returns NULL if the process item lives
	    if (processFactoryNeedsRestart())
	    {
	    	npi= processFactory(argc-optind-2,argv+optind+1);
	    	if (npi) AddConnection(npi);
	    }
	}
	else
	{
	    // Loop thru all of the connectionItems until we find the one(s) that need to
	    // do I/O
	    p = connectionItem::head;
	    nPoll=0;
	    while(p && nPoll<pollStatus)
	    {
		if (p->OnPoll()) nPoll++;
		p=p->next;
	    }
	}
    }
}

// Connection items call this to send messages to others
// // This is a party line system, messages go to everyone
// // the sender's this pointer keeps it from getting its own
// // messages.
// //
void SendToAll(const char * message,int count,const connectionItem * sender)
{
    connectionItem * p = connectionItem::head;

    // Log the traffic to file / stdout (debug)
    if ( sender==NULL  || sender->isProcess() )
    {
        if ( logFileFD > 0 ) write( logFileFD, message, count );
        if ( debugFD > 0 ) write( debugFD, message, count );
    }

    while ( p ) {
	if ( p->isProcess() ) 
	{
	    // Non-null senders that are not processes can send to processes
	    if (sender && !sender->isProcess()) p->Send(message,count);
	}
	else // Not a process
	{
	    // Null senders and processes can send to non-processes (ie connections)
	    if (sender==NULL || sender->isProcess() ) p->Send(message,count);
	}
	p=p->next;
    }
}


// Handles housekeeping
void OnPollTimeout()
{
    pid_t pid;
    int wstatus;
    connectionItem * p = connectionItem::head;
    char buf[128];
    
    if (sigChildSet)
    {
	pid = wait(&wstatus);
	while(p)
	{
	    p->OnWait(pid);
	    p=p->next;
	}
	sigChildSet--;

	sprintf( buf, NL "@@@ @@@ @@@ @@@ @@@" NL
                 "@@@ Received a sigChild for process %d." , pid );

	if (WIFEXITED(wstatus))
	{
	    sprintf( buf + strlen(buf), " Normal exit status = %d",
                     WEXITSTATUS(wstatus) );
	}

	if (WIFSIGNALED(wstatus))
	{
	    sprintf( buf + strlen(buf), " The process was killed by signal %d",
                     WTERMSIG(wstatus) );
	}
	strcat( buf, NL );
	SendToAll( buf, strlen(buf), NULL );
    }

    p = connectionItem::head;
    while (p)
    {
    
	if (p->IsDead())
	{
	    DeleteConnection(p);
	    // p is now invalid. 
	    // break the loop now and we will
	    // get another chance later.
	    break;
	}
	p=p->next;
    }
}

// Call this to add the item to the list of connections
void AddConnection(connectionItem * ci)
{
	if (connectionItem::head )
	{
	    ci->next=connectionItem::head;
	    ci->next->prev=ci;
	}
	else ci->next=NULL;
	
	ci->prev=NULL;
	connectionItem::head=ci;
	connectionNo++;
	PRINTF("Adding connection\n");
}


void DeleteConnection(connectionItem *ci)
{
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
	PRINTF("Deleting connection\n");
}

void OnSigChild(int)
{
	sigChildSet++;
}
void OnSigPipe(int)
{
	sigPipeSet++;
}

// Fork the daemon and exit the parent
void forkAndGo()
{
    pid_t p=fork();
    char buf[] = "/dev/null";
    int fh;
    struct stat statBuf;

    fstat( 1, &statBuf ); // Find out what stdout is

    if ( p ) // I am the parent
    {
        if ( !quiet ) {
            fprintf( stderr, "%s: spawning daemon process: %d\n", procservName, p );
            if ( logFile == NULL ) {
                if ( S_ISREG(statBuf.st_mode) )
                    fprintf( stderr, "The open file on stdout will be used as a log file.\n" );
                else
                    fprintf( stderr, "No log file specified and stdout is not a file "
                             "- no log will be kept.\n" );
            }
        }
	exit(0);
    }
    procservPid = getpid();

    // p==0
    // The daemon starts up here

    // Redirect all of the I/O away from /dev/tty
    fh = open( buf, O_RDWR );
    if ( fh<0 ) { perror( buf ); exit( -1 ); }

    if ( logFileFD == -1 && S_ISREG(statBuf.st_mode) )
    {
	logFileFD = dup(1);
    }
    close(0); close(1); close(2);
    
    dup(fh); dup(fh); dup(fh);
    close(fh);

    // Now, make sure we are not attached to a terminal
    fh = open( "/dev/tty", O_RDWR );
    if ( fh<0 ) return;         // Not a terminal
    ioctl( fh, TIOCNOTTY );     // Detatch from /dev/tty
    close( fh );
}


// Check to see if the command is really executable:
// Return 0 if the file is executable
int checkCommandFile(const char * command)
{
    struct stat s;
    int ngroups_max=1+sysconf(_SC_NGROUPS_MAX);
    gid_t * groups=new gid_t[ngroups_max];
    ngroups_max=getgroups(ngroups_max,groups);
    mode_t min_permissions=S_IRUSR|S_IXUSR|S_IXGRP|S_IRGRP|S_IROTH|S_IXOTH;

    // chdir if possible (to allow relative command)
    if ( chDir && chdir( chDir ) ) perror( chDir );

    if (stat(command,&s))
    {
	perror(command);
	return -1;
    }

    // Back to where we came from
    if ( chdir( myDir ) ) perror( myDir );

    if (!S_ISREG(s.st_mode))
    {
	fprintf(stderr,"%s: %s is not a regular file\n", procservName, command);
	return -1;
    }
    if ( min_permissions == (s.st_mode & min_permissions) ) return 0; // This is great!
    // else
    fprintf( stderr, "%s: Warning - Please change permissions on %s to at least -r-xr-xr-x\n"
             "procServ may not be able to continue without execute permission!\n",
             procservName, command);
    return 0;
}

connectionItem * connectionItem::head;
// Globals:
time_t procServStart; // Time when this IOC started
time_t IOCStart; // Time when the current IOC was started
