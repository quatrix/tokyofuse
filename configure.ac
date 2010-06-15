#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.59])
AC_INIT([tokyofuse], [0.1], [evil.legacy@gmail.com])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_CONFIG_SRCDIR([src/metadata.c])
PKG_CHECK_MODULES([FUSE], [fuse >= 2.7.4]) 

AC_ARG_WITH(python, [  --with-python=PATH	  Path to specific Python binary],[ PYTHON="$withval" ])

AC_ARG_ENABLE(tests,[  --enable-tests     build tests],[ 
	PKG_CHECK_MODULES([CHECK], [check >= 0.9.4]) 
	AM_PATH_PYTHON([2.6])
	AC_DEFINE_UNQUOTED([PYTHON_BIN], "${PYTHON}", [python binary defined to $PYTHON])
	build_tests=true
]
)

AM_CONDITIONAL([BUILD_TESTS], [test x$build_tests = xtrue ])


# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h inttypes.h stddef.h stdlib.h string.h sys/time.h unistd.h tchdb.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_HEADER_STDBOOL
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_CHECK_LIB([tokyocabinet], [tchdbnew],[], AC_MSG_ERROR([*** tokyocabinet libraries required]))

AC_CHECK_FUNCS([gettimeofday lchown memset mkdir mkfifo rmdir strchr strdup strndup strrchr])
AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile])
AC_CONFIG_HEADERS([config.h])
AM_PROG_CC_C_O
AC_OUTPUT
