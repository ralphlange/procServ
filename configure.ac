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

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
