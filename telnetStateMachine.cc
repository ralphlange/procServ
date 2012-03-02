// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange 03/02/2012
// GNU Public License (GPLv3) applies - see www.gnu.org

#include "procServ.h"
#define TELOPTS
#define TELCMDS
#include "telnetStateMachine.h"
#include <arpa/telnet.h>

// This interprets telnet commands 

// Constructor, clear the data and then set the options for
// ourselves and the peer
telnetStateMachine::telnetStateMachine()
{
    int i;

    _item=NULL;
    _count=-1; // not receiving

    for (i=0;i<NTELOPTS;i++)
    {
	_myOpts[i]=0;
	_otherOpts[i]=0;
	_myOptArgLen[i]=0;
	_otherOptArgLen[i]=0;
    }
    _otherOpts[TELOPT_LINEMODE]=DO;
    _myOpts[TELOPT_ECHO]=WILL;
    _myOpts[TELOPT_NAOCRD]=WILL;
    _myState=ON_START;
}

// Returns new length of the buffer, assumes the buffer may be 
// longer than given
int telnetStateMachine::OnReceive(char * buf,int len,bool priority)
{
    unsigned char * pi,*po; 
    pi=po=(unsigned char *) buf;
    
    for (; len > 0; len--, pi++) {
        if (onChar(*pi)) {
            *po++ = *pi;
        }
    }

    return (po-(unsigned char *) buf);
}

bool telnetStateMachine::onChar(unsigned char c)
{
	bool rv=false; // if rv is true the character is copied through
	
	switch(_myState)
	{
        case ON_START:
	    if (c == IAC)
	        _myState=ON_IAC;
	    else
	        rv=true;
	    
	    break;
	case ON_IAC:
	    switch(c)
	    {
	    case DO:
	    	_myState=ON_DO;
		break;
	    case DONT:
	    	_myState=ON_DONT;
		break;
	    case WILL:
	    	_myState=ON_WILL;
		break;
	    case WONT:
	    	_myState=ON_WONT;
		break;
	    case SB:
	    	_myState=ON_SB;
		break;
	    default:
	        _myState=ON_START;
	    	break;
	    }
	    break;
	case ON_DO:
	    if (TELOPT_OK(c))
        {
	        _myOpts[c]=DO;
            debugMsg(DO, c);
	    }
	    _myState=ON_START;	    
	    break;
	case ON_DONT:
	    if (TELOPT_OK(c))
	    {
	        _myOpts[c]=DONT;
            debugMsg(DONT, c);
	    }
	    _myState=ON_START;	    
	    break;
	case ON_WILL:
	    if (TELOPT_OK(c))
	    {
	        _otherOpts[c]=WILL;
            debugMsg(WILL, c);
	    }
	    _myState=ON_START;	    
	    break;
	case ON_WONT:
	    if (TELOPT_OK(c))
	    {
	        _otherOpts[c]=WONT;
            debugMsg(WONT, c);
	    }
	    _myState=ON_START;	    
	    break;
	case ON_SB:
	    if (TELOPT_OK(c))
	    {
            debugMsg(SB);
            _workingOpt=c;
		_otherOptArgLen[_workingOpt]=0;
		_myState=ON_DATA;
		
	    }
	    else _myState=ON_START;
	    break;
	case ON_DATA:
	    if (c==IAC) _myState=ON_END;
	    else if (_otherOptArgLen[_workingOpt]<OPT_STRING_LEN)
	        _otherOptArg[ _workingOpt][_otherOptArgLen[_workingOpt]++]=c;
	    break;
	case ON_END:	
	    // No matter what happens go to state ON_START
	    // this should be an SE
        debugMsg(SE);
	    _myState=ON_START;
	    break;

	default:
        PRINTF("telnet(item %p) invalid state\n", _item);
	    _myState=ON_START;
	    break;	
	}
	return rv;
}

void telnetStateMachine::debugMsg(unsigned char c, unsigned char o)
{
    PRINTF("telnet(item %p) RCVD %s %s\n", _item, TELCMD(c), TELOPT(o));
}

void telnetStateMachine::debugMsg(unsigned char c)
{
    PRINTF("telnet(item %p) RCVD %s\n", _item, TELCMD(c));
}

void telnetStateMachine::sendReply(int opt0, int opt1,int opt2, int opt3,int opt4)
{
    unsigned char buf[6];
    buf[0]=IAC;
    buf[1]=opt0;
    if (opt1<0) { _item->Send((char *) buf,2); return; }
    buf[2]=opt1;
    if (opt2<0) { _item->Send((char *) buf,3); return; }
    buf[3]=opt2;
    if (opt3<0) { _item->Send((char *) buf,4); return; }
    buf[4]=opt3;
    if (opt4<0) { _item->Send((char *) buf,5); return; }
    buf[5]=opt4;
    { _item->Send((char *) buf,6); return; }
}

void telnetStateMachine::sendInitialRequests()
{
	int i;

    for (i = 0; i < NTELOPTS; i++) {
        if (DO == _otherOpts[i]) {
            sendReply(DO, i);
            PRINTF("telnet(item %p) SENT DO %s\n", _item, TELOPT(i));
        } else if (DONT == _otherOpts[i]) {
            sendReply(DONT, i);
            PRINTF("telnet(item %p) SENT DONT %s\n", _item, TELOPT(i));
        }
    }

    for (i = 0; i < NTELOPTS; i++) {
        if (WILL == _myOpts[i]) {
            sendReply(WILL, i);
            PRINTF("telnet(item %p) SENT WILL %s\n", _item, TELOPT(i));
        } else if (WONT == _myOpts[i]) {
            sendReply(WONT, i);
            PRINTF("telnet(item %p) SENT WONT %s\n", _item, TELOPT(i));
        }
    }
}
