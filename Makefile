# Process server for soft ioc
# David H. Thompson 8/29/2003
# Ralph Lange 04/22/2008
# GNU Public License applies - see www.gnu.org

CXXFLAGS+= -g -Wall
LDLIBS += -lutil

# Change the time format ["%c"] (on console, in log files)
#CXXFLAGS+= -DSTRFTIME_FORMAT='"%b %d, %Y %r"'

# Change the minimum time between child restarts (in sec) [15]
#CXXFLAGS+= -DMIN_TIME_BETWEEN_RESTARTS=30

# Enable "--allow" option to allow access from anywhere
# !! This opens up a major security hole:
# !! anyone can access the child's stdin/stdout and - if the
# !! child permits (e.g. using "system" on iocsh) - execute
# !! arbitrary commands on the host system.   USE AT YOUR OWN RISK!!
# !! You should rather ssh to a local user and run "telnet localhost <port>"
# !! to ensure authentication.
#CXXFLAGS+= -DALLOW_FROM_ANYWHERE

procServ_OBS=procServ.o connectionItem.o acceptFactory.o clientFactory.o processFactory.o telnetStateMachine.o

all: procServ
clean:
	rm -f procServ $(procServ_OBS)


procServ.o: procServ.cc procServ.h
connectionItem.o: connectionItem.cc procServ.h
acceptFactory.o: acceptFactory.cc procServ.h
clientFactory.o: clientFactory.cc procServ.h telnetStateMachine.h
processFactory.o: processFactory.cc procServ.h
telnetStateMachine.o: telnetStateMachine.cc procServ.h telnetStateMachine.h


procServ: $(procServ_OBS)
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@
