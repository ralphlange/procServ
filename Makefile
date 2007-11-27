# Process server for soft ioc
# David H. Thompson 8/29/2003
# GNU Public License applies - see www.gnu.org

CXXFLAGS+= -g
LDLIBS += -lutil

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
