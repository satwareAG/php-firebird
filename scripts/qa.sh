#!/bin/bash
# scripts/qa.sh
# Comprehensive local QA workflow with all quality checks
# Usage: ./scripts/qa.sh [options]
#
# Options:
#   --container NAME   Container to use (default: php84-fb3-dev — amicron-platform baseline)
#   --mode MODE        fast|standard|full|security|fuzz|matrix (default: standard)
#   --valgrind         Run Valgrind memory analysis (can combine with --container)
#   --asan             Run AddressSanitizer tests (uses php83-asan container)
#   --skip-build       Skip C extension build (use existing)
#   --php-only         Only run PHP analysis (PHPStan, PHPCS)
#   --fail-fast        Stop at first error with detailed output
#   --help             Show this help message
#
# Modes:
#   fast     - Static analysis only (clang-tidy, cppcheck, PHPStan)
#   standard - Fast + unit tests
#   full     - Standard + Valgrind memory analysis + UBSan
#   security - Full + Gitleaks secret scanning
#   fuzz     - Run fuzzing (without ASan due to PHP compatibility issues)
#   matrix   - Test across PHP 8.2/FB3 (oldest) and PHP 8.5/FB5 (newest)
#
# Matrix Testing:
#   The 'matrix' mode runs the full test suite across:
#     - php82-fb3-dev (PHP 8.2 + Firebird 3.0) - oldest supported
#     - php85-fb5-dev (PHP 8.5 + Firebird 5.0) - newest supported
#
# Examples:
#   ./scripts/qa.sh --mode fast                    # Quick static analysis
#   ./scripts/qa.sh --mode matrix                  # Test oldest + newest combinations
#   ./scripts/qa.sh --valgrind --container php82-fb3-dev  # Valgrind on PHP 8.2/FB3
#   ./scripts/qa.sh --valgrind --container php85-fb5-dev  # Valgrind on PHP 8.5/FB5
#   ./scripts/qa.sh --asan                         # ASan with php83-asan container

set -e

# Defaults — php84-fb3-dev is the amicron-platform baseline (PHP 8.4 + Firebird 3)
CONTAINER="php84-fb3-dev"
MODE="standard"
SKIP_BUILD=false
PHP_ONLY=false
FAIL_FAST=false
RUN_VALGRIND=false
RUN_ASAN=false

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --container)
            CONTAINER="$2"
            shift 2
            ;;
        --mode)
            MODE="$2"
            shift 2
            ;;
        --valgrind)
            RUN_VALGRIND=true
            shift
            ;;
        --asan)
            RUN_ASAN=true
            CONTAINER="php83-asan"
            shift
            ;;
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --php-only)
            PHP_ONLY=true
            shift
            ;;
        --fail-fast)
            FAIL_FAST=true
            shift
            ;;
        --help)
            head -35 "$0" | tail -31
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOCKER_DIR="$PROJECT_ROOT/docker"

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║          PHP Firebird - Full Quality Assurance               ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "Container: ${YELLOW}$CONTAINER${NC}"
echo -e "Mode:      ${YELLOW}$MODE${NC}"
if [ "$RUN_VALGRIND" = true ]; then
    echo -e "Valgrind:  ${YELLOW}enabled${NC}"
fi
if [ "$RUN_ASAN" = true ]; then
    echo -e "ASan:      ${YELLOW}enabled${NC}"
fi
echo ""

FAILED=0

