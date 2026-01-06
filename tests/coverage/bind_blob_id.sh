#!/bin/sh

export HOSTNAME='3657bd73ad3c'
export PHP_INI_DIR='/usr/local/etc/php'
export HOME='/'
export OLDPWD='/ext'
export XDEBUG_CONFIG='client_host=host.docker.internal'
export FIREBIRD_HOST='firebird40'
export PHP_IDE_CONFIG='serverName=php-firebird-dev'
export PHP_LDFLAGS='-Wl,-O1 -pie'
export PHP_CFLAGS='-fstack-protector-strong -fpic -fpie -O2 -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64'
export PHP_VERSION='8.3.29'
export GPG_KEYS='1198C0117593497A5EC5C199286AF1F9897469DC C28D937575603EB4ABB725861C0779DC5C0A9DE4 AFD8691FDAEDF03BDF6E460563F15A9B715376CA'
export PHP_CPPFLAGS='-fstack-protector-strong -fpic -fpie -O2 -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64'
export PHP_ASC_URL='https://www.php.net/distributions/php-8.3.29.tar.xz.asc'
export PHP_URL='https://www.php.net/distributions/php-8.3.29.tar.xz'
export PATH='/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin'
export FIREBIRD_DB_DIR='/firebird'
export PHPIZE_DEPS='autoconf 		dpkg-dev 		file 		g++ 		gcc 		libc-dev 		make 		pkg-config 		re2c'
export PWD='/ext'
export PHP_SHA256='f7950ca034b15a78f5de9f1b22f4d9bad1dd497114d175cb1672a4ca78077af5'
export TZ=''
export TEMP='/tmp'
export SKIP_IO_CAPTURE_TESTS='1'
export TEST_PHP_EXECUTABLE='/usr/local/bin/php'
export TEST_PHP_EXECUTABLE_ESCAPED=''\''/usr/local/bin/php'\'''
export TEST_PHP_CGI_EXECUTABLE='/usr/local/bin/php-cgi'
export TEST_PHP_CGI_EXECUTABLE_ESCAPED=''\''/usr/local/bin/php-cgi'\'''
export TEST_PHPDBG_EXECUTABLE='/usr/local/bin/phpdbg'
export TEST_PHPDBG_EXECUTABLE_ESCAPED=''\''/usr/local/bin/phpdbg'\'''
export REDIRECT_STATUS='1'
export QUERY_STRING=''
export PATH_TRANSLATED='/ext/tests/coverage/bind_blob_id.php'
export SCRIPT_FILENAME='/ext/tests/coverage/bind_blob_id.php'
export REQUEST_METHOD='GET'
export CONTENT_TYPE=''
export CONTENT_LENGTH=''
export TEST_PHP_EXTRA_ARGS='  -d "output_handler=" -d "open_basedir=" -d "disable_functions=" -d "output_buffering=Off" -d "error_reporting=32767" -d "display_errors=1" -d "display_startup_errors=1" -d "log_errors=0" -d "html_errors=0" -d "track_errors=0" -d "report_memleaks=1" -d "report_zend_debug=0" -d "docref_root=" -d "docref_ext=.html" -d "error_prepend_string=" -d "error_append_string=" -d "auto_prepend_file=" -d "auto_append_file=" -d "ignore_repeated_errors=0" -d "precision=14" -d "serialize_precision=-1" -d "memory_limit=128M" -d "opcache.fast_shutdown=0" -d "opcache.file_update_protection=0" -d "opcache.revalidate_freq=0" -d "opcache.jit_hot_loop=1" -d "opcache.jit_hot_func=1" -d "opcache.jit_hot_return=1" -d "opcache.jit_hot_side_exit=1" -d "zend.assertions=1" -d "zend.exception_ignore_args=0" -d "zend.exception_string_param_max_len=15" -d "short_open_tag=0" -d "session.auto_start=0" -d "zlib.output_compression=Off"'
export HTTP_COOKIE=''

case "$1" in
"gdb")
    gdb --args '/usr/local/bin/php'    -d "output_handler=" -d "open_basedir=" -d "disable_functions=" -d "output_buffering=Off" -d "error_reporting=32767" -d "display_errors=1" -d "display_startup_errors=1" -d "log_errors=0" -d "html_errors=0" -d "track_errors=0" -d "report_memleaks=1" -d "report_zend_debug=0" -d "docref_root=" -d "docref_ext=.html" -d "error_prepend_string=" -d "error_append_string=" -d "auto_prepend_file=" -d "auto_append_file=" -d "ignore_repeated_errors=0" -d "precision=14" -d "serialize_precision=-1" -d "memory_limit=128M" -d "opcache.fast_shutdown=0" -d "opcache.file_update_protection=0" -d "opcache.revalidate_freq=0" -d "opcache.jit_hot_loop=1" -d "opcache.jit_hot_func=1" -d "opcache.jit_hot_return=1" -d "opcache.jit_hot_side_exit=1" -d "zend.assertions=1" -d "zend.exception_ignore_args=0" -d "zend.exception_string_param_max_len=15" -d "short_open_tag=0" -d "session.auto_start=0" -d "zlib.output_compression=Off" -f "/ext/tests/coverage/bind_blob_id.php"  2>&1
    ;;
