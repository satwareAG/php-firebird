PHP_ARG_WITH([firebird],
  [for Firebird support],
  [AS_HELP_STRING([[--with-firebird[=DIR]]],
    [Include Firebird support. DIR is the Firebird base install directory
    [/opt/firebird]])])

if test "$PHP_FIREBIRD" != "no"; then

  dnl Detect php-firebird version from git or VERSION file
  AC_MSG_CHECKING([for php-firebird version])
  if test -f "$srcdir/VERSION.txt"; then
    PHP_FIREBIRD_VERSION=`cat "$srcdir/VERSION.txt" | tr -d ' \n\r\t'`
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

  dnl Check for minimum PHP version (8.2+)
  AC_MSG_CHECKING([for minimum PHP version 8.2])
  PHP_FIREBIRD_PHP_VERSION=`$PHP_CONFIG --version`
  old_IFS=$IFS
  IFS=.
  set -- $PHP_FIREBIRD_PHP_VERSION
  IFS=$old_IFS
  php_major=$1
  php_minor=$2
  if test "$php_major" -lt 8 -o \( "$php_major" -eq 8 -a "$php_minor" -lt 2 \); then
    AC_MSG_ERROR([PHP Firebird extension requires PHP 8.2 or later. Current version: $PHP_FIREBIRD_PHP_VERSION])
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

  dnl Base source files (always compiled)
  FIREBIRD_SOURCES="firebird.c fbird_error.c fbird_connection.c fbird_transaction.c fbird_batch.c fbird_query_exec.c fbird_query_prepare.c fbird_query_bind.c fbird_query_array.c fbird_datetime.c fbird_result.c fbird_metadata.c fbird_service.c fbird_events.c fbird_blobs.c fbird_inspection.c fbird_classes.c fbird_class_connection.c fbird_class_transaction.c fbird_class_statement.c fbird_class_resultset.c fbird_class_blob.c fbird_class_service.c fbird_class_event.c fbird_class_batch.c"

  dnl Enable extra debug logging for array slice operations when requested.
  dnl This is a build-time flag used by `src/cpp/fb_array.hpp`.
  dnl
  dnl Usage:
  dnl   CPPFLAGS="-DFBIRD_ARRAY_DEBUG" ./configure --with-firebird=/usr
  dnl
  PHP_NEW_EXTENSION(firebird, $FIREBIRD_SOURCES, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1,[cxx])
  PHP_SUBST(FIREBIRD_SHARED_LIBADD)

  dnl Compiler hardening flags (Issue #164)
  FIREBIRD_CFLAGS=""
  AX_CHECK_COMPILE_FLAG(-Wall, [FIREBIRD_CFLAGS="$FIREBIRD_CFLAGS -Wall"])
  AX_CHECK_COMPILE_FLAG(-Wextra, [FIREBIRD_CFLAGS="$FIREBIRD_CFLAGS -Wextra"])
  AX_CHECK_COMPILE_FLAG(-Wformat-security, [FIREBIRD_CFLAGS="$FIREBIRD_CFLAGS -Wformat-security"])
  AX_CHECK_COMPILE_FLAG(-Wno-unused-parameter, [FIREBIRD_CFLAGS="$FIREBIRD_CFLAGS -Wno-unused-parameter"])
  AX_CHECK_COMPILE_FLAG(-fstack-protector-strong, [FIREBIRD_CFLAGS="$FIREBIRD_CFLAGS -fstack-protector-strong"])

  dnl Pin C standard to gnu17 (Issue #165)
  AX_CHECK_COMPILE_FLAG(-std=gnu17, [FIREBIRD_CFLAGS="$FIREBIRD_CFLAGS -std=gnu17"])

  dnl Link-Time Optimization (LTO) — enabled by default, opt out with --disable-fbird-lto
  AC_ARG_ENABLE([fbird-lto],
    [AS_HELP_STRING([--disable-fbird-lto],
      [Disable LTO for the firebird extension [default=enabled]])],
    [PHP_FBIRD_LTO=$enableval],
    [PHP_FBIRD_LTO=yes])

  if test "$PHP_FBIRD_LTO" != "no"; then
    dnl Auto-disable LTO on PHP < 8.3 (libtool 1.5.26 strips -flto flags,
    dnl causing link failures and test regressions. Fixed upstream in PHP
    dnl master via PR #21067 (libtool 2.5.4), not backported to 8.2.)
    if test "$php_major" -eq 8 -a "$php_minor" -lt 3; then
      AC_MSG_NOTICE([PHP 8.2 detected - disabling LTO (libtool incompatibility)])
      PHP_FBIRD_LTO=no
    fi
  fi

  if test "$PHP_FBIRD_LTO" != "no"; then
    dnl Auto-disable LTO when sanitizers are active (incompatible — linker failures)
    case "$CFLAGS" in
      *-fsanitize=*)
        AC_MSG_NOTICE([Sanitizer detected in CFLAGS - disabling LTO (incompatible)])
        PHP_FBIRD_LTO=no
        ;;
    esac
  fi

  if test "$PHP_FBIRD_LTO" != "no"; then
    dnl GCC supports -flto=auto (parallel LTO), Clang uses -flto (no =auto suffix)
    AX_CHECK_COMPILE_FLAG([-flto=auto], [
      FIREBIRD_CFLAGS="$FIREBIRD_CFLAGS -flto=auto"
      LDFLAGS="$LDFLAGS -flto=auto"
    ], [
      AX_CHECK_COMPILE_FLAG([-flto], [
        FIREBIRD_CFLAGS="$FIREBIRD_CFLAGS -flto"
        LDFLAGS="$LDFLAGS -flto"
      ])
    ])
  fi

  dnl -D_FORTIFY_SOURCE=2 requires -O1 minimum (PHP defaults to -O2)
  CFLAGS="$CFLAGS $FIREBIRD_CFLAGS -D_FORTIFY_SOURCE=2"

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
