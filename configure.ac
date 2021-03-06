dnl ---------------------------------------------------------------------------
dnl Autoconf initialization.
dnl
dnl Bug reports are sent to {raim,xtof}@tbi.univie.ac.at
dnl ---------------------------------------------------------------------------

if test -f "VERSION.txt"; then
   m4_define(ODES_VERSION,
       m4_esyscmd(cat VERSION.txt  | sed 's/\-.*$//' | tr -d '\n\r'))
fi
AC_INIT([odeSolver],ODES_VERSION, [{raim,xtof}@tbi.univie.ac.at], [SBML_odeSolver])
AC_CONFIG_AUX_DIR(build-aux)
AC_CONFIG_MACRO_DIR(config)

AC_CONFIG_SRCDIR(src/util.c)
AC_CONFIG_HEADERS(src/config.h)

AM_INIT_AUTOMAKE

AC_CANONICAL_HOST

case $host_os in
  *linux*)
    HOST_TYPE="linux"
    SHAREDLIBEXT="so"
    ;;

  *solaris*)
    HOST_TYPE="solaris"
    SHAREDLIBEXT="so"
    ;;

  *cygwin*)
    HOST_TYPE="cygwin"
    SHAREDLIBEXT="so"
    ;;

  *mingw*)
    HOST_TYPE="mingw"
    SHAREDLIBEXT="dll"
    ;;

  *aix*)
    HOST_TYPE="aix"
    SHAREDLIBEXT="a"
    ;;

  *darwin*)
    HOST_TYPE="darwin"
    SHAREDLIBEXT="dylib"
    ;;
esac
AC_SUBST(HOST_TYPE)
AC_SUBST(SHAREDLIBEXT)
AC_DEFINE_UNQUOTED([SHAREDLIBEXT], ".${SHAREDLIBEXT}",
    			    [shared library extrension])

dnl Adding global compilation parameters
AC_SEARCH_LIBS([dlopen],[dl])

dnl ---------------------------------------------------------------------------
dnl Checks for various programs and packages.
dnl ---------------------------------------------------------------------------

LT_INIT([win32-dll disable-static pic-only])
AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O # deprecated as of automake 1.14
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_PATH_PROG([AUTOCONF],[autoconf],[autoconf])
AC_PATH_PROG([ACLOCAL],[aclocal],[aclocal])
AC_PATH_PROG([MAKE],[make],[make])
AC_PATH_PROG([AR],[ar],[ar])
AC_PATH_PROG([SWIG],[swig],[swig])
AC_PATH_PROG([PERL],[perl],[perl])

dnl AC_MSG_NOTICE([LIBS=$LIBS])

CONFIG_LIB_SBML
CONFIG_LIB_SUNDIALS
dnl CONFIG_LIB_TCC
CONFIG_LIB_GRACE
CONFIG_LIB_GRAPHVIZ

dnl add the CFLAGS and LDFLAGS for tcc online compilation
if test "$prefix" = "NONE"; then
   ourPrefix=$ac_default_prefix;
else
   ourPrefix=$prefix;
fi
AC_DEFINE_UNQUOTED([SOSLIB_CPPFLAGS], "${ourPrefix}/include",
				    [SOSLIB include directories])
AC_DEFINE_UNQUOTED([SOSLIB_LDFLAGS], "${ourPrefix}/lib",
				     [SOSLIB lib directories])
AC_DEFINE_UNQUOTED([SOSLIB_LIBS], "ODES",
				  [SOSLIB libs])

dnl configure options
dnl AC_ARG_WITH(bindings, [  --with-bindings  build perl bindings])
dnl AM_CONDITIONAL(MAKE_BINDINGS, test "$with_bindings" = "yes")

AC_ARG_ENABLE(unittest,
              [AS_HELP_STRING([--enable-unittest],
                              [turn on unit tests. [default=no]])],
              [PKG_CHECK_MODULES([CHECK],[check >= 0.9.8],[enable_unittest=yes],
                                 [AC_MSG_ERROR([failed to find Check])])],
              [enable_unittest=no])
AM_CONDITIONAL([UNITTEST], [test "x$enable_unittest" = "xyes"])

dnl ---------------------------------------------------------------------------
dnl Checks for libraries.
dnl ---------------------------------------------------------------------------

AC_LANG([C])
AC_CHECK_LIB(m, isnan)

dnl ---------------------------------------------------------------------------
dnl Checks for header files.
dnl ---------------------------------------------------------------------------

AC_HEADER_STDC
AC_CHECK_HEADERS(errno.h)
AC_CHECK_HEADERS(math.h)

dnl ---------------------------------------------------------------------------
dnl Checks for typedefs, structures, and compiler characteristics.
dnl ---------------------------------------------------------------------------