# ============================================================================
# MATRIX MODE: Test across oldest and newest PHP/Firebird combinations
# ============================================================================
if [ "$MODE" == "matrix" ]; then
    echo -e "${BLUE}═══ Matrix Testing Mode ═══${NC}"
    echo -e "${YELLOW}Testing across PHP/Firebird version combinations...${NC}"
    echo ""
    
    # Matrix configurations: container:firebird_server:description
    MATRIX_CONFIGS=(
        "php82-fb3-dev:firebird30:PHP 8.2 + Firebird 3.0 (oldest)"
        "php84-fb3-dev:firebird30:PHP 8.4 + Firebird 3.0 (amicron-platform)"
        "php85-fb5-dev:firebird50:PHP 8.5 + Firebird 5.0 (newest)"
    )
    
    MATRIX_FAILED=0
    
    for config in "${MATRIX_CONFIGS[@]}"; do
        IFS=':' read -r container server description <<< "$config"
        
        echo -e "\n${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${BLUE}║  Matrix: $description${NC}"
        echo -e "${BLUE}╠══════════════════════════════════════════════════════════════╣${NC}"
        echo -e "${BLUE}║  Container: $container${NC}"
        echo -e "${BLUE}║  Server:    $server${NC}"
        echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
        
        cd "$DOCKER_DIR"
        
        # Start the required containers
        echo -e "${BLUE}>> Starting containers...${NC}"
        docker compose up -d "$container" "$server"
        
        # Wait for Firebird server to be ready (up to 30 s, TCP port 3050 check)
        echo -e "${BLUE}>> Waiting for $server to be ready...${NC}"
        FB_READY=0
        for i in $(seq 1 30); do
            # Use bash /dev/tcp trick — portable, no nc/isql needed
            if docker compose exec -T "$server" bash -c \
                "timeout 1 bash -c '</dev/tcp/localhost/3050' 2>/dev/null"; then
                FB_READY=1
                break
            fi
            sleep 1
        done
        if [ $FB_READY -eq 0 ]; then
            echo -e "${YELLOW}⚠ $server not reachable after 30 s — continuing anyway${NC}"
        else
            echo -e "${GREEN}✓ $server is ready (port 3050 open)${NC}"
        fi

        # Ensure test.fdb exists on Firebird server (init script only runs on
        # first volume creation; subsequent container restarts skip it).
        echo -e "${BLUE}>> Ensuring test.fdb exists on $server...${NC}"
        docker compose exec -T "$server" bash -c '
            DB_PATH="/firebird/data/test.fdb"
            if [ -f "$DB_PATH" ]; then
                echo "  test.fdb already present"
            else
                echo "  Creating test.fdb..."
                # Try isql-fb first (FB5), fall back to isql (FB3)
                ISQL_CMD="isql-fb"
                command -v isql-fb >/dev/null 2>&1 || ISQL_CMD="isql"
                "$ISQL_CMD" -user SYSDBA -password masterkey <<EOF
