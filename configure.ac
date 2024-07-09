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
CFLAGS="-O0 -g"

CPPFLAGS_save="$CPPFLAGS -std=c11"
LDFLAGS_save="$LDFLAGS"

AC_ARG_WITH([variorum],
            [AS_HELP_STRING([--with-variorum=DIR],
                            [path to Variorum library])],
            [
              VARIORUM_LIB="-L$withval/lib -Wl,--rpath=$withval/lib"
              VARIORUM_INCLUDE="-I$withval/include"
              AC_SUBST([VARIORUM_INCLUDE])
              AC_SUBST([VARIORUM_LIB])
              CPPFLAGS="$CPPFLAGS $VARIORUM_INCLUDE"
              LDFLAGS="$LDFLAGS $VARIORUM_LIB"
         #     AC_MSG_ERROR([Variorum path is $withval/lib])
             # AC_CHECK_LIB([variorum], [variorum_get_power_json],
             #              [],
             #              [AC_MSG_ERROR([Function variorum_get_power_json is required in Variorum library.])])
            ],
           [AC_MSG_ERROR([Variorum library is required. Please use --with-variorum to specify its location.])]
         )
AC_MSG_NOTICE([variorum cppflags : ${CPPFLAGS}])
AC_MSG_NOTICE([variorum ldflags : ${LDFLAGS}])
AC_MSG_NOTICE([variorum cppflags : ${VARIORUM_INCLUDE}])
AC_MSG_NOTICE([variorum ldflags : ${VARIORUM_LIB}])

#CPPFLAGS="$CPPFLAGS_save $VARIORUM_INCLUDE"
#LDFLAGS="$LDFLAGS_save $VARIORUM_LIB"
AC_ARG_ENABLE([ubsan],
  [AS_HELP_STRING([--enable-ubsan], [Enable Undefined Behavior Sanitizer])],
  [CFLAGS="$CFLAGS -fsanitize=undefined"
   CXXFLAGS="$CXXFLAGS -fsanitize=undefined"
   LDFLAGS="$LDFLAGS -fsanitize=undefined"])
#FFTW for fourier transform
# AC_ARG_WITH([fftw],
#             [AS_HELP_STRING([--with-fftw=DIR],
#                             [path to FFTW library])],
#             [
#               FFTW_LIB="-L$withval/lib"
#               FFTW_INCLUDE="-I$withval/include"
#               CPPFLAGS="$CPPFLAGS $FFTW_INCLUDE"
#               LDFLAGS="$LDFLAGS $FFTW_LIB"
#             ],
#             [AC_MSG_ERROR([FFTW library is required. Please use --with-fftw to specify its location.])])
#PKG_CHECK_MODULES([FFTW], [fftw3],
#                  [],
#                  [AC_MSG_ERROR([FFTW library not found. Please install it.])])

PKG_CHECK_MODULES([czmq], [libczmq],
                  [],
                  [AC_MSG_ERROR([libczmq library not found. Please install it.])])
# Enable Address Sanitizer
AC_ARG_ENABLE([asan],
  [AS_HELP_STRING([--enable-asan], [Enable Address Sanitizer])],
  [CFLAGS="$CFLAGS -fsanitize=address"
   CXXFLAGS="$CXXFLAGS -fsanitize=address"
   LDFLAGS="$LDFLAGS -fsanitize=address"])
AX_FLUX_CORE
fluxpwr_ldflags=" -avoid-version -export-symbols-regex '^mod_(main|name|service)\$\$' --disable-static -shared -export-dynamic"
AC_SUBST(fluxpwr_ldflags)

AC_CHECK_LIB([pthread], [pthread_create], 
             [], 
             [AC_MSG_ERROR([Could not find required pthread library])])
AX_PTHREAD([],
           [AC_MSG_ERROR([Could not find pthreads support])])
# Checks for header files.
AC_CHECK_HEADERS([stdint.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UINT32_T

PKG_CHECK_MODULES([JANSSON], [jansson >= 2.7],
                  [],
                  [AC_MSG_ERROR([Jansson library not found. Please install it.])])

# Checks for library functions.

# AC_FUNC_MALLOC
AC_CHECK_FUNCS([gethostname strdup ssize_t])

AC_CONFIG_FILES(\
                Makefile \
                src/flux_pwr_monitor/Makefile \
                src/common/Makefile 
              )
AC_OUTPUT