AC_C_BIGENDIAN
AC_C_CONST
AC_TYPE_SIZE_T
AC_EXEEXT
AC_OBJEXT

dnl ---------------------------------------------------------------------------
dnl Miscellaneous
dnl ---------------------------------------------------------------------------

dnl ---------------------------------------------------------------------------
dnl Output
dnl ---------------------------------------------------------------------------

AC_CONFIG_FILES([Makefile])
dnl AC_CONFIG_FILES([doc/Makefile])
dnl AC_CONFIG_FILES([doc/SBML_odeSolver.1])
AC_CONFIG_FILES([examples/Makefile])
AC_CONFIG_FILES([odeSolver/Makefile])
AC_CONFIG_FILES([src/Makefile])
AC_CONFIG_FILES([src/libODES.pc])
AC_CONFIG_FILES([tutorial/Makefile])
AC_CONFIG_FILES([unittest/Makefile])
dnl AC_CONFIG_FILES([bindings/Makefile])
dnl AC_CONFIG_FILES([bindings/swig/Makefile])
dnl AC_CONFIG_FILES([bindings/perl/Makefile])
dnl AC_CONFIG_FILES([bindings/perl/Makefile.PL])

AC_OUTPUT

dnl ---------------------------------------------------------------------------
dnl Inform users of settings
dnl ---------------------------------------------------------------------------

if test "$sbml_functional" != no; then
  yes_libsbml="yes"
else
  yes_libsbml="no"
fi

if test "$sundials_functional" != no; then
  yes_libsundials="yes"
else
  yes_libsundials="no"
fi

dnl if test "$tcc_functional" != no; then
dnl  yes_tcc="yes"
dnl else
dnl  yes_tcc="no"
dnl fi

if test "$grace_functional" != no; then
  yes_grace="yes"
else
  yes_grace="no"
fi


if test "$graphviz_functional" != no; then
  yes_graphviz="yes"
else
  yes_graphviz="no"
fi


echo ""
echo "Build Settings for ${PACKAGE_TARNAME} v${PACKAGE_VERSION}:"
echo "----------------------------------"
echo "  host type            = $HOST_TYPE"
echo "  shared lib extension = $SHAREDLIBEXT"
echo "  install dir          = $prefix"
echo "  CC                   = $CC"
echo "  CFLAGS               = $CFLAGS"
echo "  LDFLAGS              = $LDFLAGS"
echo "  LIBS                 = $LIBS"
echo "  SBML Library         = $yes_libsbml"
if test "$yes_libsbml" = yes; then
  echo "       CPPFLAGS        = $SBML_CPPFLAGS"
  echo "       LDFLAGS         = $SBML_LDFLAGS"
  echo "       LIBS            = $SBML_LIBS"
fi
echo "  SUNDIALS Library       = $yes_libsundials"
if test "$yes_libsundials" = yes; then
  echo "       CPPFLAGS        = $SUNDIALS_CPPFLAGS"
  echo "       LDFLAGS         = $SUNDIALS_LDFLAGS"
  echo "       LIBS            = $SUNDIALS_LIBS"
fi
dnl echo "  TCC Library       = $yes_tcc"
dnl if test "$yes_tcc" = yes; then
dnl   echo "       CPPFLAGS        = $TCC_CPPFLAGS"
dnl   echo "       LDFLAGS         = $TCC_LDFLAGS"
dnl   echo "       LIBS            = $TCC_LIBS"
dnl else
dnl    echo "TCC online compilation will NOT be installed!"
dnl fi
echo "  GRACE                = $yes_grace"
if test "$yes_grace" = yes; then
  echo "       CPPFLAGS        = $GRACE_CPPFLAGS"
  echo "       LDFLAGS         = $GRACE_LDFLAGS"
  echo "       LIBS            = $GRACE_LIBS"
else
   echo "XMGRACE functionality will NOT be installed!"
fi
echo "  GRAPHVIZ             = $yes_graphviz"
if test "$yes_graphviz" = yes; then
  echo "       CPPFLAGS        = $GRAPHVIZ_CPPFLAGS"
  echo "       LDFLAGS         = $GRAPHVIZ_LDFLAGS"
  echo "       LIBS            = $GRAPHVIZ_LIBS"
else
   echo "GRAPHVIZ functionality will NOT be installed!"
fi

echo ""
echo "NOTE:"
echo "     SOSlib has the following dependencies:"
echo "     (*) SUNDIALS suite v2.4.0"
echo "     (*) libSBML v3.4.1 or later"
dnl echo "     (*) libxerces-c v2.7.0 or expat v2.0.0"
dnl echo "     (*) tcc from http://savannah.nongnu.org/projects/tinycc"
echo ""
