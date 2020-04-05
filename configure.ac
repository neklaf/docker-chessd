#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(chessd, svn, chessd-devel@lists.sourceforge.net)
AC_CONFIG_AUX_DIR(auxdir)
AC_CONFIG_MACRO_DIR(m4)
#AC_GNU_SOURCE # needed for asprintf and others
AM_INIT_AUTOMAKE(foreign)
AC_CONFIG_SRCDIR(src/chessd_main.c)
AM_CONFIG_HEADER(config.h)

AC_CONFIG_FILES([Makefile
         intl/Makefile
         po/Makefile.in])

# Checks for programs.
#AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
#AC_PROG_RANLIB
AC_PROG_YACC

# Libtool
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL

# Checks for libraries.
AC_CHECK_LIB([c], [exit])
AC_CHECK_LIB([m], [sqrt])

AC_CHECK_LIB(dbi, dbi_initialize, [],
             [AC_MSG_ERROR([Missing required library libdbi])])

AC_CHECK_LIB(ltdl, lt_dlopen, [],
             [AC_MSG_ERROR([Missing required library libltdl])])

PKG_CHECK_MODULES(LIBXML, libxml-2.0)

# Checks for header files.
AC_FUNC_ALLOCA
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([argz.h arpa/inet.h fcntl.h float.h inttypes.h langinfo.h \
				  libintl.h limits.h locale.h malloc.h memory.h netdb.h \
				  netinet/in.h stddef.h stdint.h stdio_ext.h stdlib.h string.h \
				  strings.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h \
				  sys/filio.h unistd.h wchar.h])
AC_CHECK_HEADER([dbi/dbi.h], [],
                [AC_MSG_ERROR([Missing dbi/dbi.h, is libdbi installed?])])
AC_CHECK_HEADER([ltdl.h], [], 
				[AC_MSG_ERROR([Missing ltdl.h, is libltdl3-dev installed?])])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_TYPE_UID_T
AC_C_VOLATILE
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_CLOSEDIR_VOID
AM_GNU_GETTEXT
AM_GNU_GETTEXT_VERSION(0.14.1)
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_STRNLEN
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([__argz_count __argz_next __argz_stringify bzero dup2 floor \
				getcwd gettimeofday inet_ntoa memmove mempcpy memset munmap \
				nl_langinfo pow select setenv setlocale socket sqrt stpcpy \
				strcasecmp strchr strcspn strdup strerror strncasecmp strndup \
				strrchr strstr strtoul asprintf vasprintf])

dnl Not needed on modern setups, since turning GNU_SOURCE on defines it
dnl AC_DEFINE(_XOPEN_SOURCE, 500, [Needed for pread()])

AC_OUTPUT

