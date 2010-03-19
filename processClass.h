// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange 03/18/2010
// GNU Public License (GPLv3) applies - see www.gnu.org


#ifndef processClassH
#define processClassH

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
    void markDeadIfChildIs(pid_t pid);
    char factoryName[100];
    virtual bool isProcess() const { return true; }
    static void restartOnce ();
    static bool exists() { return _runningItem ? true : false; }
    virtual ~processClass();
protected:
    pid_t _pid;
    static processClass * _runningItem;
    static time_t _restartTime;
};


#endif /* #ifndef processClassH */
