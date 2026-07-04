#!/bin/bash
set -e

# Modern implementation of test runner across PHP versions
# Usage: ./test_matrix.sh [container_name] [firebird_server] [test_files...]
# Example: ./test_matrix.sh php83-dev
# Example: ./test_matrix.sh php85-fb5-dev firebird50
# Example: ./test_matrix.sh php84-fb3-dev "" tests/fbird_blob_001.phpt
# Example (all versions, specific test): ./test_matrix.sh "" "" tests/fbird_blob_001.phpt
#
# Sanitizer targets (not in default matrix - use explicitly):
#   ./test_matrix.sh php83-tsan           # ThreadSanitizer + ZTS
#   ./test_matrix.sh php83-asan           # AddressSanitizer
#
# Arguments:
#   container_name   - PHP container to test (e.g., php82-dev, php83-tsan, php84-fb3-dev)
#   firebird_server  - Target Firebird server (firebird30, firebird40, firebird50)
#   test_files       - Optional specific test files to run

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

PROJECT_ROOT=$(pwd)
DOCKER_DIR="$PROJECT_ROOT/docker"

echo -e "${BLUE}=== PHP Firebird Test Matrix ===${NC}"

# Helper: clean test artifacts left by run-tests.php
# These files accumulate between runs and can cause stale results or confusion.
clean_test_artifacts() {
    local cleaned=0
    for dir in tests pdo_fbird/tests; do
        if [ -d "$PROJECT_ROOT/$dir" ]; then
            # Pass 1: recursive for unambiguous artifact extensions
            cleaned=$(( cleaned + $(find "$PROJECT_ROOT/$dir" \
                \( -name '*.diff' -o -name '*.out' -o -name '*.exp' \
                   -o -name '*.log' -o -name '*.mem' \) \
                2>/dev/null | wc -l) ))
            find "$PROJECT_ROOT/$dir" \
                \( -name '*.diff' -o -name '*.out' -o -name '*.exp' \
                   -o -name '*.log' -o -name '*.mem' \) \
                -delete 2>/dev/null || true
            # Pass 2: top-level only for *.php and *.sh
            # (protects tracked source files in subdirs like tests/sanitizer/)
            cleaned=$(( cleaned + $(find "$PROJECT_ROOT/$dir" -maxdepth 1 \
                \( -name '*.php' -o -name '*.sh' \) \
                -not -name 'common.inc' -not -name 'config.inc' \
                -not -name 'firebird.inc' -not -name 'functions.inc' \
                -not -name 'skipif.inc' \
                2>/dev/null | wc -l) ))
            find "$PROJECT_ROOT/$dir" -maxdepth 1 \
                \( -name '*.php' -o -name '*.sh' \) \
                -not -name 'common.inc' -not -name 'config.inc' \
                -not -name 'firebird.inc' -not -name 'functions.inc' \
                -not -name 'skipif.inc' \
                -delete 2>/dev/null || true
        fi
    done
    if [ "$cleaned" -gt 0 ]; then
        echo -e "${YELLOW}Cleaned $cleaned test artifact(s) from previous run${NC}"
    fi
}

# 1. Check Prerequisites
if ! command -v docker >/dev/null 2>&1; then
    echo -e "${RED}Error: docker is not installed.${NC}"
    exit 1
fi

# 2. Ensure Environment
echo -e "${BLUE}>> Infrastructure Check${NC}"
cd "$DOCKER_DIR"

# Export UID/GID for docker compose substitution
export CURRENT_UID=$(id -u)
export CURRENT_GID=$(id -g)

# Bring up containers (ensure up-to-date)
# When a specific container is requested ($1), start only that container
# and its dependencies (Docker Compose handles depends_on automatically).
# When no container is specified (full matrix), start all services.
if [ -n "$1" ]; then
    echo -e "${BLUE}Starting container: $1 (+ dependencies)${NC}"
    if ! docker compose up -d "$1"; then
        echo -e "${RED}Failed to start Docker environment for $1.${NC}"
        exit 1
    fi
