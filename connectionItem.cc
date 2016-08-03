// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange <ralph.lange@gmx.de> 2007-2016
// GNU Public License (GPLv3) applies - see www.gnu.org

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "procServ.h"

// This does I/O to stdio stdin and stdout

connectionItem::connectionItem(int fd, bool readonly)
{
    _fd = fd;
    _readonly = readonly;
    _markedForDeletion = false;
}

connectionItem::~connectionItem()
{
    PRINTF("~connectionItem()\n");
    if (_fd >= 0) close(_fd);
}

// Called if sig child received
// default implementation: empty (only the IOC connection does implement this)
void connectionItem::markDeadIfChildIs(pid_t pid) {}
