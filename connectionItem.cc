// Process server for soft ioc
// David H. Thompson 8/29/2003
// GNU Public License applies - see www.gnu.org

#include "procServ.h"
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
// This does I/O to stdio stdin and stdout

connectionItem::connectionItem(int fd)
{
    _ioHandle=fd;
    _markedForDeletion=false;
    _events=POLLIN|POLLPRI;
}

connectionItem::~connectionItem()
{
    XPRINTF("~connectionItem()\n");

    if (_ioHandle>=0) close(_ioHandle);
}

// OnPoll is called after a poll returns non-zero in events
// return false if the object should be deleted.
// return true normally
// may modify pfd-revents as needed
bool connectionItem::OnPoll()
{
    char buf[1600];
    int len;

    if (_pfd==NULL || _pfd->revents==0 ) return false;

    // Otherwise process the revents and return true;

    if (_pfd->revents&(POLLIN|POLLPRI))
    {
	len=read(_ioHandle,&buf,sizeof(buf)-1);
	if (len<1)
	{
	    _markedForDeletion=true;
	}
	else 
	{
	    buf[len]='\0';
	    SendToAll(buf,len,this);
	}
    }
    if (_pfd->revents&(POLLHUP|POLLERR))
    {
	XPRINTF("ConnectionItem:: Got hangup or error \n");
	_markedForDeletion=true;
    }
    if (_pfd->revents&POLLNVAL)
    {
        XPRINTF("Got invalid handle %d\n",_ioHandle);
	_ioHandle=-1;
	_markedForDeletion=true;
    }
    return true;
}


bool connectionItem::SetPoll(struct pollfd * pfd)
{
    // Do we need to be polled
    if (_markedForDeletion || _ioHandle<1 )
    {
        _pfd=NULL; // This prevents OnPoll from processing
    	return false; // This prevents this item from being counted
    }
    // else copy the data into the poll descriptor
    _pfd=pfd;
    pfd->fd=_ioHandle;
    pfd->events=_events;
    return true;    // and count this in npoll
}

void connectionItem::OnWait(int pid)
{
}
bool connectionItem::isProcess() const { return false; }
