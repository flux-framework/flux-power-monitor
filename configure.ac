#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([flux-pwr-mgr], [0.0.1], [nkulshr@clemson.edu])

AC_CONFIG_SRCDIR([NEWS.md])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])

AM_INIT_AUTOMAKE([subdir-objects tar-pax foreign])
AM_SILENT_RULES([yes])
AC_CONFIG_HEADERS([config/config.h])

AC_DEFINE([_GNU_SOURCE], 1,
          [Define _GNU_SOURCE so that we get all necessary prototypes])
AC_DEFINE([BUILD_MODULE],1,[Define A module is getting build for flux])
PKG_PROG_PKG_CONFIG

# Checks for programs.
m4_version_prereq(2.70, [AC_PROG_CC], [AC_PROG_CC_C99])


AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL

#Initialize Libtool
LT_INIT

AC_ARG_WITH([variorum],
            [AS_HELP_STRING([--with-variorum=DIR],
                            [path to Variorum library])],
            [
              VARIORUM_LIB="-L$withval/lib"
              VARIORUM_INCLUDE="-I$withval/include"
              CPPFLAGS="$CPPFLAGS $VARIORUM_INCLUDE"
              LDFLAGS="$LDFLAGS $VARIORUM_LIB"
              AC_CHECK_LIB([variorum], [variorum_get_node_power_json],
                           [],
                           [AC_MSG_ERROR([Function variorum_get_power is required in Variorum library.])])
            ],
            [AC_MSG_ERROR([Variorum library is required. Please use --with-variorum to specify its location.])])
CPPFLAGS="$CPPFLAGS_save"
LDFLAGS="$LDFLAGS_save"
AX_FLUX_CORE

# Checks for header files.
AC_CHECK_HEADERS([stdint.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gethostname])

AC_CONFIG_FILES(\
                Makefile \
                src/flux_pwr_monitor/Makefile \
                src/common/Makefile
              )
AC_OUTPUT
