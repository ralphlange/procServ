#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([procServ Process Server], [1.5.0], [tech-talk@aps.anl.gov], [procServ],
	[http://www-csr.bessy.de/control/SoftDist/procServ/])

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

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lutil':
AC_CHECK_LIB([util], [forkpty], [], [AC_MSG_ERROR([Missing required library])])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netinet/in.h sys/ioctl.h sys/socket.h sys/time.h termios.h utmp.h pty.h],[],
	[AC_MSG_ERROR([Missing required header(s)])])

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
AC_FUNC_MALLOC
AC_FUNC_SETPGRP
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([localtime_r socket strdup strerror],[],[AC_MSG_ERROR([Missing required function(s)])])

# Add configure option for time string format
AC_ARG_ENABLE([time-format],
              [AS_HELP_STRING([--enable-time-format="<strftime format string>"],
                              [set strftime format for output and log ["%c"]])],
              [],
              [enable_time_format=no])
AS_IF([test "x$enable_time_format" != xno],
      [AC_DEFINE_UNQUOTED([STRFTIME_FORMAT], "$enable_time_format",
                 [Define to set time format])])

# Add configure option for holdoff time between child restarts
AC_ARG_ENABLE([restart-holdoff-time],
              [AS_HELP_STRING([--enable-restart-holdoff-time=<time>],
                              [set holdoff time (in s) between child process restarts [15]])],
              [],
              [enable_restart_holdoff_time=no])
AS_IF([test "x$enable_restart_holdoff_time" != xno],
      [AC_DEFINE_UNQUOTED([RESTART_HOLDOFF_TIME], [$enable_restart_holdoff_time],
                 [Define to set holdoff time between child restarts])])

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
                              [Do not install documentation])],
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
])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