CREATE DATABASE '"'"'$DB_PATH'"'"' USER '"'"'SYSDBA'"'"' PASSWORD '"'"'masterkey'"'"' PAGE_SIZE 16384 DEFAULT CHARACTER SET UTF8;
COMMIT;
EXIT;
EOF
                [ -f "$DB_PATH" ] && echo "  ✓ test.fdb created" || echo "  ⚠ test.fdb creation failed (tests may still work via temp databases)"
            fi
        ' 2>/dev/null || true
        
        # Clean up stale test-coverage FDB files left by previous runs.
        # The Firebird service API creates temp databases during restore tests;
        # if a PHP process was killed before cleanup, those files remain open
        # on the Firebird server.  PID recycling then causes "DATABASE IS IN USE"
        # failures on the next matrix run.
        docker compose exec -T "$server" bash -c \
            'rm -f /tmp/test_coverage_*.fdb /tmp/test_*.fdb 2>/dev/null; true' \
            2>/dev/null || true

        # Pre-flight cleanup
        echo -e "${BLUE}>> Pre-flight cleanup...${NC}"
        docker compose exec -T -u root "$container" bash -c "
            cd /ext
            if [ -f Makefile ]; then make clean 2>/dev/null || true; fi
            phpize --clean 2>/dev/null || true
            find . -name '*.dep' -delete 2>/dev/null || true
            find . -name '*.lo' -delete 2>/dev/null || true
            rm -f compile_commands.json 2>/dev/null || true
            rm -rf modules/firebird.so .libs/ .deps/ build/ autom4te.cache/ 2>/dev/null || true
        " 2>/dev/null || true
        
        # Build extension
        echo -e "${BLUE}>> Building extension...${NC}"
        BUILD_EXIT=0
        docker compose exec -T "$container" bash -c "
            cd /ext
            phpize
            # Detect Firebird location (FB3 uses /opt/firebird, others use /usr)
            if [ -d /opt/firebird ]; then
                CPPFLAGS='-I/opt/firebird/include' ./configure --with-firebird=/opt/firebird
            else
                CPPFLAGS='-I/usr/include/firebird' ./configure --with-firebird=/usr
            fi
            make -j\$(nproc)
        " || BUILD_EXIT=$?
        
        if [ $BUILD_EXIT -ne 0 ]; then
            echo -e "${RED}✗ Build failed for $description${NC}"
            MATRIX_FAILED=1
            if [ "$FAIL_FAST" = true ]; then
                echo -e "${RED}✗ --fail-fast: aborting matrix after build failure${NC}"
                exit 1
            fi
            continue
        fi
        echo -e "${GREEN}✓ Build succeeded${NC}"
        
        # Run tests
        echo -e "${BLUE}>> Running tests...${NC}"
        TEST_EXIT=0
        docker compose exec -T "$container" bash -c "
            cd /ext
            /ext/scripts/test.sh
        " || TEST_EXIT=$?
        
        if [ $TEST_EXIT -ne 0 ]; then
            echo -e "${RED}✗ Tests failed for $description${NC}"
            MATRIX_FAILED=1
            if [ "$FAIL_FAST" = true ]; then
                echo -e "${RED}✗ --fail-fast: aborting matrix after test failure${NC}"
                exit 1
            fi
        else
            echo -e "${GREEN}✓ Tests passed for $description${NC}"
        fi
        
        # Run Valgrind memory check
        echo -e "${BLUE}>> Running Valgrind memory analysis...${NC}"
        VALGRIND_EXIT=0
        docker compose exec -T "$container" bash -c "
            cd /ext
            # Rebuild with debug symbols
            make clean
            CFLAGS='-g -O0 -fno-omit-frame-pointer' \
            CXXFLAGS='-g -O0 -fno-omit-frame-pointer' \
            make -j\$(nproc)
            
            # Run quick Valgrind test
            /ext/scripts/analysis/valgrind.sh --quick
        " || VALGRIND_EXIT=$?
        
        if [ $VALGRIND_EXIT -ne 0 ]; then
            echo -e "${YELLOW}⚠ Valgrind reported issues for $description${NC}"
        else
            echo -e "${GREEN}✓ Valgrind clean for $description${NC}"
        fi
        
        # Cleanup
        echo -e "${BLUE}>> Cleanup...${NC}"
        docker compose exec -T -u root "$container" bash -c "
            cd /ext
            make clean 2>/dev/null || true
            phpize --clean 2>/dev/null || true
            rm -rf modules/firebird.so .libs/ .deps/ autom4te.cache/ 2>/dev/null || true
        " 2>/dev/null || true
    done
    
    echo -e "\n${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
    if [ $MATRIX_FAILED -eq 0 ]; then
        echo -e "${GREEN}║           ✓ MATRIX TESTING COMPLETED SUCCESSFULLY            ║${NC}"
    else
        echo -e "${RED}║           ✗ MATRIX TESTING HAD FAILURES                      ║${NC}"
    fi
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
    
    exit $MATRIX_FAILED
fi

