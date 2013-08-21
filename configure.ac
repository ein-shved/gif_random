AC_INIT([Gif Random], [0.1.6], [Shvedov Yury <shved@lvk.cs.msu.su>])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC

PKG_CHECK_MODULES([GLIB], glib >= 1.2.0)
PKG_CHECK_MODULES([GTK], gtk+-2.0)

AC_CHECK_HEADERS(gif_lib.h)
AC_CHECK_LIB(gif, DGifOpenFileName)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
    Makefile
    src/Makefile
])
AC_OUTPUT
