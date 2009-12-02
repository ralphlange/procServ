# Process Server (for soft IOCs)
# David H. Thompson 8/29/2003
# Ralph Lange 11/05/2009
# GNU Public License (GPLv3) applies - see www.gnu.org

# Last resort rules if Makefile is not present

ifeq ($(wildcard Makefile),Makefile)
include Makefile
else

AUTORECONF=autoreconf
HG=hg

all:	clean
	$(AUTORECONF) -si

clean distclean maintainer-clean:
	@echo Cleaning autotools debris
	@rm -rf build-aux autom4te.cache
	@if $(HG) --version &>/dev/null; then \
	  $(HG) status -i -n | xargs rm -f; \
	else \
	  rm -f configure aclocal.m4 Makefile.in Makefile.Automake.in; \
	fi

endif
