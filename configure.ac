#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(procServ Process Server, 1.5.0, tech-talk@aps.anl.gov, procServ)
AM_INIT_AUTOMAKE

AC_CONFIG_SRCDIR([connectionItem.cc])
# we don't need config.h for now
#AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lutil':
AC_CHECK_LIB([util], [forkpty])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netinet/in.h sys/ioctl.h sys/socket.h sys/time.h termios.h utmp.h pty.h])

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
AC_CHECK_FUNCS([localtime_r socket strdup strerror])

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
      [AC_DEFINE_UNQUOTED([MIN_TIME_BETWEEN_RESTARTS], [$enable_restart_holdoff_time],
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

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