# ============================================================================
# DIRECT VALGRIND MODE: Run Valgrind on specified container
# ============================================================================
if [ "$RUN_VALGRIND" = true ] && [ "$MODE" != "full" ] && [ "$MODE" != "security" ]; then
    echo -e "${BLUE}═══ Direct Valgrind Mode ═══${NC}"
    echo -e "${YELLOW}Running Valgrind memory analysis on $CONTAINER...${NC}"
    echo ""
    
    cd "$DOCKER_DIR"
    
    # Start container
    echo -e "${BLUE}>> Starting container $CONTAINER...${NC}"
    docker compose up -d "$CONTAINER"
    
    # Pre-flight cleanup
    echo -e "${BLUE}>> Pre-flight cleanup (root)...${NC}"
    docker compose exec -T -u root "$CONTAINER" bash -c "
        cd /ext
        if [ -f Makefile ]; then make clean 2>/dev/null || true; fi
        phpize --clean 2>/dev/null || true
        find . -name '*.dep' -delete 2>/dev/null || true
        find . -name '*.lo' -delete 2>/dev/null || true
        rm -f compile_commands.json 2>/dev/null || true
        rm -rf modules/firebird.so .libs/ .deps/ build/ autom4te.cache/ 2>/dev/null || true
    " 2>/dev/null || true
    
    # Build with debug symbols
    echo -e "${BLUE}>> Building extension with debug symbols...${NC}"
    docker compose exec -T "$CONTAINER" bash -c "
        cd /ext
        phpize
        # Detect Firebird location
        if [ -d /opt/firebird ]; then
            CPPFLAGS='-I/opt/firebird/include' \
            CFLAGS='-g -O0 -fno-omit-frame-pointer' \
            CXXFLAGS='-g -O0 -fno-omit-frame-pointer' \
            ./configure --with-firebird=/opt/firebird
        else
            CPPFLAGS='-I/usr/include/firebird' \
            CFLAGS='-g -O0 -fno-omit-frame-pointer' \
            CXXFLAGS='-g -O0 -fno-omit-frame-pointer' \
            ./configure --with-firebird=/usr
        fi
        make -j\$(nproc)
    "
    echo -e "${GREEN}✓ Extension built with debug symbols${NC}"
    
    # Run Valgrind
    echo -e "${BLUE}>> Running Valgrind...${NC}"
    VALGRIND_EXIT=0
    docker compose exec -T "$CONTAINER" /ext/scripts/analysis/valgrind.sh --full || VALGRIND_EXIT=$?
    
    # Cleanup
    echo -e "${BLUE}>> Cleanup...${NC}"
    docker compose exec -T -u root "$CONTAINER" bash -c "
        cd /ext
        make clean 2>/dev/null || true
        phpize --clean 2>/dev/null || true
        rm -rf modules/firebird.so .libs/ .deps/ autom4te.cache/ 2>/dev/null || true
    " 2>/dev/null || true
    
    if [ $VALGRIND_EXIT -eq 0 ]; then
        echo -e "${GREEN}✓ Valgrind analysis completed${NC}"
    else
        echo -e "${RED}✗ Valgrind found issues (exit code: $VALGRIND_EXIT)${NC}"
    fi
    
    exit $VALGRIND_EXIT
fi

