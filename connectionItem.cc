// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange 03/18/2010
// GNU Public License (GPLv3) applies - see www.gnu.org

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "procServ.h"

// This does I/O to stdio stdin and stdout

connectionItem::connectionItem(int fd, bool readonly)
{
    _ioHandle = fd;
    _readonly = readonly;
    _markedForDeletion = false;
    _events = POLLIN|POLLPRI;
}

connectionItem::~connectionItem()
{
    PRINTF("~connectionItem()\n");

    if (_ioHandle>=0) close(_ioHandle);
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

// Called if sig child received
// default implementation: empty (only the IOC connection does implement this)
void connectionItem::markDeadIfChildIs(pid_t pid) {}
