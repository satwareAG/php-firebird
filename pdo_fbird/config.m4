dnl pdo_fbird — PDO driver for Firebird (fbird: DSN prefix)
dnl Separate shared extension depending on the firebird extension.

PHP_ARG_WITH([pdo-fbird],
  [for Firebird PDO support (fbird: DSN)],
  [AS_HELP_STRING([--with-pdo-fbird],
    [Include PDO Firebird driver using fbird: DSN prefix])])

if test "$PHP_PDO_FBIRD" != "no"; then

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

  for i in /usr /usr/local /opt/firebird; do
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
    AC_MSG_ERROR([Firebird client headers not found. Install libfbclient-dev.])
  fi

  PHP_ADD_INCLUDE($FIREBIRD_INCDIR)
  PHP_ADD_LIBRARY_WITH_PATH(fbclient, $FIREBIRD_LIBDIR, PDO_FBIRD_SHARED_LIBADD)

  dnl Also include the parent extension headers
  PHP_ADD_INCLUDE([$srcdir/..])

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
