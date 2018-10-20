// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange <ralph.lange@gmx.de> 2007-2016
// GNU Public License (GPLv3) applies - see www.gnu.org


#ifndef processClassH
#define processClassH

#include "procServ.h"

#ifdef __CYGWIN__
#include <windows.h>
#endif /* __CYGWIN__ */

class processClass : public connectionItem
{
friend connectionItem * processFactory(char *exe, char *argv[]);
friend bool processFactoryNeedsRestart();
friend bool processFactoryOneShot();
friend void processFactorySendSignal(int signal);
public:
    processClass(char *exe, char *argv[]);
    void readFromFd(void);
    int Send(const char *,int);
    void markDeadIfChildIs(pid_t pid) { if (pid==_pid) _markedForDeletion=true; }
    char factoryName[100];
    virtual bool isProcess() const { return true; }
    virtual bool isLogger() const { return false; }
    static void restartOnce ();
    static bool exists() { return _runningItem ? true : false; }
    virtual ~processClass();
protected:
    pid_t _pid;
    static processClass * _runningItem;
    static time_t _restartTime;
    void terminateJob();
#ifdef __CYGWIN__
    HANDLE _hwinjob;
#endif /* __CYGWIN__ */
};


#endif /* #ifndef processClassH */
