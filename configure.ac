#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([simput], [m4_esyscmd([build-aux/git-version-gen .tarball-version])], [sixte-support@lists.fau.de])

# Store auxiliary files in the sub-directory 'build-aux'.
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR(build-aux/m4) 

AM_INIT_AUTOMAKE([foreign -Wall])
AC_CONFIG_SRCDIR([libsimput/fileaccess.c])

AC_REQUIRE_AUX_FILE([git-version-gen])


# Check for programs:
AC_PROG_CC # Search for a C compiler and store it in the Variable 'CC'.
AC_PROG_FC # Search for a Fortran compiler.
AM_PROG_AR

LT_PREREQ(2.4)

AC_PROG_LIBTOOL

# Compiler and Linker Flags:
# "-g -W -Wall -O0" for the compiler for debugging
# "-g" to create debugging information (seems to be used automatically by autotools)
# "-Werror" for the compiler to treat warnings as errors
# "-pg" for the compiler and linker to include information for gprof (Profiler)
# "-std=c99 -D_GNU_SOURCE" for C99 syntax standards
# "-O0" for zero optimization
CFLAGS="$CFLAGS -g -std=c99 -D_GNU_SOURCE -W -Wall -fPIC -Dg77Fortran "
# FCFLAGS="-O0"

# Conditional compilation of Fortran code.
AC_ARG_ENABLE([fortran],
              [AS_HELP_STRING([--enable-fortran],
	                      [enable compilation of Fortran code])],
	          [fortran=true], [fortran=false])
AM_CONDITIONAL([FORTRAN], [test x$fortran = xtrue])

# Conditional compilation of program 'galabs' (requires HEAdas).
AC_ARG_ENABLE([galabs],
              [AS_HELP_STRING([--enable-galabs],
	                      [enable compilation of binary 'galabs'])],
	          [galabs=true], [galabs=false])
AM_CONDITIONAL([GALABS], [test x$galabs = xtrue])

# make ar better behaved
AC_SUBST([ARFLAGS],[rvU])

# Check for libraries.
AC_SEARCH_LIBS(sin, m, [], [AC_MSG_ERROR([library math not found!])], [])
# ncurses and readline (required by ape library).
AC_SEARCH_LIBS(beep, ncurses, [], [AC_MSG_ERROR([library ncurses not found!])], [])
AC_SEARCH_LIBS(readline, readline, [], [AC_MSG_ERROR([library readline not found!])], [])

dir="`pwd`"
LDFLAGS="$LDFLAGS -g -W -Wall -L$dir/extlib/cfitsio -L$dir/extlib/wcslib/C -L$dir/extlib/fftw "
LIBS="$LIBS -lcfitsio -lfftw3 -lwcs"

case "$build_os" in
  darwin*) IS_OSX=true;;
  *)	   IS_OSX=false;;
esac
AM_CONDITIONAL([IS_OSX], [test x$IS_OSX = xtrue])

OSX_VERSION_MAJOR=""
OSX_VERSION_MINOR=""
case $build_os in
  darwin*)
  OSX_VERSION_MAJOR=`sw_vers -productVersion | awk -F '.' '{print $1}'`
  OSX_VERSION_MINOR=`sw_vers -productVersion | awk -F '.' '{print $2}'`
  CFLAGS="$CFLAGS -headerpad_max_install_names"
  LDFLAGS="$LDFLAGS -headerpad_max_install_names"
  ;;
esac
AC_SUBST(OSX_VERSION_MAJOR)
AC_SUBST(OSX_VERSION_MINOR)

# Check for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([limit.h math.h stdio.h stdlib.h stdarg.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# The following line tells 'configure' to create a header file 'config.h'
# that is filled with definitions from this script.
# The 'config.h' contains for example the name of the package and can be
# included in the programm sources.
AC_CONFIG_HEADER([simputconfig.h])

# Create the following Makefiles (including subdirectories).
# A Makefile that is not listed here will not be created by 'automake'!
AC_CONFIG_FILES([Makefile
                 extlib/Makefile
		 extlib/ape/Makefile
		 extlib/ape/include/Makefile
		 extlib/ape/include/ape/Makefile
		 extlib/ape/src/Makefile
		 extlib/atFunctions/Makefile
		 extlib/atFunctions/include/Makefile
		 extlib/atFunctions/src/Makefile
		 extlib/heaio/Makefile
		 extlib/heainit/Makefile
		 extlib/heautils/Makefile
		 extlib/heasp/Makefile
		 libsimput/Makefile
		 sample/Makefile
		 sample/csample/Makefile
		 sample/fsample/Makefile
		 tools/Makefile
		 tools/labnh/Makefile
		 tools/galabs/Makefile
		 tools/simputfile/Makefile
		 tools/simputlc/Makefile
		 tools/simputpsd/Makefile
		 tools/simputimg/Makefile
		 tools/simputmerge/Makefile
		 tools/simputspec/Makefile
		 tools/simputmultispec/Makefile
		 tools/simputrotate/Makefile
		 tools/simputsrc/Makefile
		 tools/simputmulticell/Makefile
		 tools/simputverify/Makefile
		 tools/simputversion/Makefile])


AC_CONFIG_SUBDIRS([extlib/cfitsio])
AC_CONFIG_SUBDIRS([extlib/wcslib])
AC_CONFIG_SUBDIRS([extlib/fftw])

AC_OUTPUT
