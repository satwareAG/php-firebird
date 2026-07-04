#!/bin/bash
set -e

# Change to extension root directory
cd /ext
source "$(dirname "$0")/lib/logging.sh"
source "$(dirname "$0")/lib/clean-artifacts.sh"

log_info "Testing PHP Firebird extension..."

# Clean test artifacts from previous runs
clean_test_artifacts "$(pwd)"

# Ensure extension is built and compatible
if [ ! -f modules/firebird.so ]; then
    log_info "Extension not found. Building first..."
    /ext/scripts/build.sh
else
    # Check if extension loads successfully (handles API mismatch leftovers)
    if ! php -n -d extension=$(pwd)/modules/firebird.so -r "exit(extension_loaded('firebird') ? 0 : 1);" >/dev/null 2>&1; then
        log_warn "Extension found but failed to load (possible API mismatch). Rebuilding..."
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
    log_info "Running tests..."
    # Allow an optional argument to restrict the test set (e.g. a single .phpt file
    # or subdirectory). Defaults to running the entire tests/ tree.
    if [ $# -eq 0 ]; then
        TARGET="tests/"
        # Include pdo_fbird tests if they exist
        if [ -d "tests/pdo_fbird" ]; then
            TARGET="$TARGET tests/pdo_fbird/"
        fi
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
            # Try tests/pdo_fbird/ prefix
            elif [ -e "tests/pdo_fbird/$arg" ]; then
                resolved="tests/pdo_fbird/$arg"
            elif [ -e "tests/pdo_fbird/$arg.phpt" ]; then
                resolved="tests/pdo_fbird/$arg.phpt"
            else
                log_error "Cannot find test file \"$arg\"."
                exit 1
            fi
            TARGET="$TARGET $resolved"
        done
    fi
    # Build extension arguments - load firebird and pdo_fbird separately
    EXT_ARGS="-d extension=$(pwd)/modules/firebird.so"
    if [ -f pdo_fbird/modules/pdo_fbird.so ]; then
        EXT_ARGS="$EXT_ARGS -d extension=$(pwd)/pdo_fbird/modules/pdo_fbird.so"
    fi
    # Check if pcntl is available (needed for fork tests)
    if php -m 2>/dev/null | grep -q pcntl; then
        EXT_ARGS="$EXT_ARGS -d extension=pcntl"
    fi
    TEST_PHP_EXECUTABLE=$(which php) TEST_PHP_ARGS="-n" php -n run-tests.php $EXT_ARGS $TARGET
else
    log_warn "No test directory found. Skipping tests."
fi

log_info "Testing complete."