# ============================================================================
# DIRECT ASAN MODE: Run AddressSanitizer tests
# ============================================================================
if [ "$RUN_ASAN" = true ]; then
    echo -e "${BLUE}═══ Direct ASan Mode ═══${NC}"
    echo -e "${YELLOW}Running AddressSanitizer tests on $CONTAINER...${NC}"
    echo -e "${YELLOW}Note: ASan requires specially built PHP (php83-asan container)${NC}"
    echo ""
    
    cd "$DOCKER_DIR"
    
    # Start ASan container and Firebird
    echo -e "${BLUE}>> Starting containers...${NC}"
    docker compose up -d "$CONTAINER" firebird40
    
    # Wait for Firebird
    echo -e "${BLUE}>> Waiting for Firebird to be ready...${NC}"
    sleep 5
    
    # Pre-flight cleanup (ASan container runs as root)
    echo -e "${BLUE}>> Pre-flight cleanup...${NC}"
    docker compose exec -T "$CONTAINER" bash -c "
        cd /ext
        if [ -f Makefile ]; then make clean 2>/dev/null || true; fi
        phpize --clean 2>/dev/null || true
        rm -rf modules/firebird.so .libs/ .deps/ build/ autom4te.cache/ 2>/dev/null || true
    " 2>/dev/null || true
    
    # Build extension with ASan flags (inherited from container environment)
    echo -e "${BLUE}>> Building extension with ASan...${NC}"
    docker compose exec -T "$CONTAINER" bash -c "
        cd /ext
        phpize
        CPPFLAGS='-I/usr/include/firebird' ./configure --with-firebird=/usr
        make -j\$(nproc)
    "
    echo -e "${GREEN}✓ Extension built with ASan${NC}"
    
    # Run tests under ASan
    echo -e "${BLUE}>> Running tests with ASan...${NC}"
    ASAN_EXIT=0
    docker compose exec -T "$CONTAINER" bash -c "
        cd /ext
        # Export ASan options (also set in Dockerfile but ensure they're active)
        export ASAN_OPTIONS='exitcode=139:abort_on_error=0:detect_leaks=1:halt_on_error=0'
        export USE_ZEND_ALLOC=0
        export ZEND_DONT_UNLOAD_MODULES=1
        
        # Run tests
        /ext/scripts/test.sh
    " || ASAN_EXIT=$?
    
    # Cleanup
    echo -e "${BLUE}>> Cleanup...${NC}"
    docker compose exec -T "$CONTAINER" bash -c "
        cd /ext
        make clean 2>/dev/null || true
        phpize --clean 2>/dev/null || true
        rm -rf modules/firebird.so .libs/ .deps/ autom4te.cache/ 2>/dev/null || true
    " 2>/dev/null || true
    
    if [ $ASAN_EXIT -eq 0 ]; then
        echo -e "${GREEN}✓ ASan tests completed${NC}"
    elif [ $ASAN_EXIT -eq 139 ]; then
        echo -e "${RED}✗ ASan detected memory errors (exit code 139)${NC}"
    else
        echo -e "${RED}✗ ASan tests failed (exit code: $ASAN_EXIT)${NC}"
    fi
    
    exit $ASAN_EXIT
fi

# ============================================================================
# FAST PATH: Fuzz mode - skip all other phases, go directly to fuzzing
# ============================================================================
if [ "$MODE" == "fuzz" ]; then
    echo -e "${BLUE}═══ Fuzz Mode (Direct) ═══${NC}"
    echo -e "${YELLOW}Skipping phases 1-5, running fuzzing directly...${NC}"
    echo ""
    
    # fuzz_asan.sh handles its own build with ASan flags
    FUZZ_EXIT=0
    "$PROJECT_ROOT/scripts/fuzz_asan.sh" 1000 || FUZZ_EXIT=$?
    
    # CLEANUP: ASan container runs as root, so we must clean up build artifacts
    # to prevent permission errors in subsequent operations
    echo -e "${BLUE}Cleaning up ASan build artifacts...${NC}"
    cd "$DOCKER_DIR"
    docker compose exec -T -u root php83-asan bash -c "cd /ext && make clean 2>/dev/null || true && phpize --clean 2>/dev/null || true && rm -rf modules/firebird.so .libs/ .deps/ build/ autom4te.cache/" 2>/dev/null || true
    
    if [ $FUZZ_EXIT -eq 0 ]; then
        echo -e "${GREEN}✓ Fuzzing completed${NC}"
        exit 0
    else
        echo -e "${RED}✗ Fuzzing failed (exit code: $FUZZ_EXIT)${NC}"
        exit $FUZZ_EXIT
    fi
fi

