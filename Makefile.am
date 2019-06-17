# Process Server (for CLI processes like Gateways or IOCs)
# Ralph Lange <ralph.lange@gmx.de> 2012-2019
# GNU Public License (GPLv3) applies - see www.gnu.org

bin_PROGRAMS = procServ

procServ_SOURCES = procServ.cc procServ.h \
                   connectionItem.cc acceptFactory.cc clientFactory.cc \
                   processFactory.cc processClass.h \
                   procServ.txt

LDADD = $(LIBOBJS)

DISTCLEANFILES = *~ *.orig procServ.xml docbook-xsl.css pid.txt procServ.map
MAINTAINERCLEANFILES = procServ.pdf procServ.html procServ.1
MAINTAINERCLEANFILES += manage-procs.pdf manage-procs.html manage-procs.1

EXTRA_DIST = Makefile.Epics.in forkpty.c libtelnet.c libtelnet.h

EXTRA_DIST += README.md conserver.cf.example
# procServUtils are an add-on and not build by the regular make
EXTRA_DIST += procServUtils
EXTRA_DIST += manage-procs procServ-launcher setup.py
EXTRA_DIST += systemd-procserv-generator-system systemd-procserv-generator-user

doc_DATA = AUTHORS COPYING ChangeLog NEWS README.md procServ.txt

if INSTALL_DOC

dist_man1_MANS = procServ.1
dist_doc_DATA = procServ.pdf procServ.html

if WITH_SYSTEMD_UTILS
dist_man1_MANS += manage-procs.1
dist_doc_DATA += manage-procs.pdf manage-procs.html
endif

endif # INSTALL_DOC

if BUILD_DOC

A2X_FLAGS = -a revdate=@PACKAGE_DATE@ -a revnumber=@PACKAGE_VERSION@ -D ${builddir}

procServ.1: $(srcdir)/procServ.txt
	$(A2X) $(A2X_FLAGS) -f manpage $(srcdir)/procServ.txt
procServ.pdf: $(srcdir)/procServ.txt
	$(A2X) $(A2X_FLAGS) -f pdf $(srcdir)/procServ.txt
procServ.html: $(srcdir)/procServ.txt
	$(A2X) $(A2X_FLAGS) -f xhtml $(srcdir)/procServ.txt

if WITH_SYSTEMD_UTILS
manage-procs.1: $(srcdir)/manage-procs.txt
	$(A2X) $(A2X_FLAGS) -f manpage $(srcdir)/manage-procs.txt
manage-procs.pdf: $(srcdir)/manage-procs.txt
	$(A2X) $(A2X_FLAGS) -f pdf $(srcdir)/manage-procs.txt
manage-procs.html: $(srcdir)/manage-procs.txt
	$(A2X) $(A2X_FLAGS) -f xhtml $(srcdir)/manage-procs.txt
endif

endif # BUILD_DOC

if WITH_SYSTEMD_UTILS

# Add setup for the Python procServUtils

all-local:
	(cd $(srcdir); $(PYTHON) setup.py build \
	--build-base $(shell readlink -f $(builddir))/build)

install-exec-local:
	$(PYTHON) $(srcdir)/setup.py install \
	--prefix $(DESTDIR)$(prefix)

endif # WITH_SYSTEMD_UTILS
