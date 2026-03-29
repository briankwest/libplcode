AC_INIT([libplcode], [1.0.1], [brian@mcalester.net], [libplcode],
        [https://github.com/briankwest/libplcode])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall -Werror])

# silent rules if supported (make V=1 overrides)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# ar is needed for static convenience archives
AM_PROG_AR

LT_INIT([pic-only])

AC_PROG_CC

AC_CHECK_LIB([m], [sin])

AC_CONFIG_FILES([Makefile plcode.pc])
AC_OUTPUT
