PHP_ARG_WITH([firebird],
  [for Firebird support],
  [AS_HELP_STRING([[--with-firebird[=DIR]]],
    [Include Firebird support. DIR is the Firebird base install directory
    [/opt/firebird]])])

if test "$PHP_FIREBIRD" != "no"; then

  dnl Detect php-firebird version from git or VERSION file
  AC_MSG_CHECKING([for php-firebird version])
  if test -f "$srcdir/VERSION"; then
    PHP_FIREBIRD_VERSION=`cat "$srcdir/VERSION" | tr -d ' \n\r\t'`
  elif test -d "$srcdir/.git" -a -x "`which git 2>/dev/null`"; then
    PHP_FIREBIRD_VERSION=`cd "$srcdir" && git describe --tags --always --dirty 2>/dev/null | sed 's/^v//'`
    if test -z "$PHP_FIREBIRD_VERSION"; then
      PHP_FIREBIRD_VERSION="0.0.0-unknown"
    fi
  else
    PHP_FIREBIRD_VERSION="0.0.0-unknown"
  fi
  AC_MSG_RESULT([$PHP_FIREBIRD_VERSION])
  AC_DEFINE_UNQUOTED([PHP_FIREBIRD_VERSION_STRING], ["$PHP_FIREBIRD_VERSION"], [PHP Firebird extension version])

  dnl Check for minimum PHP version (8.1+)
  AC_MSG_CHECKING([for minimum PHP version 8.1])
  PHP_FIREBIRD_PHP_VERSION=`$PHP_CONFIG --version`
  old_IFS=$IFS
  IFS=.
  set -- $PHP_FIREBIRD_PHP_VERSION
  IFS=$old_IFS
  php_major=$1
  php_minor=$2
  if test "$php_major" -lt 8 -o \( "$php_major" -eq 8 -a "$php_minor" -lt 1 \); then
    AC_MSG_ERROR([PHP Firebird extension requires PHP 8.1 or later. Current version: $PHP_FIREBIRD_PHP_VERSION])
  fi
  AC_MSG_RESULT([yes (PHP $PHP_FIREBIRD_PHP_VERSION)])

  AC_PATH_PROG(FB_CONFIG, fb_config, no)

  if test -x "$FB_CONFIG" && test "$PHP_FIREBIRD" = "yes"; then
    AC_MSG_CHECKING(for libfbconfig)
    FB_CFLAGS=`$FB_CONFIG --cflags`
    FB_LIBDIR=`$FB_CONFIG --libs`
    FB_VERSION=`$FB_CONFIG --version`
    AC_MSG_RESULT(version $FB_VERSION)

    dnl Check for minimum Firebird version (3.0+)
    AC_MSG_CHECKING([for minimum Firebird version 3.0])
    fb_major=`echo $FB_VERSION | cut -d. -f1`
    if test "$fb_major" -lt 3; then
      AC_MSG_ERROR([Firebird 3.0 or later is required. Found version: $FB_VERSION])
    fi
    AC_MSG_RESULT([yes (Firebird $FB_VERSION)])

    PHP_EVAL_LIBLINE($FB_LIBDIR, FIREBIRD_SHARED_LIBADD)
    PHP_EVAL_INCLINE($FB_CFLAGS)

  else
    if test "$PHP_FIREBIRD" = "yes"; then
      FIREBIRD_INCDIR=/opt/firebird/include
      FIREBIRD_LIBDIR=/opt/firebird/lib
    else
      FIREBIRD_INCDIR=$PHP_FIREBIRD/include
      FIREBIRD_LIBDIR=$PHP_FIREBIRD/$PHP_LIBDIR
    fi

    PHP_CHECK_LIBRARY(fbclient, isc_detach_database,
    [
      FIREBIRD_LIBNAME=fbclient
    ], [
      PHP_CHECK_LIBRARY(gds, isc_detach_database,
      [
        FIREBIRD_LIBNAME=gds
      ], [
        PHP_CHECK_LIBRARY(ib_util, isc_detach_database,
        [
          FIREBIRD_LIBNAME=ib_util
        ], [
          AC_MSG_ERROR([libfbclient, libgds or libib_util not found! Check config.log for more information.])
        ], [
          -L$FIREBIRD_LIBDIR
        ])
      ], [
        -L$FIREBIRD_LIBDIR
      ])
    ], [
      -L$FIREBIRD_LIBDIR
    ])

    PHP_ADD_LIBRARY_WITH_PATH($FIREBIRD_LIBNAME, $FIREBIRD_LIBDIR, FIREBIRD_SHARED_LIBADD)
    PHP_ADD_INCLUDE($FIREBIRD_INCDIR)
  fi

  AC_DEFINE(HAVE_FIREBIRD,1,[ ])

  dnl Check for PDO availability (required for pdo_fbird integration)
  ifdef([PHP_CHECK_PDO_INCLUDES],
  [
    PHP_CHECK_PDO_INCLUDES
    AC_DEFINE(HAVE_PDO_FBIRD,1,[Whether pdo_fbird is integrated])
  ],[
    AC_MSG_WARN([PDO not found — pdo_fbird driver will not be available])
  ])

  dnl Enable extra debug logging for array slice operations when requested.
  dnl This is a build-time flag used by `src/cpp/fb_array.hpp`.
  dnl
  dnl Usage:
  dnl   CPPFLAGS="-DFBIRD_ARRAY_DEBUG" ./configure --with-firebird=/usr
  dnl
  PHP_NEW_EXTENSION(firebird, firebird.c fbird_error.c fbird_connection.c fbird_transaction.c fbird_batch.c fbird_query_exec.c fbird_query_prepare.c fbird_query_bind.c fbird_query_array.c fbird_datetime.c fbird_result.c fbird_metadata.c fbird_service.c fbird_events.c fbird_blobs.c fbird_inspection.c fbird_classes.c pdo_fbird/pdo_fbird.c pdo_fbird/pdo_fbird_driver.c pdo_fbird/pdo_fbird_stmt.c pdo_fbird/pdo_fbird_error.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1,[cxx])
  PHP_ADD_EXTENSION_DEP(firebird, pdo)
  PHP_SUBST(FIREBIRD_SHARED_LIBADD)

  PHP_REQUIRE_CXX()
  PHP_CXX_COMPILE_STDCXX([17], [mandatory], [PHP_FIREBIRD_STDCXX])

  PHP_FIREBIRD_CXX_SOURCES="firebird_utils.cpp"

  AS_VAR_IF([ext_shared], [no],
    [PHP_ADD_SOURCES([$ext_dir],
      [$PHP_FIREBIRD_CXX_SOURCES],
      [$PHP_FIREBIRD_STDCXX])],
    [PHP_ADD_SOURCES_X([$ext_dir],
      [$PHP_FIREBIRD_CXX_SOURCES],
      [$PHP_FIREBIRD_STDCXX],
      [shared_objects_firebird],
      [yes])])

fi
