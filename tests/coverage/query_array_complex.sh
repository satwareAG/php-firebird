#!/bin/sh

export TEST_PHP_ARGS='-n'
export TEST_PHP_EXECUTABLE='/usr/local/bin/php'
export FIREBIRD_DB_DIR='/firebird'
export HOSTNAME='b2a4368b8fad'
export PHP_VERSION='8.3.14'
export FIREBIRD_HOST='firebird40'
export PWD='/ext'
export TZ=''
export LDFLAGS='-fsanitize=address'
export HOME='/root'
export PHP_IDE_CONFIG='serverName=php-firebird-dev'
export ASAN_OPTIONS='exitcode=139:abort_on_error=0:detect_leaks=1:halt_on_error=0'
export SHLVL='2'
export USE_ZEND_ALLOC='0'
export ZEND_DONT_UNLOAD_MODULES='1'
export PATH='/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin'
export CFLAGS='-fsanitize=address -fno-omit-frame-pointer -g -O1 -DZEND_TRACK_ARENA_ALLOC'
export OLDPWD='/ext'
export _='/usr/local/bin/php'
export TEMP='/tmp'
export TEST_PHP_EXECUTABLE_ESCAPED=''\''/usr/local/bin/php'\'''
export TEST_PHP_CGI_EXECUTABLE='/usr/local/bin/php-cgi'
export TEST_PHP_CGI_EXECUTABLE_ESCAPED=''\''/usr/local/bin/php-cgi'\'''
export TEST_PHPDBG_EXECUTABLE='/usr/local/bin/phpdbg'
export TEST_PHPDBG_EXECUTABLE_ESCAPED=''\''/usr/local/bin/phpdbg'\'''
export REDIRECT_STATUS='1'
export QUERY_STRING=''
export PATH_TRANSLATED='/ext/tests/coverage/query_array_complex.php'
export SCRIPT_FILENAME='/ext/tests/coverage/query_array_complex.php'
export REQUEST_METHOD='GET'
export CONTENT_TYPE=''
export CONTENT_LENGTH=''
export TEST_PHP_EXTRA_ARGS=' -n  -d "output_handler=" -d "open_basedir=" -d "disable_functions=" -d "output_buffering=Off" -d "error_reporting=32767" -d "display_errors=1" -d "display_startup_errors=1" -d "log_errors=0" -d "html_errors=0" -d "track_errors=0" -d "report_memleaks=1" -d "report_zend_debug=0" -d "docref_root=" -d "docref_ext=.html" -d "error_prepend_string=" -d "error_append_string=" -d "auto_prepend_file=" -d "auto_append_file=" -d "ignore_repeated_errors=0" -d "precision=14" -d "serialize_precision=-1" -d "memory_limit=128M" -d "opcache.fast_shutdown=0" -d "opcache.file_update_protection=0" -d "opcache.revalidate_freq=0" -d "opcache.jit_hot_loop=1" -d "opcache.jit_hot_func=1" -d "opcache.jit_hot_return=1" -d "opcache.jit_hot_side_exit=1" -d "zend.assertions=1" -d "zend.exception_ignore_args=0" -d "zend.exception_string_param_max_len=15" -d "short_open_tag=0" -d "extension=/ext/modules/firebird.so" -d "extension=pcntl" -d "session.auto_start=0" -d "zlib.output_compression=Off"'
export HTTP_COOKIE=''

case "$1" in
"gdb")
    gdb --args '/usr/local/bin/php'  -n   -d "output_handler=" -d "open_basedir=" -d "disable_functions=" -d "output_buffering=Off" -d "error_reporting=32767" -d "display_errors=1" -d "display_startup_errors=1" -d "log_errors=0" -d "html_errors=0" -d "track_errors=0" -d "report_memleaks=1" -d "report_zend_debug=0" -d "docref_root=" -d "docref_ext=.html" -d "error_prepend_string=" -d "error_append_string=" -d "auto_prepend_file=" -d "auto_append_file=" -d "ignore_repeated_errors=0" -d "precision=14" -d "serialize_precision=-1" -d "memory_limit=128M" -d "opcache.fast_shutdown=0" -d "opcache.file_update_protection=0" -d "opcache.revalidate_freq=0" -d "opcache.jit_hot_loop=1" -d "opcache.jit_hot_func=1" -d "opcache.jit_hot_return=1" -d "opcache.jit_hot_side_exit=1" -d "zend.assertions=1" -d "zend.exception_ignore_args=0" -d "zend.exception_string_param_max_len=15" -d "short_open_tag=0" -d "extension=/ext/modules/firebird.so" -d "extension=pcntl" -d "session.auto_start=0" -d "zlib.output_compression=Off" -f "/ext/tests/coverage/query_array_complex.php"  2>&1
    ;;
