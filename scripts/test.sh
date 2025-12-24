#!/bin/bash
set -e

echo "Testing PHP Firebird extension..."

# Change to extension root directory
cd /ext

# Ensure extension is built and compatible
if [ ! -f modules/firebird.so ]; then
    echo "Extension not found. Building first..."
    /ext/scripts/build.sh
else
    # Check if extension loads successfully (handles API mismatch leftovers)
    if ! php -n -d extension=$(pwd)/modules/firebird.so -r "exit(extension_loaded('firebird') ? 0 : 1);" >/dev/null 2>&1; then
        echo "Extension found but failed to load (possible API mismatch). Rebuilding..."
        /ext/scripts/build.sh
    fi
fi

# Print PHP version and extension information
php -n -d extension=$(pwd)/modules/firebird.so -r "
    echo 'PHP Version: ' . PHP_VERSION . PHP_EOL;
    echo 'Extension loaded: ' . (extension_loaded('firebird') ? 'YES' : 'NO') . PHP_EOL;
    echo 'Extension version: ' . phpversion('firebird') . PHP_EOL;
"

# Run tests if available
if [ -d "tests" ]; then
    echo "Running tests..."
    # Allow an optional argument to restrict the test set (e.g. a single .phpt file
    # or subdirectory). Defaults to running the entire tests/ tree.
    if [ $# -eq 0 ]; then
        TARGET="tests/"
    else
        # Resolve test file arguments to actual paths
        TARGET=""
        for arg in "$@"; do
            resolved=""
            # If argument is already a valid file or directory, use it as-is
            if [ -e "$arg" ]; then
                resolved="$arg"
            # Try tests/ prefix
            elif [ -e "tests/$arg" ]; then
                resolved="tests/$arg"
            # Try adding .phpt extension
            elif [ -e "$arg.phpt" ]; then
                resolved="$arg.phpt"
            # Try tests/ prefix with .phpt extension
            elif [ -e "tests/$arg.phpt" ]; then
                resolved="tests/$arg.phpt"
            else
                echo "Cannot find test file \"$arg\"."
                exit 1
            fi
            TARGET="$TARGET $resolved"
        done
    fi
    TEST_PHP_EXECUTABLE=/usr/local/bin/php TEST_PHP_ARGS="-n" php -n run-tests.php -d extension=$(pwd)/modules/firebird.so $TARGET
else
    echo "No test directory found. Skipping tests."
fi

echo "Testing complete."
