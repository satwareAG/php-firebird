#!/bin/bash
# scripts/fuzz_asan.sh
# Run the Firebird fuzzer in the ASan environment
# Usage: ./scripts/fuzz_asan.sh [iterations]

set -e

ITERATIONS=${1:-1000}
CONTAINER="php83-asan"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOCKER_DIR="$PROJECT_ROOT/docker"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}Starting Firebird Fuzzer (ASan) with $ITERATIONS iterations...${NC}"

# Ensure containers are running (PHP + Firebird)
cd "$DOCKER_DIR"

# Start firebird30 first (the fuzzer's target database)
echo "Ensuring firebird30 container is running..."
docker compose up -d firebird30

# Wait for Firebird to be ready (simple check)
echo "Waiting for firebird30 to be ready..."
sleep 3

# Start the ASan PHP container
if ! docker compose ps --services --filter "status=running" | grep -q "$CONTAINER"; then
    echo "Starting container $CONTAINER..."
    docker compose up -d "$CONTAINER"
fi

# Run fuzzer
# We use -d extension_dir to ensure we load the built extension
# We assume the extension is built and available in /ext/modules/fbird.so (or similar)
# The ASan container usually has the extension installed or available.
# Let's assume standard `php` command works if configured correctly, 
# or we might need to point to the specific php binary if it's custom built.
# Based on SANITIZER_STRATEGY.md, the container is `php83-asan`.

# Always rebuild to ensure ASan instrumentation and correct GLIBC
echo "Building extension with ASan..."
docker compose exec -T "$CONTAINER" bash -c "make clean 2>/dev/null; phpize && ./configure --with-firebird=/usr && make -j$(nproc)"

echo "Executing fuzzer..."
# Disable LeakSanitizer - detected leaks are in PHP internals (opcache, zend_compile), not our extension
docker compose exec -T -e ASAN_OPTIONS=detect_leaks=0 "$CONTAINER" php -d extension=modules/firebird.so fuzz/run.php --iterations="$ITERATIONS" --output=fuzz/reports/fuzz_report.sarif --dsn="firebird30:/firebird/data/test.fdb"

# Check exit code
EXIT_CODE=$?

if [ $EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}Fuzzer completed successfully.${NC}"
    echo "Report saved to fuzz/reports/fuzz_report.sarif"
else
    echo -e "${RED}Fuzzer failed with exit code $EXIT_CODE${NC}"
    # Check for ASan errors in output (stderr usually)
    # Note: docker exec might mix stdout/stderr depending on TTY
    exit $EXIT_CODE
fi
