AC_PREREQ(2.60)
AC_INIT([signonui-efl], [0.0.1], [jussi.kukkonen@intel.com])
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
              [--enable-timeout Enable/disable daemon timeout],
              [enable_timeout=$enableval],
              [enable_timeout=yes])
if test "x$enable_timeout" = "xyes" ; then
    AC_DEFINE(ENABLE_TIMEOUT, [1], [Enable daemon timeout])
fi

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
        Makefile
        src/Makefile
        data/Makefile
        tools/Makefile
        tests/Makefile
])
AC_OUTPUT
