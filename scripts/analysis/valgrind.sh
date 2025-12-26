#!/bin/bash
# scripts/analysis/valgrind.sh
# Valgrind memory analysis for PHP Firebird extension

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "Running Valgrind memory analysis..."

# Environment variables for better Valgrind analysis
export ZEND_DONT_UNLOAD_MODULES=1
export USE_ZEND_ALLOC=0

# Determine PHP binary (php or phpdbg)
PHP_BINARY=${PHP_BINARY:-php}
PHP_ARGS=""

# If using phpdbg, we need the -qrr flag (Quiet, Run, Return) to execute and exit
if [[ "$PHP_BINARY" == *"phpdbg"* ]]; then
    echo "Using phpdbg for Valgrind analysis..."
    PHP_ARGS="-qrr"
fi

# Locate suppressions file
SUPP_FILE="$PROJECT_ROOT/valgrind-php.supp"
SUPP_OPTS=""

if [ -f "$SUPP_FILE" ]; then
    echo "Using suppressions file: $SUPP_FILE"
    SUPP_OPTS="--suppressions=$SUPP_FILE"
else
    echo "⚠️ Suppressions file not found at $SUPP_FILE"
    # Try /ext/valgrind-php.supp (Docker path)
    if [ -f "/ext/valgrind-php.supp" ]; then
        echo "Using suppressions file: /ext/valgrind-php.supp"
        SUPP_OPTS="--suppressions=/ext/valgrind-php.supp"
    fi
fi

# Valgrind options for PHP extension testing
VALGRIND_OPTS="
    --tool=memcheck
    --leak-check=full
    --show-leak-kinds=all
    --track-origins=yes
    --verbose
    --error-exitcode=1
    $SUPP_OPTS"

# Run simple extension load test
echo ">> Test 1: Extension Loading"
valgrind $VALGRIND_OPTS $PHP_BINARY $PHP_ARGS -d extension=./modules/firebird.so -r "echo 'Extension loaded';"

# Run comprehensive test that exercises modernized functions
echo ">> Test 2: Client Version Check"
valgrind $VALGRIND_OPTS $PHP_BINARY $PHP_ARGS -d extension=./modules/firebird.so -r '
if (function_exists("fbu_get_client_version")) {
    $version = fbu_get_client_version(null);
    echo "Client version (null test): " . $version . PHP_EOL;
} else {
    echo "fbu_get_client_version not found, skipping test" . PHP_EOL;
}
'

echo "✅ Valgrind analysis completed"