"lldb")
    lldb -- '/usr/local/bin/php'    -d "output_handler=" -d "open_basedir=" -d "disable_functions=" -d "output_buffering=Off" -d "error_reporting=32767" -d "display_errors=1" -d "display_startup_errors=1" -d "log_errors=0" -d "html_errors=0" -d "track_errors=0" -d "report_memleaks=1" -d "report_zend_debug=0" -d "docref_root=" -d "docref_ext=.html" -d "error_prepend_string=" -d "error_append_string=" -d "auto_prepend_file=" -d "auto_append_file=" -d "ignore_repeated_errors=0" -d "precision=14" -d "serialize_precision=-1" -d "memory_limit=128M" -d "opcache.fast_shutdown=0" -d "opcache.file_update_protection=0" -d "opcache.revalidate_freq=0" -d "opcache.jit_hot_loop=1" -d "opcache.jit_hot_func=1" -d "opcache.jit_hot_return=1" -d "opcache.jit_hot_side_exit=1" -d "zend.assertions=1" -d "zend.exception_ignore_args=0" -d "zend.exception_string_param_max_len=15" -d "short_open_tag=0" -d "session.auto_start=0" -d "zlib.output_compression=Off" -f "/ext/tests/coverage/bind_blob_id.php"  2>&1
    ;;
"valgrind")
    USE_ZEND_ALLOC=0 valgrind $2 '/usr/local/bin/php'    -d "output_handler=" -d "open_basedir=" -d "disable_functions=" -d "output_buffering=Off" -d "error_reporting=32767" -d "display_errors=1" -d "display_startup_errors=1" -d "log_errors=0" -d "html_errors=0" -d "track_errors=0" -d "report_memleaks=1" -d "report_zend_debug=0" -d "docref_root=" -d "docref_ext=.html" -d "error_prepend_string=" -d "error_append_string=" -d "auto_prepend_file=" -d "auto_append_file=" -d "ignore_repeated_errors=0" -d "precision=14" -d "serialize_precision=-1" -d "memory_limit=128M" -d "opcache.fast_shutdown=0" -d "opcache.file_update_protection=0" -d "opcache.revalidate_freq=0" -d "opcache.jit_hot_loop=1" -d "opcache.jit_hot_func=1" -d "opcache.jit_hot_return=1" -d "opcache.jit_hot_side_exit=1" -d "zend.assertions=1" -d "zend.exception_ignore_args=0" -d "zend.exception_string_param_max_len=15" -d "short_open_tag=0" -d "session.auto_start=0" -d "zlib.output_compression=Off" -f "/ext/tests/coverage/bind_blob_id.php"  2>&1
    ;;
"rr")
    rr record $2 '/usr/local/bin/php'    -d "output_handler=" -d "open_basedir=" -d "disable_functions=" -d "output_buffering=Off" -d "error_reporting=32767" -d "display_errors=1" -d "display_startup_errors=1" -d "log_errors=0" -d "html_errors=0" -d "track_errors=0" -d "report_memleaks=1" -d "report_zend_debug=0" -d "docref_root=" -d "docref_ext=.html" -d "error_prepend_string=" -d "error_append_string=" -d "auto_prepend_file=" -d "auto_append_file=" -d "ignore_repeated_errors=0" -d "precision=14" -d "serialize_precision=-1" -d "memory_limit=128M" -d "opcache.fast_shutdown=0" -d "opcache.file_update_protection=0" -d "opcache.revalidate_freq=0" -d "opcache.jit_hot_loop=1" -d "opcache.jit_hot_func=1" -d "opcache.jit_hot_return=1" -d "opcache.jit_hot_side_exit=1" -d "zend.assertions=1" -d "zend.exception_ignore_args=0" -d "zend.exception_string_param_max_len=15" -d "short_open_tag=0" -d "session.auto_start=0" -d "zlib.output_compression=Off" -f "/ext/tests/coverage/bind_blob_id.php"  2>&1
    ;;
*)
    '/usr/local/bin/php'    -d "output_handler=" -d "open_basedir=" -d "disable_functions=" -d "output_buffering=Off" -d "error_reporting=32767" -d "display_errors=1" -d "display_startup_errors=1" -d "log_errors=0" -d "html_errors=0" -d "track_errors=0" -d "report_memleaks=1" -d "report_zend_debug=0" -d "docref_root=" -d "docref_ext=.html" -d "error_prepend_string=" -d "error_append_string=" -d "auto_prepend_file=" -d "auto_append_file=" -d "ignore_repeated_errors=0" -d "precision=14" -d "serialize_precision=-1" -d "memory_limit=128M" -d "opcache.fast_shutdown=0" -d "opcache.file_update_protection=0" -d "opcache.revalidate_freq=0" -d "opcache.jit_hot_loop=1" -d "opcache.jit_hot_func=1" -d "opcache.jit_hot_return=1" -d "opcache.jit_hot_side_exit=1" -d "zend.assertions=1" -d "zend.exception_ignore_args=0" -d "zend.exception_string_param_max_len=15" -d "short_open_tag=0" -d "session.auto_start=0" -d "zlib.output_compression=Off" -f "/ext/tests/coverage/bind_blob_id.php"  2>&1
    ;;
esac