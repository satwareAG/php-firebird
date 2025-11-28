#!/bin/bash
set -e

echo "Testing PHP Firebird extension..."

# Change to extension root directory
cd /ext

# Ensure extension is built and compatible
if [ ! -f modules/interbase.so ]; then
    echo "Extension not found. Building first..."
    /ext/scripts/container/build.sh
else
    # Check if extension loads successfully (handles API mismatch leftovers)
    if ! php -d extension=$(pwd)/modules/interbase.so -r "exit(extension_loaded('interbase') ? 0 : 1);" >/dev/null 2>&1; then
        echo "Extension found but failed to load (possible API mismatch). Rebuilding..."
        /ext/scripts/container/build.sh
    fi
fi

# Print PHP version and extension information
php -d extension=$(pwd)/modules/interbase.so -r "
    echo 'PHP Version: ' . PHP_VERSION . PHP_EOL;
    echo 'Extension loaded: ' . (extension_loaded('interbase') ? 'YES' : 'NO') . PHP_EOL;
    echo 'Extension version: ' . phpversion('interbase') . PHP_EOL;
"

# Run tests if available
if [ -d "tests" ]; then
    echo "Running tests..."
    # Allow an optional argument to restrict the test set (e.g. a single .phpt file
    # or subdirectory). Defaults to running the entire tests/ tree.
    if [ $# -eq 0 ]; then
        TARGET="tests/"
    else
        TARGET="$@"
    fi
    TEST_PHP_EXECUTABLE=/usr/local/bin/php php run-tests.php -d extension=$(pwd)/modules/interbase.so $TARGET
else
    echo "No test directory found. Skipping tests."
fi

echo "Testing complete."
