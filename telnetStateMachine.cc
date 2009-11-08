// Process server for soft ioc
// David H. Thompson 8/29/2003
// Ralph Lange 04/22/2008
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
    
    for(;len>0; len--,pi++)
    {
	if (onChar(*pi)) *po++=*pi; // copy the character
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
		onDo(c);
	    }
	    _myState=ON_START;	    
	    break;
	case ON_DONT:
	    if (TELOPT_OK(c))
	    {
	        _myOpts[c]=DONT;
		onDont(c);
	    }
	    _myState=ON_START;	    
	    break;
	case ON_WILL:
	    if (TELOPT_OK(c))
	    {
	        _otherOpts[c]=WILL;
		onWill(c);
	    }
	    _myState=ON_START;	    
	    break;
	case ON_WONT:
	    if (TELOPT_OK(c))
	    {
	        _otherOpts[c]=WONT;
		onWont(c);
	    }
	    _myState=ON_START;	    
	    break;
	case ON_SB:
	    if (TELOPT_OK(c))
	    {
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
	    onOpt(c);
	    _myState=ON_START;
	    break;

	default:
	    PRINTF("telent state machine invalid state\n");
	    _myState=ON_START;
	    break;	
	}
	return rv;
}



void telnetStateMachine::onDo(unsigned char opt)
{
    PRINTF("telnetStateMachine::onDo(%d)\n",opt);
}
void telnetStateMachine::onDont(unsigned char opt)
{
    PRINTF("telnetStateMachine::onDont(%d)\n",opt);
}
void telnetStateMachine::onWill(unsigned char opt)
{
    PRINTF("telnetStateMachine::onWill(%d)\n",opt);
}
void telnetStateMachine::onWont(unsigned char opt)
{
    PRINTF("telnetStateMachine::onWont(%d)\n",opt);
}
void telnetStateMachine::onOpt(unsigned char opt)
{
    PRINTF("telnetStateMachine::onOpt(%d)\n",opt);
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

void telnetStateMachine::doCommand() // The state is in _buf[]/_count;
{
    if (TELCMD_OK(_buf[0]))
    {
	switch((unsigned char)_buf[0])
	{
	case WILL:
	case WONT:
	    PRINTF("telnetStateMachine::doCommand() %s %s \n",TELCMD(_buf[0]),TELOPT(_buf[1]) );
	    _otherOpts[_buf[1]]=_buf[0];
	    break;
	case DO:
	case DONT:
	    PRINTF("telnetStateMachine::doCommand() %s %s \n",TELCMD(_buf[0]),TELOPT(_buf[1]) );
	    _myOpts[_buf[1]]=_buf[0];
	    break;
	case SB:
	    PRINTF("telnetStateMachine::doCommand() %s %s \n",TELCMD(_buf[0]),TELOPT(_buf[1]) );
	    break;
	default:
	    PRINTF("telnetStateMachine::doCommand() %s\n",TELCMD(_buf[0])) ;
	    break;

	}
    }
    // This terminates the command.
    _count=-1;
}
void telnetStateMachine::sendInitialRequests()
{
	int i;
	for (i=0;i<NTELOPTS;i++)
	{
	    if (_otherOpts[i]==DO)
	    {
		sendReply(DO,i);
	    }
	    else if (_otherOpts[i]==DONT)
	    {
		sendReply( DONT,i);
	    }

	}
	for (i=0;i<NTELOPTS;i++)
	{
	    if (_myOpts[i]==WILL)
	    {
		sendReply( WILL,i);
	    }
	    else if (_myOpts[i]==WONT)
	    {
		sendReply( WONT,i);
	    }

	}
}
