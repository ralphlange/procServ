# Process server for soft ioc
# David H. Thompson 8/29/2003
# Ralph Lange 04/07/2008
# GNU Public License applies - see www.gnu.org

CXXFLAGS+= -g
LDLIBS += -lutil

# If you don't like the time format (on console, in log files), change it here
#CXXFLAGS+= -DSTRFTIME_FORMAT='"%b %d, %Y %r"'

procServ_OBS=procServ.o connectionItem.o acceptFactory.o clientFactory.o processFactory.o telnetStateMachine.o logBuffer.o

all: procServ
clean:
	rm -f procServ $(procServ_OBS)


procServ.o: procServ.cc procServ.h
connectionItem.o: connectionItem.cc procServ.h
acceptFactory.o: acceptFactory.cc procServ.h
clientFactory.o: clientFactory.cc procServ.h telnetStateMachine.h
processFactory.o: processFactory.cc procServ.h
telnetStateMachine.o: telnetStateMachine.cc procServ.h telnetStateMachine.h
logBuffer.o: logBuffer.cc logBuffer.h



procServ: $(procServ_OBS)
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@
