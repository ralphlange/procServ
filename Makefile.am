# Process Server (for soft IOCs)
# Ralph Lange 04/16/2012
# GNU Public License (GPLv3) applies - see www.gnu.org

bin_PROGRAMS = procServ

procServ_SOURCES = procServ.cc procServ.h \
                   connectionItem.cc acceptFactory.cc clientFactory.cc \
                   processFactory.cc processClass.h \
                   procServ.txt

LDADD = $(LIBOBJS)

DISTCLEANFILES = *~ *.orig procServ.xml docbook-xsl.css pid.txt
MAINTAINERCLEANFILES = procServ.pdf procServ.html procServ.1

doc_DATA = AUTHORS COPYING ChangeLog NEWS README procServ.txt

if INSTALL_DOC

dist_man1_MANS = procServ.1

doc_DATA += procServ.pdf procServ.html
EXTRA_DIST = $(doc_DATA) Makefile.Epics.in forkpty.c libtelnet.c libtelnet.h

endif # INSTALL_DOC

if BUILD_DOC

A2X_FLAGS = -a revdate=@PACKAGE_DATE@ -a revnumber=@PACKAGE_VERSION@ -D ${builddir}

procServ.1: $(srcdir)/procServ.txt
	$(A2X) $(A2X_FLAGS) -f manpage $(srcdir)/procServ.txt
procServ.pdf: $(srcdir)/procServ.txt
	$(A2X) $(A2X_FLAGS) -f pdf $(srcdir)/procServ.txt
procServ.html: $(srcdir)/procServ.txt
	$(A2X) $(A2X_FLAGS) -f xhtml $(srcdir)/procServ.txt

endif # BUILD_DOC