# Helper function to check result and fail fast if requested
check_result() {
    local PHASE="$1"
    local NAME="$2"
    local CMD="$3"
    local OUTPUT_FILE=$(mktemp)
    
    # Run command and capture output while streaming to stdout
    # We use a pipe to tee, so we need PIPESTATUS to get the command's exit code
    if eval "$CMD" 2>&1 | tee "$OUTPUT_FILE"; then
        # Check PIPESTATUS array for the first command's exit code
        if [ ${PIPESTATUS[0]} -eq 0 ]; then
            echo -e "${GREEN}✓ $NAME passed${NC}"
            rm "$OUTPUT_FILE"
            return 0
        fi
    fi
    
    # If we get here, the command failed
    local EXIT_CODE=${PIPESTATUS[0]}
    echo "" # Ensure newline
    echo -e "${RED}✗ $NAME failed${NC}"
    
    if [ "$FAIL_FAST" = true ]; then
        echo ""
        echo -e "${RED}╔═══════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${RED}║ ✗ FAILED: $NAME                                        ║${NC}"
        echo -e "${RED}╠═══════════════════════════════════════════════════════════════╣${NC}"
        echo -e "${RED}║ Phase:   $PHASE                                  ║${NC}"
        echo -e "${RED}║ Command: $CMD                                      ║${NC}"
        echo -e "${RED}║ Exit:    $EXIT_CODE                                          ║${NC}"
        echo -e "${RED}╟───────────────────────────────────────────────────────────────╢${NC}"
        echo -e "${RED}║ Output (captured above):                                      ║${NC}"
        # We don't reprint the whole output since it was just streamed
        echo -e "${RED}║ (See output above for details)                                ║${NC}"
        echo -e "${RED}╚═══════════════════════════════════════════════════════════════╝${NC}"
        rm "$OUTPUT_FILE"
        exit $EXIT_CODE
    else
        FAILED=1
        rm "$OUTPUT_FILE"
        return 1
    fi
}

# ============================================================================
# PHASE 1: Host-side checks (no container needed)
# ============================================================================
echo -e "${BLUE}═══ Phase 1: Host-side Quality Checks ═══${NC}"

# 1.1 Gitleaks (secret detection)
if [[ "$MODE" == "security" ]] || [[ "$MODE" == "full" ]]; then
    echo -e "\n${BLUE}>> [1.1] Gitleaks - Secret Detection...${NC}"
    if command -v gitleaks &> /dev/null; then
        cd "$PROJECT_ROOT"
        # Use config file if available
        GITLEAKS_OPTS=""
        if [ -f ".gitleaks.toml" ]; then
            GITLEAKS_OPTS="--config=.gitleaks.toml"
        fi
        check_result "Phase 1" "Gitleaks" "gitleaks detect --source . --no-git $GITLEAKS_OPTS --no-banner"
    else
        echo -e "${YELLOW}⚠ Gitleaks not installed. Install with: go install github.com/gitleaks/gitleaks/v8@latest${NC}"
    fi
fi

# 1.2 PHP Static Analysis (if composer is available)
echo -e "\n${BLUE}>> [1.2] PHPStan - PHP Static Analysis...${NC}"
cd "$PROJECT_ROOT"
if [ -f composer.json ]; then
    if [ ! -d vendor ]; then
        echo "Installing composer dependencies..."
        if command -v composer &> /dev/null; then
            composer install --dev --quiet 2>/dev/null || true
        else
            echo -e "${YELLOW}⚠ Composer not found on host, will try in container${NC}"
        fi
    fi

    if [ -f vendor/bin/phpstan ]; then
        check_result "Phase 1" "PHPStan" "vendor/bin/phpstan analyse --configuration=phpstan.neon --no-progress"
    else
        echo -e "${YELLOW}⚠ PHPStan not installed, skipping${NC}"
    fi
fi

# 1.3 PHP CodeSniffer
echo -e "\n${BLUE}>> [1.3] PHPCS - PHP Code Style...${NC}"
if [ -f vendor/bin/phpcs ] && [ -d src ]; then
    # Use phpcs.xml if available, otherwise fall back to PSR12
    PHPCS_OPTS=""
    if [ -f phpcs.xml ]; then
        PHPCS_OPTS=""  # phpcs.xml is auto-detected
    else
        PHPCS_OPTS="--standard=PSR12"
    fi
    # PHPCS is usually non-blocking in standard mode, but we'll treat it as a check
    # If fail-fast is on, it will block. If not, it sets FAILED=1.
    check_result "Phase 1" "PHPCS" "vendor/bin/phpcs $PHPCS_OPTS src/ --report=summary"
