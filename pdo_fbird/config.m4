dnl pdo_fbird - PDO driver for Firebird (fbird: DSN prefix)
dnl Separate shared extension depending on the firebird extension.
dnl Self-contained: shared headers from parent are bundled in this package.

PHP_ARG_WITH([pdo-fbird],
  [for Firebird PDO support (fbird: DSN)],
  [AS_HELP_STRING([--with-pdo-fbird],
    [Include PDO Firebird driver using fbird: DSN prefix])])

PHP_ARG_WITH([firebird],
  [for Firebird client library],
  [AS_HELP_STRING([--with-firebird[=DIR]],
    [Firebird client install prefix (default: autodetect)])],
  [yes])

if test "$PHP_PDO_FBIRD" != "no"; then

  AC_DEFINE(HAVE_PDO_FBIRD,1,[Whether pdo_fbird is available])

  dnl Version: read from VERSION.txt (bundled in this package)
  AC_MSG_CHECKING([for PDO fbird version])
  if test -f "$srcdir/VERSION.txt"; then
    PHP_PDO_FBIRD_VERSION=`cat "$srcdir/VERSION.txt" | tr -d ' \n\r\t'`
  else
    PHP_PDO_FBIRD_VERSION="0.0.0-unknown"
  fi
  AC_MSG_RESULT([$PHP_PDO_FBIRD_VERSION])
  AC_DEFINE_UNQUOTED([PHP_PDO_FBIRD_VERSION], ["$PHP_PDO_FBIRD_VERSION"], [PDO fbird extension version])

  dnl Require PDO headers (PDO may be built-in or shared)
  ifdef([PHP_CHECK_PDO_INCLUDES],
    [PHP_CHECK_PDO_INCLUDES],
    [AC_MSG_CHECKING([for PDO includes])
     if test -f "$abs_srcdir/ext/pdo/php_pdo_driver.h"; then
       pdo_inc_path="$abs_srcdir/ext/pdo"
     elif test -f "$phpincludedir/ext/pdo/php_pdo_driver.h"; then
       pdo_inc_path="$phpincludedir/ext/pdo"
     else
       AC_MSG_ERROR([Cannot find php_pdo_driver.h. Make sure PDO is available.])
     fi
     PHP_ADD_INCLUDE([$pdo_inc_path])
     AC_MSG_RESULT([$pdo_inc_path])])

  dnl Find Firebird client library
  FIREBIRD_INCDIR=""
  FIREBIRD_LIBDIR=""

  dnl Build search path: explicit --with-firebird=DIR first, then defaults
  FB_SEARCH_PATHS=""
  if test "$PHP_FIREBIRD" != "no" && test "$PHP_FIREBIRD" != "yes"; then
    FB_SEARCH_PATHS="$PHP_FIREBIRD"
  fi
  FB_SEARCH_PATHS="$FB_SEARCH_PATHS /usr /usr/local /opt/firebird"

  for i in $FB_SEARCH_PATHS; do
    if test -f "$i/include/firebird/Interface.h"; then
      FIREBIRD_INCDIR="$i/include"
      FIREBIRD_LIBDIR="$i/lib"
      break
    fi
    if test -f "$i/include/ibase.h"; then
      FIREBIRD_INCDIR="$i/include"
      FIREBIRD_LIBDIR="$i/lib"
      break
    fi
  done

  if test -z "$FIREBIRD_INCDIR"; then
    AC_MSG_ERROR([Firebird client headers not found. Install libfbclient-dev or specify --with-firebird=DIR.])
  fi

  PHP_ADD_INCLUDE($FIREBIRD_INCDIR)
  PHP_ADD_LIBRARY_WITH_PATH(fbclient, $FIREBIRD_LIBDIR, PDO_FBIRD_SHARED_LIBADD)

  PHP_NEW_EXTENSION(pdo_fbird,
    pdo_fbird.c \
    pdo_fbird_driver.c \
    pdo_fbird_stmt.c \
    pdo_fbird_error.c,
    $ext_shared,, [-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 -I$FIREBIRD_INCDIR])

  PHP_ADD_EXTENSION_DEP(pdo_fbird, pdo)
  PHP_ADD_EXTENSION_DEP(pdo_fbird, firebird)
  PHP_SUBST(PDO_FBIRD_SHARED_LIBADD)
fi