"lldb")
    lldb -- '/usr/local/bin/php'  -n   -d "output_handler=" -d "open_basedir=" -d "disable_functions=" -d "output_buffering=Off" -d "error_reporting=32767" -d "display_errors=1" -d "display_startup_errors=1" -d "log_errors=0" -d "html_errors=0" -d "track_errors=0" -d "report_memleaks=1" -d "report_zend_debug=0" -d "docref_root=" -d "docref_ext=.html" -d "error_prepend_string=" -d "error_append_string=" -d "auto_prepend_file=" -d "auto_append_file=" -d "ignore_repeated_errors=0" -d "precision=14" -d "serialize_precision=-1" -d "memory_limit=128M" -d "opcache.fast_shutdown=0" -d "opcache.file_update_protection=0" -d "opcache.revalidate_freq=0" -d "opcache.jit_hot_loop=1" -d "opcache.jit_hot_func=1" -d "opcache.jit_hot_return=1" -d "opcache.jit_hot_side_exit=1" -d "zend.assertions=1" -d "zend.exception_ignore_args=0" -d "zend.exception_string_param_max_len=15" -d "short_open_tag=0" -d "extension=/ext/modules/firebird.so" -d "extension=pcntl" -d "session.auto_start=0" -d "zlib.output_compression=Off" -f "/ext/tests/coverage/query_array_complex.php"  2>&1
    ;;
"valgrind")
    USE_ZEND_ALLOC=0 valgrind $2 '/usr/local/bin/php'  -n   -d "output_handler=" -d "open_basedir=" -d "disable_functions=" -d "output_buffering=Off" -d "error_reporting=32767" -d "display_errors=1" -d "display_startup_errors=1" -d "log_errors=0" -d "html_errors=0" -d "track_errors=0" -d "report_memleaks=1" -d "report_zend_debug=0" -d "docref_root=" -d "docref_ext=.html" -d "error_prepend_string=" -d "error_append_string=" -d "auto_prepend_file=" -d "auto_append_file=" -d "ignore_repeated_errors=0" -d "precision=14" -d "serialize_precision=-1" -d "memory_limit=128M" -d "opcache.fast_shutdown=0" -d "opcache.file_update_protection=0" -d "opcache.revalidate_freq=0" -d "opcache.jit_hot_loop=1" -d "opcache.jit_hot_func=1" -d "opcache.jit_hot_return=1" -d "opcache.jit_hot_side_exit=1" -d "zend.assertions=1" -d "zend.exception_ignore_args=0" -d "zend.exception_string_param_max_len=15" -d "short_open_tag=0" -d "extension=/ext/modules/firebird.so" -d "extension=pcntl" -d "session.auto_start=0" -d "zlib.output_compression=Off" -f "/ext/tests/coverage/query_array_complex.php"  2>&1
    ;;
"rr")
    rr record $2 '/usr/local/bin/php'  -n   -d "output_handler=" -d "open_basedir=" -d "disable_functions=" -d "output_buffering=Off" -d "error_reporting=32767" -d "display_errors=1" -d "display_startup_errors=1" -d "log_errors=0" -d "html_errors=0" -d "track_errors=0" -d "report_memleaks=1" -d "report_zend_debug=0" -d "docref_root=" -d "docref_ext=.html" -d "error_prepend_string=" -d "error_append_string=" -d "auto_prepend_file=" -d "auto_append_file=" -d "ignore_repeated_errors=0" -d "precision=14" -d "serialize_precision=-1" -d "memory_limit=128M" -d "opcache.fast_shutdown=0" -d "opcache.file_update_protection=0" -d "opcache.revalidate_freq=0" -d "opcache.jit_hot_loop=1" -d "opcache.jit_hot_func=1" -d "opcache.jit_hot_return=1" -d "opcache.jit_hot_side_exit=1" -d "zend.assertions=1" -d "zend.exception_ignore_args=0" -d "zend.exception_string_param_max_len=15" -d "short_open_tag=0" -d "extension=/ext/modules/firebird.so" -d "extension=pcntl" -d "session.auto_start=0" -d "zlib.output_compression=Off" -f "/ext/tests/coverage/query_array_complex.php"  2>&1
    ;;
*)
    '/usr/local/bin/php'  -n   -d "output_handler=" -d "open_basedir=" -d "disable_functions=" -d "output_buffering=Off" -d "error_reporting=32767" -d "display_errors=1" -d "display_startup_errors=1" -d "log_errors=0" -d "html_errors=0" -d "track_errors=0" -d "report_memleaks=1" -d "report_zend_debug=0" -d "docref_root=" -d "docref_ext=.html" -d "error_prepend_string=" -d "error_append_string=" -d "auto_prepend_file=" -d "auto_append_file=" -d "ignore_repeated_errors=0" -d "precision=14" -d "serialize_precision=-1" -d "memory_limit=128M" -d "opcache.fast_shutdown=0" -d "opcache.file_update_protection=0" -d "opcache.revalidate_freq=0" -d "opcache.jit_hot_loop=1" -d "opcache.jit_hot_func=1" -d "opcache.jit_hot_return=1" -d "opcache.jit_hot_side_exit=1" -d "zend.assertions=1" -d "zend.exception_ignore_args=0" -d "zend.exception_string_param_max_len=15" -d "short_open_tag=0" -d "extension=/ext/modules/firebird.so" -d "extension=pcntl" -d "session.auto_start=0" -d "zlib.output_compression=Off" -f "/ext/tests/coverage/query_array_complex.php"  2>&1
    ;;
esac