fi

# Exit early if PHP only
if [ "$PHP_ONLY" = true ]; then
    echo -e "\n${BLUE}═══ PHP-only mode complete ═══${NC}"
    exit $FAILED
fi

# ============================================================================
# PHASE 2: Container setup
# ============================================================================
echo -e "\n${BLUE}═══ Phase 2: Container Environment ═══${NC}"

cd "$DOCKER_DIR"
echo -e "${BLUE}>> Starting container $CONTAINER...${NC}"
docker compose up -d "$CONTAINER"

# Pre-flight Cleanup: Ensure no root-owned artifacts from previous runs exist
echo -e "${BLUE}>> Pre-flight cleanup (root)...${NC}"
docker compose exec -T -u root "$CONTAINER" bash -c "
    cd /ext
    if [ -f Makefile ]; then make clean 2>/dev/null || true; fi
    phpize --clean 2>/dev/null || true
    find . -name '*.dep' -delete 2>/dev/null || true
    find . -name '*.lo' -delete 2>/dev/null || true
    rm -f compile_commands.json 2>/dev/null || true
    rm -rf modules/firebird.so .libs/ .deps/ build/ autom4te.cache/ 2>/dev/null || true
" 2>/dev/null || true

# Install QA tools in container if missing
echo -e "${BLUE}>> Ensuring QA tools in container...${NC}"
docker compose exec -T -u root "$CONTAINER" bash -c "
    export DEBIAN_FRONTEND=noninteractive
    NEED_INSTALL=0
    command -v bear >/dev/null || NEED_INSTALL=1
    command -v clang-tidy >/dev/null || NEED_INSTALL=1
    command -v cppcheck >/dev/null || NEED_INSTALL=1

    if [ \$NEED_INSTALL -eq 1 ]; then
        apt-get update -qq
        apt-get install -y -qq bear clang-tools clang-tidy cppcheck libxml2-utils 2>/dev/null
    fi
" 2>/dev/null || true

# ============================================================================
# PHASE 3: Build C Extension
# ============================================================================
if [ "$SKIP_BUILD" = false ]; then
    echo -e "\n${BLUE}═══ Phase 3: Build Extension with Bear ═══${NC}"
    docker compose exec -T "$CONTAINER" bash -c "
        cd /ext
        if [ -f Makefile ]; then make clean 2>/dev/null || true; phpize --clean 2>/dev/null || true; fi
        phpize
        # Detect Firebird location (FB3 uses /opt/firebird, others use /usr)
        if [ -d /opt/firebird ]; then
            CPPFLAGS='-I/opt/firebird/include' ./configure --with-firebird=/opt/firebird
        else
            CPPFLAGS='-I/usr/include/firebird' ./configure --with-firebird=/usr
        fi
        bear -- make -j\$(nproc)
    "
    echo -e "${GREEN}✓ Extension built successfully${NC}"
else
    echo -e "${YELLOW}⚠ Skipping build (--skip-build)${NC}"
fi

# ============================================================================
# PHASE 4: Static Analysis (C/C++)
# ============================================================================
echo -e "\n${BLUE}═══ Phase 4: C/C++ Static Analysis ═══${NC}"

# 4.1 Clang-Tidy
echo -e "\n${BLUE}>> [4.1] Clang-Tidy...${NC}"
check_result "Phase 4" "Clang-Tidy" "docker compose exec -T \"$CONTAINER\" /ext/scripts/analysis/clang_tidy.sh"

# 4.2 Cppcheck
echo -e "\n${BLUE}>> [4.2] Cppcheck...${NC}"
check_result "Phase 4" "Cppcheck" "docker compose exec -T \"$CONTAINER\" /ext/scripts/analysis/cppcheck.sh"

