#!/bin/bash
set -e

echo "Testing PHP Firebird extension..."

# Change to extension root directory
cd /ext

# Ensure extension is built
if [ ! -f modules/interbase.so ]; then
    echo "Extension not found. Building first..."
    /ext/satware-docs/environments/docker/scripts/build-extension.sh
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
    TEST_PHP_EXECUTABLE=/usr/local/bin/php php run-tests.php -d extension=$(pwd)/modules/interbase.so tests/
else
    echo "No test directory found. Skipping tests."
fi

echo "Testing complete."