else
    if ! docker compose up -d --remove-orphans; then
        echo -e "${RED}Failed to start Docker environment.${NC}"
        exit 1
    fi
fi

# 3. Define Targets
if [ -n "$1" ]; then
    TARGETS=("$1")
else
    # Test all PHP containers across all supported Firebird client libraries (12 combinations)
    # Sanitizer containers (php83-tsan, php83-asan) excluded by default - use explicitly
    TARGETS=(
        "php82-fb3-dev" "php82-dev" "php82-fb5-dev"
        "php83-fb3-dev" "php83-dev" "php83-fb5-dev"
        "php84-fb3-dev" "php84-dev" "php84-fb5-dev"
        "php85-fb3-dev" "php85-dev" "php85-fb5-dev"
    )
fi

# 4. Parse Firebird Server Target (optional second argument)
# Auto-detect server based on container name if not explicitly specified
auto_detect_firebird_server() {
    local container="$1"
    case "$container" in
        *-fb3-*)
            echo "firebird30"
            ;;
        *-fb5-*|*-tsan)
            # TSan container defaults to firebird50
            echo "firebird50"
            ;;
        *-asan)
            # ASan container defaults to firebird40
            echo "firebird40"
            ;;
        *)
            # Default dev containers use firebird40
            echo ""
            ;;
    esac
}

FIREBIRD_SERVER=""
if [ -n "$2" ]; then
    case "$2" in
        firebird25|firebird30|firebird40|firebird50)
            FIREBIRD_SERVER="$2"
            echo -e "${BLUE}>> Firebird Server Target: $FIREBIRD_SERVER${NC}"
            ;;
        "")
            # Empty string - use container default
            ;;
        *)
            echo -e "${RED}Error: Invalid Firebird server '$2'${NC}"
            echo "Valid servers: firebird25, firebird30, firebird40, firebird50"
            echo "Usage: $0 [container] [firebird_server] [test_files...]"
            exit 1
            ;;
    esac
fi

# 5. Define Test Targets (optional, starting from position 3)
TEST_TARGETS=""
if [ -n "$3" ]; then
    # Capture all arguments starting from position 3
    TEST_TARGETS="${@:3}"
    echo -e "${BLUE}>> Targeting specific tests: $TEST_TARGETS${NC}"
fi

# 6. Execute Matrix
FAILED_CONTAINERS=()
PASSED_CONTAINERS=()

# Clean stale test artifacts before starting the matrix
echo -e "${BLUE}>> Cleaning test artifacts from previous runs${NC}"
clean_test_artifacts

# Helper: clean test environment (orphaned processes + leftover DB files) between runs.
# Runs on the host via docker compose exec — can reach both PHP and Firebird containers.
# Called BEFORE each container's test suite starts, so no test runner process is at risk.
clean_test_environment() {
    local container="$1"
    # Kill orphaned PHP child processes from previous test run.
    # Safe: runs BEFORE this container's test suite starts.
    if [ -n "$(docker compose ps -q "$container" 2>/dev/null)" ]; then
        docker compose exec -u root "$container" \
            bash -c "pkill -9 -f 'php.*firebird' 2>/dev/null || true" \
            2>/dev/null || true
    fi
    # Clean leftover .fdb files on all Firebird server containers.
    # Orphaned processes hold locks on these files, preventing DROP DATABASE.
    for fb in firebird30 firebird40 firebird50; do
        if [ -n "$(docker compose ps -q "$fb" 2>/dev/null)" ]; then
            docker compose exec -u root "$fb" \
                bash -c "rm -f /tmp/*.fdb 2>/dev/null || true" \
                2>/dev/null || true
        fi
    done
}

