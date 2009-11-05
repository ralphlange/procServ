# Process Server (for soft IOCs)
# David H. Thompson 8/29/2003
# Ralph Lange 11/05/2009
# GNU Public License (GPLv3) applies - see www.gnu.org

A2X = a2x

bin_PROGRAMS = procServ

procServ_SOURCES = procServ.cc procServ.h \
                   connectionItem.cc acceptFactory.cc clientFactory.cc \
                   processFactory.cc processClass.h \
                   telnetStateMachine.cc telnetStateMachine.h \
                   procServ.txt

dist_man1_MANS = procServ.1

EXTRA_DIST = procServ.pdf procServ.html
MAINTAINERCLEANFILES = ${srcdir}/procServ.pdf ${srcdir}/procServ.html ${srcdir}/procServ.1
DISTCLEANFILES = ${srcdir}/*~ ${srcdir}/procServ.xml

procServ.1: procServ.txt
	$(A2X) -D ${srcdir} -f manpage $<
procServ.pdf: procServ.txt
	$(A2X) -D ${srcdir} -f pdf $<
procServ.html: procServ.txt
	$(A2X) -D ${srcdir} -f xhtml $<
