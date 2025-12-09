#!/bin/bash
# scripts/qa_local.sh
# Unified local QA workflow: setup -> build(bear) -> analysis -> test
# Usage: ./scripts/qa_local.sh [container_name] [mode]
#   container_name: defaults to php82-dev
#   mode: 'fast' (default) or 'full' (includes ASan/Valgrind)

set -e

# 1. Parse Args
CONTAINER=${1:-php82-dev}
MODE=${2:-fast} # fast | full

PROJECT_ROOT=$(pwd)
DOCKER_DIR="$PROJECT_ROOT/docker"

# Check for color support
if [ -t 1 ]; then
    GREEN='\033[0;32m'
    BLUE='\033[0;34m'
    RED='\033[0;31m'
    NC='\033[0m'
else
    GREEN=''
    BLUE=''
    RED=''
    NC=''
fi

echo -e "${BLUE}=== PHP Firebird Local QA Workflow ($CONTAINER) ===${NC}"

# 2. Start Container
echo -e "${BLUE}>> Ensuring environment is running...${NC}"
cd "$DOCKER_DIR"
if ! docker compose up -d "$CONTAINER"; then
    echo -e "${RED}Failed to start container $CONTAINER${NC}"
    exit 1
fi

# 3. Install QA Tools (if missing)
echo -e "${BLUE}>> Checking/Installing QA tools in container...${NC}"
# We check for bear, clang-tidy, and cppcheck (apt-get update only if needed)
docker compose exec -u root "$CONTAINER" bash -c "
    export DEBIAN_FRONTEND=noninteractive
    MISSING=0
    if ! command -v bear >/dev/null; then MISSING=1; fi
    if ! command -v clang-tidy >/dev/null; then MISSING=1; fi
    if ! command -v cppcheck >/dev/null; then MISSING=1; fi
    if ! command -v xmllint >/dev/null; then MISSING=1; fi

    if [ \$MISSING -eq 1 ]; then
        echo 'Installing tools: bear clang-tools clang-tidy cppcheck libxml2-utils...'
        apt-get update -qq && apt-get install -y -qq bear clang-tools clang-tidy cppcheck libxml2-utils

        # Fix clang-tidy symlink if missing (and not managed by update-alternatives)
        if ! command -v clang-tidy >/dev/null; then
            echo 'Fixing clang-tidy symlink...'
            TARGET=\"\"
            # Check standard llvm locations
            for bin in /usr/lib/llvm-*/bin/clang-tidy; do
                if [ -x \"\$bin\" ]; then
                    TARGET=\"\$bin\"
                fi
            done

            if [ -z \"\$TARGET\" ]; then
                 # Fallback: find anywhere in /usr/lib
                 TARGET=\$(find /usr/lib -name \"clang-tidy*\" -type f 2>/dev/null | head -n 1)
            fi

            if [ -n \"\$TARGET\" ]; then
                ln -sf \"\$TARGET\" /usr/bin/clang-tidy
                echo \"Linked /usr/bin/clang-tidy to \$TARGET\"
            else
                echo 'Error: Could not find clang-tidy binary.'
                find /usr/lib -name \"clang-tidy*\" -type f 2>/dev/null || echo 'Find returned nothing'
            fi
        fi
    else
        echo 'QA tools present.'
    fi
"

# 4. Build with Bear (Generates compile_commands.json)
echo -e "${BLUE}>> Building extension with Bear (Database Generation)...${NC}"
# This ensures we have a valid compilation database for static analysis
# effectively replacing 'build-extension.sh' for this workflow but with 'bear'
# We clean to ensure the DB is complete.
docker compose exec "$CONTAINER" bash -c "
    cd /ext
    if [ -f Makefile ]; then make clean; phpize --clean; fi
    phpize
    CPPFLAGS='-I/usr/include/firebird' ./configure --with-firebird=/usr
    bear -- make -j\$(nproc)
"

# 5. Run Static Analysis
echo -e "${BLUE}>> Running Clang-Tidy...${NC}"
if ! docker compose exec "$CONTAINER" /ext/scripts/container/analysis/clang_tidy.sh; then
    echo -e "${RED}Clang-Tidy failed.${NC}"
    exit 1
fi

echo -e "${BLUE}>> Running Cppcheck...${NC}"
if ! docker compose exec "$CONTAINER" /ext/scripts/container/analysis/cppcheck.sh; then
    echo -e "${RED}Cppcheck failed.${NC}"
    exit 1
fi

# 6. Run Unit Tests
echo -e "${BLUE}>> Running Unit Tests...${NC}"
if ! docker compose exec "$CONTAINER" /ext/scripts/container/test.sh; then
    echo -e "${RED}Unit tests failed.${NC}"
    exit 1
fi

# 7. Dynamic Analysis (Optional)
if [ "$MODE" == "full" ]; then
    echo -e "${BLUE}>> Running Valgrind (on standard build)...${NC}"
    # Valgrind parses the current binary. Since 'make install' wasn't run in step 4 (just make),
    # test-extension.sh finds modules/interbase.so.
    # test_with_valgrind.sh also uses ./modules/interbase.so.
    if ! docker compose exec "$CONTAINER" /ext/scripts/container/analysis/valgrind.sh; then
        echo -e "${RED}Valgrind failed.${NC}"
        exit 1
    fi

    echo -e "${BLUE}>> Running AddressSanitizer (Rebuilds with ASan)...${NC}"
    # ASan requires rebuild. This invalidates the previous build.
    if ! docker compose exec "$CONTAINER" /ext/scripts/container/analysis/asan.sh; then
        echo -e "${RED}AddressSanitizer tests failed.${NC}"
        exit 1
    fi
fi

echo -e "\n${GREEN}=== QA Workflow Completed Successfully ===${NC}"