for CONTAINER in "${TARGETS[@]}"; do
    echo -e "\n${BLUE}>> Testing Target: $CONTAINER${NC}"

    # Clean test artifacts between container runs to prevent cross-contamination
    clean_test_artifacts

    # Clean orphaned processes + leftover DB files from previous container run
    clean_test_environment "$CONTAINER"

    # Verify container state
    if [ -z "$(docker compose ps -q $CONTAINER)" ]; then
        echo -e "${YELLOW}Container $CONTAINER is not running. Starting...${NC}"
        docker compose up -d $CONTAINER
        if [ -z "$(docker compose ps -q $CONTAINER)" ]; then
            echo -e "${RED}Failed to start container $CONTAINER.${NC}"
            echo "Available services:"
            docker compose ps --services
            FAILED_CONTAINERS+=("$CONTAINER (failed to start)")
            continue
        fi
    fi

    # Fix permissions as root first (in case previous runs left root-owned files)
    echo "Fixing permissions..."
    docker compose exec -u root "$CONTAINER" chown -R $CURRENT_UID:$CURRENT_GID /ext

    # Build environment variable options for docker exec
    # Determine which Firebird server to use:
    # 1. Explicit argument takes precedence
    # 2. Auto-detect from container name (e.g., php85-fb5-dev -> firebird50)
    # 3. Fall back to container's default environment
    TARGET_SERVER="$FIREBIRD_SERVER"
    if [ -z "$TARGET_SERVER" ]; then
        TARGET_SERVER=$(auto_detect_firebird_server "$CONTAINER")
    fi
    
    ENV_OPTS=""
    if [ -n "$TARGET_SERVER" ]; then
        # When overriding FIREBIRD_HOST we also force FIREBIRD_DB_DIR to /tmp.
        # Reason: some PHP containers mount /firebird volumes that are NOT present
        # (or writable) in the selected Firebird server container, which breaks
        # CREATE DATABASE during SKIPIF/init_db().
        ENV_OPTS="-e FIREBIRD_HOST=$TARGET_SERVER -e FIREBIRD_DB_DIR=/tmp"
        echo "Using Firebird server: $TARGET_SERVER (FIREBIRD_DB_DIR=/tmp)"
    fi

    # Run Build & Test in single session
    # CRITICAL: build.sh must clean .dep files between PHP versions
    # These dependency files contain absolute paths to PHP headers (e.g., /usr/local/include/php/main/php_stdint.h)
    # that differ between PHP versions, causing "No rule to make target" errors if not cleaned.
    echo "Running build and test suite (Server: ${TARGET_SERVER:-default}, Tests: ${TEST_TARGETS:-ALL})..."

    # Construct command with optional target
    CMD="/ext/scripts/build.sh && /ext/scripts/test.sh"
    if [ -n "$TEST_TARGETS" ]; then
        CMD="$CMD $TEST_TARGETS"
    fi

    if docker compose exec $ENV_OPTS "$CONTAINER" bash -c "$CMD"; then
        echo -e "${GREEN}✓ $CONTAINER passed${NC}"
        PASSED_CONTAINERS+=("$CONTAINER")
    else
        echo -e "${RED}✗ $CONTAINER failed${NC}"
        FAILED_CONTAINERS+=("$CONTAINER")
    fi
done

# Final cleanup after all containers have run
echo -e "\n${BLUE}>> Final artifact cleanup${NC}"
clean_test_artifacts

echo -e "\n${BLUE}=== Test Matrix Summary ===${NC}"
echo -e "Passed (${#PASSED_CONTAINERS[@]}): ${PASSED_CONTAINERS[*]}"
echo -e "Failed (${#FAILED_CONTAINERS[@]}): ${FAILED_CONTAINERS[*]}"

if [ ${#FAILED_CONTAINERS[@]} -eq 0 ]; then
    echo -e "\n${GREEN}=== All Targeted Versions Passed ===${NC}"
    exit 0
else
    echo -e "\n${RED}=== Some Versions Failed ===${NC}"
    exit 1
fi