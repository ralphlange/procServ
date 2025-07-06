# Process Server (for CLI processes like Gateways or IOCs)
# Ralph Lange <ralph.lange@gmx.de> 2012-2019
# GNU Public License (GPLv3) applies - see www.gnu.org

bin_PROGRAMS = procServ

procServ_SOURCES = procServ.cc procServ.h \
                   connectionItem.cc acceptFactory.cc clientFactory.cc \
                   processFactory.cc processClass.h \
                   procServ.md

LDADD = $(LIBOBJS)

DISTCLEANFILES = *~ *.orig procServ.xml docbook-xsl.css pid.txt procServ.map
MAINTAINERCLEANFILES = procServ.pdf procServ.html procServ.1
MAINTAINERCLEANFILES += manage-procs.pdf manage-procs.html manage-procs.1

EXTRA_DIST = Makefile.Epics.in forkpty.c libtelnet.c libtelnet.h

EXTRA_DIST += README.md conserver.cf.example
# procServUtils are an add-on and not build by the regular make
EXTRA_DIST += procServUtils
EXTRA_DIST += systemd-procserv-generator-system systemd-procserv-generator-user

doc_DATA = AUTHORS COPYING ChangeLog NEWS README.md

if INSTALL_DOC

dist_man1_MANS = procServ.1
dist_doc_DATA = procServ.md procServ.pdf procServ.html

if WITH_SYSTEMD_UTILS
dist_man1_MANS += manage-procs.1
dist_doc_DATA += manage-procs.pdf manage-procs.html
endif

endif # INSTALL_DOC

if BUILD_DOC

procServ.1: $(srcdir)/procServ.md
	$(PANDOC) -s -f gfm -t man $< -o $@
procServ.pdf: $(srcdir)/procServ.md
	$(PANDOC) -s -f gfm -t pdf $< -o $@
procServ.html: $(srcdir)/procServ.md
	$(PANDOC) -s -f gfm -t html $< -o $@

if WITH_SYSTEMD_UTILS
manage-procs.1: $(srcdir)/manage-procs.md
	$(PANDOC) -s -f gfm -t man $< -o $@
manage-procs.pdf: $(srcdir)/manage-procs.md
	$(PANDOC) -s -f gfm -t pdf $< -o $@
manage-procs.html: $(srcdir)/manage-procs.md
	$(PANDOC) -s -f gfm -t html $< -o $@
endif

endif # BUILD_DOC

if WITH_SYSTEMD_UTILS

# Add setup for the Python procServUtils

all-local:
	(cd $(srcdir); rm -rf dist)
	(cd $(srcdir); $(PYTHON) -m build)

install-exec-local:
	(cd $(srcdir); $(PYTHON) -m pip install dist/procServUtils*.whl --force-reinstall --break-system-packages)

endif # WITH_SYSTEMD_UTILS
