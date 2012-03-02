// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange 03/02/2012
// GNU Public License (GPLv3) applies - see www.gnu.org

#ifndef telnetStateMachineH
#define telnetStateMachineH

#include <arpa/telnet.h>

#ifndef NTELOPTS                            // Fix for arpa/telnet.h needed on Solaris
#define NTELOPTS (1+TELOPT_NEW_ENVIRON)
#endif

int TelnetStateMachine(char * buf,int len); // Returns new length
#define OPT_STRING_LEN 32

class telnetStateMachine
{
public:
    telnetStateMachine();
    void SetConnectionItem(connectionItem * item)
    {
	_item=item;
	sendInitialRequests();
    }
    int OnReceive(char * buf,int len,bool priority=false);

private:
    signed char _buf[128]; // Where to put data after an IAC
    int _count; // This is normally -1, after IAC it is 0 + chars received
    int _expected; // When to process the command
    connectionItem * _item; // This is the client that owns us

    // 0==> I don't care
    // The constructor will set the ones that I want to negotiate
    // If the server wants to negotiate then he will probably get a wont or dont
    unsigned char _myOpts[NTELOPTS]; // Either I will, I wont, I do, I dont
    unsigned char _otherOpts[NTELOPTS]; // Will he, wont he, he does, he does not.
    unsigned char _myOptArg[NTELOPTS][OPT_STRING_LEN]; // Either I will, I wont, I do, I dont
    unsigned char _otherOptArg[NTELOPTS][OPT_STRING_LEN]; // Will he, wont he, he does, he does not.
    unsigned char _myOptArgLen[NTELOPTS]; // Either I will, I wont, I do, I dont
    unsigned char _otherOptArgLen[NTELOPTS]; // Will he, wont he, he does, he does not.

private: // Methods
    void sendReply( int opt0, int opt1=-1,int opt2=-1, int opt3=-1,int opt4=-1);
    void sendInitialRequests();
    void debugMsg(unsigned char c, unsigned char o);
    void debugMsg(unsigned char c);
    bool onChar(unsigned char c);
    
    enum state
    {
        ON_START,
	ON_IAC,
	ON_DO,
	ON_DONT,
	ON_WILL,
	ON_WONT,
	ON_SB,
	ON_SE,
	ON_DATA,
	ON_END	
    } _myState;
    unsigned char _workingOpt; // Used in state ON_OPT
};

#endif /*#ifndef telnetStateMachineH */
