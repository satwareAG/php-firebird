#!/bin/bash
# scripts/test_with_valgrind.sh

set -e

echo "Running Valgrind memory analysis..."

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
valgrind $VALGRIND_OPTS php -d extension=./modules/interbase.so -r "echo 'Extension loaded';"

# Run comprehensive test that exercises modernized functions
valgrind $VALGRIND_OPTS php -d extension=./modules/interbase.so -r '
$version = fbu_get_client_version(null);
echo "Client version (null test): " . $version . PHP_EOL;
'

echo "✅ Valgrind analysis completed"
