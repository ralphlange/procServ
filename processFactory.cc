// Process server for soft ioc
// David H. Thompson 8/29/2003
// GNU Public License applies - see www.gnu.org
//

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pty.h>  /* for openpty and forkpty */ 
#include <utmp.h> /* for login_tty */ 
#include <time.h>
#include <string.h>

#include "procServ.h"


class processClass : public connectionItem
{
friend connectionItem * processFactory(int argc, char *argv[]);
friend bool processFactoryNeedsRestart();
friend void processFactorySendSignal(int signal);
public:
    processClass(int argc,char * argv[]);
    bool OnPoll();
    int Send( const char *,int);
    void OnWait(int pid);
    char factoryName[100];
    void SetupTio(struct termios *);
    virtual bool isProcess() const { return true; }
public:
    virtual ~processClass();
protected:
    pid_t _pid;
    static processClass * _runningItem;
    static time_t _restartTime;
};

processClass * processClass::_runningItem=NULL;
time_t processClass::_restartTime=0;

bool processFactoryNeedsRestart()
{
    time_t now=time(0);
    if (processClass::_runningItem || now<processClass::_restartTime ) return false;
    return true;
    
}
// service and calls clientFactory when clients are accepted
connectionItem * processFactory(int argc, char *argv[])
{
    char buf[100];
    time(&IOCStart); // When did we do this?
    
    
    if (processFactoryNeedsRestart())
    {
	sprintf( buf, "Restarting IOC %s" NL, iocName );
	SendToAll(buf,strlen(buf),0);

        if ( strcmp( iocName, argv[0] ) != 0 ) {
            sprintf( buf, "   (as %s)" NL, argv[0] );
            SendToAll(buf,strlen(buf),0);
        }

	return new processClass(argc,argv);
    }
    else
	return NULL;
}

processClass::~processClass()
{

    struct tm now_tm;
    time_t now;
    char now_buf[128];

    time(&now);
    localtime_r(&now,&now_tm);
    strftime(now_buf,sizeof(now_buf)-1,"It is now:%b %d, %Y %r" NL,&now_tm);
    
    char * goodbye="Closing the IOC, a new one will be restarted shortly" NL;
    SendToAll(goodbye,strlen(goodbye),this);    
    SendToAll(now_buf,strlen(now_buf),this);    
    if (_pid>0) kill(-_pid,SIGKILL); // Negative PID sends signal to all members of process group
    if (_ioHandle>0) close(_ioHandle);
    _runningItem=NULL;
}


// Accept item constuctor
// This opens a socket and binds it to the decided port
processClass::processClass(int argc,char * argv[])
{

    _runningItem=this;
    struct termios tio;
    SetupTio(&tio);
    _pid=forkpty(&_ioHandle,factoryName,&tio,NULL);
    char buf[100];

    _markedForDeletion=_pid<=0;
    if (_pid) // I am the parent
    {
	PRINTF("Created process %d on %s\n",_pid,factoryName);
	// Don't start a new one before this time:
	_restartTime=15+time(0);
	sprintf(infoMessage2,"The PID of IOC %s is now: %d" NL, iocName, _pid);

	sprintf(buf,"The new PID of IOC %s is now: %d" NL, iocName, _pid);

	strcat(buf,"@@@@@@@@@@@@@" NL);
	SendToAll(buf,strlen(buf),this);
    }
    else // I am the child 
    {
	setpgrp(); // The child is head of its own process group
	int status=execv(*argv,argv); // This wont return
	// It did...
	printf("Could not execute: %s, %s\n",*argv,strerror(errno));
	kill(daemon_pid,SIGUSR1); // Let someone know
	sleep(10); // Dont do anything harmful
	exit(-1);
    }
}

// OnPoll is called after a poll returns non-zero in events
// return 0 normally
// may modify pfd-revents as needed
bool processClass::OnPoll()
{
    if (_pfd==NULL || _pfd->revents==0 ) return false;
    // Otherwise process the revents and return true;

    char  buf[1600];

    if (_pfd->revents&(POLLIN|POLLPRI))
    {
	int len=read(_ioHandle,buf,sizeof( buf)-1);
	if (len>=0)
	{
	    buf[len]='\0';
	}
	if (len<1)
	{
	    _markedForDeletion=true;
	}
	else
	{
	    buf[len]='\0';
	    SendToAll(&buf[0],len,this);
	}

    }
    if (_pfd->revents&(POLLHUP|POLLERR))
    {
	PRINTF("ProcessItem:: Got hangup or error \n");
	_markedForDeletion=true;
    }
    if (_pfd->revents&POLLNVAL)
    {
	_markedForDeletion=true;
    }
    return true;
}


// Send characters to clients
int processClass::Send( const char * buf,int count)
{
    int status=0;

    int i;
    for (i=0;i<count;i++) if (buf[i]==('X'&0x1f)) processFactorySendSignal(SIGKILL);
    if (count>0)
    {
	status=write(_ioHandle,buf,count);
	if (status<0) _markedForDeletion=true;
    }
    return status;
}

// This gets called if a SIGCHLD was received by the main thread
void processClass::OnWait(pid_t pid)
{

    if (pid==_pid)
    {
	_markedForDeletion=true;
    }
}

// The telnet state machine can call this to blast a running
// client IOC
void processFactorySendSignal(int signal)
{
    if (processClass::_runningItem)
    {
	PRINTF("Sending signal %d to pid %d\n",
		signal,processClass::_runningItem->_pid);
	kill(-processClass::_runningItem->_pid,signal);
    }
}

#define CC(c) (c&0x1f)

void processClass::SetupTio(struct termios *tio)
{
    tio->c_iflag=   IXON ; 
    tio->c_oflag=OPOST|ONLCR|NL0|CR0|TAB0|BS0|FF0|VT0 ;
    tio->c_cflag=   B38400|CS8|CREAD   ;        
    tio->c_lflag= ISIG|ICANON|IEXTEN |ECHO|ECHONL;
    tio->c_line=0; // Line dicipline 0 
    tio->c_cc[VINTR ]=CC('C');
    tio->c_cc[VQUIT ]=CC('\\');
    tio->c_cc[VERASE ]=0x7f;
    tio->c_cc[VKILL ]=CC('U');
    tio->c_cc[VEOF ]=CC('D');
    tio->c_cc[VTIME ]=0;
    tio->c_cc[VMIN ]=1;
    tio->c_cc[VSWTC ]=0;
    tio->c_cc[VSTART ]=CC('Q');
    tio->c_cc[VSTOP ]=CC('S');
    tio->c_cc[VSUSP ]=CC('Z');
    tio->c_cc[VEOL ]=0;
    tio->c_cc[VREPRINT ]=CC('R');
    tio->c_cc[VDISCARD ]=0; // CC('');
    tio->c_cc[VWERASE ]=CC('W');
    tio->c_cc[VLNEXT ]=CC('V');
    tio->c_cc[VEOL2 ]=0;
     tio->c_ispeed=B38400;
    tio->c_ospeed=B38400;
}

bool processClass::isProcess() const  { return true; };
