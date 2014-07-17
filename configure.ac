AC_PREREQ(2.60)
AC_INIT([signonui-efl], [0.0.3], [jussi.kukkonen@intel.com])
AM_INIT_AUTOMAKE([1.11 -Wall -Werror dist-bzip2])
AM_SILENT_RULES([yes])

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

PKG_CHECK_MODULES(GLIB,
                  glib-2.0 >= 2.30
                  gio-unix-2.0)

PKG_CHECK_MODULES(EFL,
                  ecore
                  evas
                  elementary)

PKG_CHECK_MODULES(GSIGNOND,
                  gsignond)

AC_ARG_ENABLE(timeout,
              [  --enable-timeout   Enable/disable daemon timeout],
              [enable_timeout=$enableval],
              [enable_timeout=yes])
if test "x$enable_timeout" = "xyes" ; then
    AC_DEFINE(ENABLE_TIMEOUT, [1], [Enable daemon timeout])
fi

AC_ARG_WITH([ewebkit2],
           [  --with-webkit2    Use ewebkit2 for oauth authentication],
           [with_webkit2=$withval],
           [with_webkit2="no"])
if test "x$with_webkit2" = "xyes" ; then
  PKG_CHECK_MODULES(EWEBKIT, ewebkit2)
  AC_DEFINE([USE_WEBKIT], [1], [Use webkit])
else
  PKG_CHECK_MODULES(LIBSOUP, libsoup-2.4)
  
  AC_ARG_ENABLE([browser-cmd],
                [  --enable-browser-cmd   configure which browser to use, default /usr/bin/firefox],
                [browser_cmd=$enableval],
                [browser_cmd='/usr/bin/firefox'])
  AC_DEFINE_UNQUOTED([BROWSER_CMD], ["$browser_cmd"], [Browser to use])
  
  AC_ARG_ENABLE([http-port],
                [  --enable-http-port   fallack http port to use],
                [http_port=$enableval],
                [http_port=9004])
  AC_DEFINE_UNQUOTED([DEFULT_HTTP_PORT], [$http_port], [Fallback http port])
fi  
AM_CONDITIONAL([WITH_WEBKIT], [test "$with_webkit2" = "yes"])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
        Makefile
        src/Makefile
        data/Makefile
        tools/Makefile
        tests/Makefile
])
AC_OUTPUT
