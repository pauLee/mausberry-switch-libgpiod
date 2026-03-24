AC_PREREQ(2.69)

AC_INIT([Mausberry Switch], [0.9], [https://github.com/pauLee/mausberry-switch-libgpiod/issues], [mausberry-switch], [https://github.com/pauLee/mausberry-switch-libgpiod])
AC_CONFIG_SRCDIR(src)
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Programs
AC_PROG_CC
AC_PROG_INSTALL

# Libraries
PKG_CHECK_MODULES(LIBGPIOD, libgpiod >= 1.0)

# Headers
AC_CHECK_HEADERS([signal.h stdlib.h stdio.h string.h unistd.h])

AC_CONFIG_FILES([
Makefile
src/Makefile
tests/Makefile
])
AC_OUTPUT
