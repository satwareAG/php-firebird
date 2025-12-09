#!/bin/bash
# scripts/test_with_valgrind.sh

set -e

echo "Running Valgrind memory analysis..."

# Determine PHP binary (php or phpdbg)
PHP_BINARY=${PHP_BINARY:-php}
PHP_ARGS=""

# If using phpdbg, we need the -qrr flag (Quiet, Run, Return) to execute and exit
if [[ "$PHP_BINARY" == *"phpdbg"* ]]; then
    echo "Using phpdbg for Valgrind analysis..."
    PHP_ARGS="-qrr"
fi

# Valgrind options for PHP extension testing
VALGRIND_OPTS="
    --tool=memcheck
    --leak-check=full
    --show-leak-kinds=all
    --track-origins=yes
    --verbose
    --error-exitcode=1
    --suppressions=valgrind-php.supp"

# Run simple extension load test
valgrind $VALGRIND_OPTS $PHP_BINARY $PHP_ARGS -d extension=./modules/firebird.so -r "echo 'Extension loaded';"

# Run comprehensive test that exercises modernized functions
valgrind $VALGRIND_OPTS $PHP_BINARY $PHP_ARGS -d extension=./modules/firebird.so -r '
$version = fbu_get_client_version(null);
echo "Client version (null test): " . $version . PHP_EOL;
'

echo "✅ Valgrind analysis completed"
