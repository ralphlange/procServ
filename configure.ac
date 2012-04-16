#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# Process Server (for soft IOCs)
# Ralph Lange 04/16/2012
# GNU Public License (GPLv3) applies - see www.gnu.org


AC_PREREQ([2.61])
AC_INIT([procServ Process Server], [2.6.0], [tech-talk@aps.anl.gov], [procServ],
	[http://sourceforge.net/projects/procserv/])
AC_SUBST([PACKAGE_DATE],[04/16/2012])

AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([1.10 -Wall])

AC_CONFIG_SRCDIR([connectionItem.cc])
# we don't need config.h for now
#AC_CONFIG_HEADER([config.h])

CFLAGS="$CFLAGS -Wall"
CXXFLAGS="$CXXFLAGS -Wall"

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for header files.
#AC_CHECK_HEADERS([arpa/inet.h fcntl.h netinet/in.h sys/ioctl.h sys/socket.h sys/time.h termios.h utmp.h pty.h],[],
#	[AC_MSG_ERROR([Missing required header(s)])])
AC_CHECK_HEADERS([pty.h libutil.h util.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_C_RESTRICT
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_GETGROUPS
AC_PROG_GCC_TRADITIONAL
AC_PROG_RANLIB
AC_FUNC_STRFTIME
AC_CHECK_LIB([telnet], [telnet_recv],,[AC_LIBOBJ([libtelnet])])
AC_SEARCH_LIBS([socket], [socket])
AC_SEARCH_LIBS([setsockopt], [socket])
AC_SEARCH_LIBS([inet_aton], [resolv])
AC_SEARCH_LIBS([forkpty], [util])
AC_REPLACE_FUNCS([forkpty])

# Add configure option for access from anywhere
AC_ARG_ENABLE([access-from-anywhere],
              [AS_HELP_STRING([--enable-access-from-anywhere],
                              [allow unauthenticated access from anywhere \
                               (CAVEAT: opens a security hole!!)])],
              [],
              [enable_access_from_anywhere=no])
AS_IF([test "x$enable_access_from_anywhere" != xno],
      [AC_DEFINE([ALLOW_FROM_ANYWHERE], [1],
                 [Define to allow access from anywhere])])

# Add configure option for creating EPICS build system Makefile
AC_ARG_WITH([epics-top],
              [AS_HELP_STRING([--with-epics-top=TOP],
                              [create Makefile for EPICS build system: relative path to TOP])],
              [],
              [with_epics_top=no])
AS_IF([test "x$with_epics_top" != xno],
      [if test "x$with_epics_top" = xyes; then
         with_epics_top=../..
       fi
       use_epics_makefile=yes
       a=`pwd`; b=`echo ${a##*/} | tr . -`; c=${a%/*}/${b%%App}App
       if test "x$a" != "x$c"; then
         echo "renaming directory to fit the EPICS build rules"
         mv $a $c
       fi
       AC_SUBST([EPICS_TOP],[$with_epics_top])
      ])
AM_CONDITIONAL([WITH_EPICS], [test x$with_epics_top != xno])

AC_ARG_ENABLE([devel],
              [AS_HELP_STRING([--enable-devel],
                              [Control build type: debug (enable), release (disable), default (none)])],
              [], [])
AS_IF([test x$enable_devel = xyes], [
  CFLAGS="$CFLAGS -Werror"
  CXXFLAGS="$CXXFLAGS -Werror"
  CPPFLAGS="$CPPFLAGS -DDEBUG"
  ])
  

# Skip building documentation

AC_ARG_ENABLE([doc],
              [AS_HELP_STRING([--disable-doc],
                              [Do not install documentation])
AS_HELP_STRING([--enable-doc],
                              [Build (and install) documentation])],
              [],[])
# If $enable_doc is:
#  no              - Don't build or install generated documentation.
#  empty (default) - Install generated docs, but don't build.
#                    Assumes they are already exist (ie dist. tarball)
#  yes             - build and install generated documentation.

AM_CONDITIONAL([INSTALL_DOC], [test x$enable_doc != xno])
AM_CONDITIONAL([BUILD_DOC], [test x$enable_doc = xyes])

AC_CHECK_PROGS(A2X, a2x)

# These are used by a2x and must be present for it to function
AC_CHECK_PROGS(ASCIIDOC, asciidoc)
AC_CHECK_PROGS(XMLLINT, xmllint)
AC_CHECK_PROGS(XSLTPROC, xsltproc)
# a2x will use one of these
AC_CHECK_PROGS(PDFGEN, fop dblatex)

AS_IF([test x$enable_doc = xyes], [
  AS_IF([test x$A2X = x],[AC_MSG_ERROR([Missing a2x (from asciidoc)])],
        [test x$ASCIIDOC = x],[AC_MSG_ERROR([Missing asciidoc])],
        [test x$XMLLINT = x],[AC_MSG_ERROR([Missing xmllint])],
        [test x$XSLTPROC = x],[AC_MSG_ERROR([Missing xsltproc])],
        [test x$PDFGEN = x],[AC_MSG_ERROR([Missing a docbook to pdf converter (fop or dblatex)])]
  )
],[test x$enable_doc != xno], [
  AS_IF([test -f ${srcdir}/procServ.1 -a -f ${srcdir}/procServ.pdf -a -f ${srcdir}/procServ.html],[],
    [AC_MSG_ERROR([Documentation not found.  Use --disable-doc to skip installing it, or --enable-doc to generate it])])
])

# Clumsy workaround (waiting for AM_COND_IF in Automake 1.10.3) to use EPICS Makefile.in
# Careful: while in "EPICS mode" you must remove Makefile.Automake.in before manually running autoreconf.
# Better:  Run "make maintainer-clean" then "make" which will clean up then autoreconf.
AC_CONFIG_FILES([Makefile],
                [],
                [if test ! -e Makefile.Automake.in; then
                   mv Makefile.in Makefile.Automake.in
                 fi
                 if test x$use_epics_makefile = xyes; then
                   cp Makefile.Epics.in Makefile.in
                 else
                   mv Makefile.Automake.in Makefile.in
                 fi
                ])
AC_OUTPUT