# Exit if fast mode
if [ "$MODE" == "fast" ]; then
    echo -e "\n${BLUE}═══ Fast mode complete ═══${NC}"
    exit $FAILED
fi

# ============================================================================
# PHASE 5: Unit Tests
# ============================================================================
echo -e "\n${BLUE}═══ Phase 5: Unit Tests ═══${NC}"

check_result "Phase 5" "Unit Tests" "docker compose exec -T \"$CONTAINER\" /ext/scripts/test.sh"

# Exit if standard mode
if [ "$MODE" == "standard" ]; then
    echo -e "\n${BLUE}═══ Standard mode complete ═══${NC}"
    exit $FAILED
fi

# ============================================================================
# PHASE 6: Dynamic Analysis (Full/Security modes)
# ============================================================================
echo -e "\n${BLUE}═══ Phase 6: Dynamic Analysis (Memory Testing) ═══${NC}"
echo -e "${YELLOW}Note: Using Valgrind for memory testing (ASan removed due to PHP compatibility issues)${NC}"

# 6.1 Valgrind Memory Check (Primary memory testing tool)
# Valgrind works properly with PHP extensions unlike ASan which requires
# special PHP builds due to RTLD_DEEPBIND conflicts
echo -e "\n${BLUE}>> [6.1] Valgrind Memory Check...${NC}"

# Ensure clean debug build for accurate Valgrind analysis
# -g: Debug symbols for line numbers
# -O0: No optimization for accurate stack traces
# -fno-omit-frame-pointer: Required for proper stack unwinding
docker compose exec -T "$CONTAINER" bash -c "
    cd /ext
    # Clean any previous builds
    make clean 2>/dev/null || true
    phpize --clean 2>/dev/null || true
    
    # Rebuild with debug symbols for accurate Valgrind output
    phpize
    # Detect Firebird location
    if [ -d /opt/firebird ]; then
        CPPFLAGS='-I/opt/firebird/include' \
        CFLAGS='-g -O0 -fno-omit-frame-pointer' \
        CXXFLAGS='-g -O0 -fno-omit-frame-pointer' \
        ./configure --with-firebird=/opt/firebird
    else
        CFLAGS='-g -O0 -fno-omit-frame-pointer' \
        CXXFLAGS='-g -O0 -fno-omit-frame-pointer' \
        ./configure --with-firebird=/usr
    fi
    make -j\$(nproc)
"

# Run Valgrind with appropriate mode based on QA mode
if [ "$MODE" == "full" ] || [ "$MODE" == "security" ]; then
    # Full mode: comprehensive Valgrind tests
    VALGRIND_MODE="--full"
else
    # Standard fallback (shouldn't reach here normally)
    VALGRIND_MODE="--quick"
fi

check_result "Phase 6" "Valgrind" "docker compose exec -T \"$CONTAINER\" /ext/scripts/analysis/valgrind.sh $VALGRIND_MODE"

# 6.2 UndefinedBehaviorSanitizer (still works with PHP)
# UBSan is more compatible with PHP than ASan
echo -e "\n${BLUE}>> [6.2] UndefinedBehaviorSanitizer...${NC}"

# Check if sanitizers.sh supports ubsan mode
if [ -f "$PROJECT_ROOT/scripts/analysis/sanitizers.sh" ]; then
    check_result "Phase 6" "UndefinedBehaviorSanitizer" "docker compose exec -T \"$CONTAINER\" /ext/scripts/analysis/sanitizers.sh ubsan" || true
else
    echo -e "${YELLOW}⚠ sanitizers.sh not found, skipping UBSan${NC}"
fi

# ============================================================================
# Summary
# ============================================================================
echo -e "\n${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}║               ✓ ALL QUALITY CHECKS PASSED                   ║${NC}"
else
    echo -e "${RED}║               ✗ SOME QUALITY CHECKS FAILED                  ║${NC}"
fi
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"

exit $FAILED