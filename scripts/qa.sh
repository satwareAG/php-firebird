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

source "$(dirname "$0")/lib/logging.sh"

# Defaults — php84-fb3-dev is the amicron-platform baseline (PHP 8.4 + Firebird 3)
CONTAINER="php84-fb3-dev"
MODE="standard"
SKIP_BUILD=false
PHP_ONLY=false
FAIL_FAST=false
RUN_VALGRIND=false
RUN_ASAN=false

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

log_info "╔══════════════════════════════════════════════════════════════╗"
log_info "║          PHP Firebird - Full Quality Assurance               ║"
log_info "╚══════════════════════════════════════════════════════════════╝"
echo ""
log_info "Container: $CONTAINER"
log_info "Mode:      $MODE"
if [ "$RUN_VALGRIND" = true ]; then
    log_info "Valgrind:  enabled"
fi
if [ "$RUN_ASAN" = true ]; then
    log_info "ASan:      enabled"
fi
echo ""

FAILED=0

# ============================================================================
# MATRIX MODE: Test across oldest and newest PHP/Firebird combinations
# ============================================================================
if [ "$MODE" == "matrix" ]; then
    log_info "═══ Matrix Testing Mode ═══"
    log_warn "Testing across PHP/Firebird version combinations..."
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
        
        echo ""
        log_info "╔══════════════════════════════════════════════════════════════╗"
        log_info "║  Matrix: $description"
        log_info "╠══════════════════════════════════════════════════════════════╣"
        log_info "║  Container: $container"
        log_info "║  Server:    $server"
        log_info "╚══════════════════════════════════════════════════════════════╝"
        
        cd "$DOCKER_DIR"
        
        # Start the required containers
        log_info ">> Starting containers..."
        docker compose up -d "$container" "$server"

        # Wait for Firebird server to be ready (up to 30 s, TCP port 3050 check)
        log_info ">> Waiting for $server to be ready..."
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
            log_warn "$server not reachable after 30 s — continuing anyway"
        else
            log_pass "$server is ready (port 3050 open)"
        fi

        # Ensure test.fdb exists on Firebird server (init script only runs on
        # first volume creation; subsequent container restarts skip it).
        log_info ">> Ensuring test.fdb exists on $server..."
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
        log_info ">> Pre-flight cleanup..."
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
        log_info ">> Building extension..."
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
            log_fail "Build failed for $description"
            MATRIX_FAILED=1
            if [ "$FAIL_FAST" = true ]; then
                log_fail "--fail-fast: aborting matrix after build failure"
                exit 1
            fi
            continue
        fi
        log_pass "Build succeeded"

        # Run tests
        log_info ">> Running tests..."
        TEST_EXIT=0
        docker compose exec -T "$container" bash -c "
            cd /ext
            /ext/scripts/test.sh
        " || TEST_EXIT=$?

        if [ $TEST_EXIT -ne 0 ]; then
            log_fail "Tests failed for $description"
            MATRIX_FAILED=1
            if [ "$FAIL_FAST" = true ]; then
                log_fail "--fail-fast: aborting matrix after test failure"
                exit 1
            fi
        else
            log_pass "Tests passed for $description"
        fi

        # Run Valgrind memory check
        log_info ">> Running Valgrind memory analysis..."
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
            log_warn "Valgrind reported issues for $description"
        else
            log_pass "Valgrind clean for $description"
        fi
        
        # Cleanup
        log_info ">> Cleanup..."
        docker compose exec -T -u root "$container" bash -c "
            cd /ext
            make clean 2>/dev/null || true
            phpize --clean 2>/dev/null || true
            rm -rf modules/firebird.so .libs/ .deps/ autom4te.cache/ 2>/dev/null || true
        " 2>/dev/null || true
    done
    
    echo ""
    log_info "╔══════════════════════════════════════════════════════════════╗"
    if [ $MATRIX_FAILED -eq 0 ]; then
        log_pass "║           MATRIX TESTING COMPLETED SUCCESSFULLY            ║"
    else
        log_fail "║           MATRIX TESTING HAD FAILURES                      ║"
    fi
    log_info "╚══════════════════════════════════════════════════════════════╝"
    
    exit $MATRIX_FAILED
fi

# ============================================================================
# DIRECT VALGRIND MODE: Run Valgrind on specified container
# ============================================================================
if [ "$RUN_VALGRIND" = true ] && [ "$MODE" != "full" ] && [ "$MODE" != "security" ]; then
    log_info "═══ Direct Valgrind Mode ═══"
    log_warn "Running Valgrind memory analysis on $CONTAINER..."
    echo ""

    cd "$DOCKER_DIR"

    # Start container
    log_info ">> Starting container $CONTAINER..."
    docker compose up -d "$CONTAINER"

    # Pre-flight cleanup
    log_info ">> Pre-flight cleanup (root)..."
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
    log_info ">> Building extension with debug symbols..."
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
    log_pass "Extension built with debug symbols"

    # Run Valgrind
    log_info ">> Running Valgrind..."
    VALGRIND_EXIT=0
    docker compose exec -T "$CONTAINER" /ext/scripts/analysis/valgrind.sh --full || VALGRIND_EXIT=$?

    # Cleanup
    log_info ">> Cleanup..."
    docker compose exec -T -u root "$CONTAINER" bash -c "
        cd /ext
        make clean 2>/dev/null || true
        phpize --clean 2>/dev/null || true
        rm -rf modules/firebird.so .libs/ .deps/ autom4te.cache/ 2>/dev/null || true
    " 2>/dev/null || true

    if [ $VALGRIND_EXIT -eq 0 ]; then
        log_pass "Valgrind analysis completed"
    else
        log_fail "Valgrind found issues (exit code: $VALGRIND_EXIT)"
    fi
    
    exit $VALGRIND_EXIT
fi

# ============================================================================
# DIRECT ASAN MODE: Run AddressSanitizer tests
# ============================================================================
if [ "$RUN_ASAN" = true ]; then
    log_info "═══ Direct ASan Mode ═══"
    log_warn "Running AddressSanitizer tests on $CONTAINER..."
    log_warn "Note: ASan requires specially built PHP (php83-asan container)"
    echo ""

    cd "$DOCKER_DIR"

    # Start ASan container and Firebird
    log_info ">> Starting containers..."
    docker compose up -d "$CONTAINER" firebird40

    # Wait for Firebird
    log_info ">> Waiting for Firebird to be ready..."
    sleep 5

    # Pre-flight cleanup (ASan container runs as root)
    log_info ">> Pre-flight cleanup..."
    docker compose exec -T "$CONTAINER" bash -c "
        cd /ext
        if [ -f Makefile ]; then make clean 2>/dev/null || true; fi
        phpize --clean 2>/dev/null || true
        rm -rf modules/firebird.so .libs/ .deps/ build/ autom4te.cache/ 2>/dev/null || true
    " 2>/dev/null || true

    # Build extension with ASan flags (inherited from container environment)
    log_info ">> Building extension with ASan..."
    docker compose exec -T "$CONTAINER" bash -c "
        cd /ext
        phpize
        CPPFLAGS='-I/usr/include/firebird' ./configure --with-firebird=/usr
        make -j\$(nproc)
    "
    log_pass "Extension built with ASan"

    # Run tests under ASan
    log_info ">> Running tests with ASan..."
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
    log_info ">> Cleanup..."
    docker compose exec -T "$CONTAINER" bash -c "
        cd /ext
        make clean 2>/dev/null || true
        phpize --clean 2>/dev/null || true
        rm -rf modules/firebird.so .libs/ .deps/ autom4te.cache/ 2>/dev/null || true
    " 2>/dev/null || true

    if [ $ASAN_EXIT -eq 0 ]; then
        log_pass "ASan tests completed"
    elif [ $ASAN_EXIT -eq 139 ]; then
        log_fail "ASan detected memory errors (exit code 139)"
    else
        log_fail "ASan tests failed (exit code: $ASAN_EXIT)"
    fi
    
    exit $ASAN_EXIT
fi

# ============================================================================
# FAST PATH: Fuzz mode - skip all other phases, go directly to fuzzing
# ============================================================================
if [ "$MODE" == "fuzz" ]; then
    log_info "═══ Fuzz Mode (Direct) ═══"
    log_warn "Skipping phases 1-5, running fuzzing directly..."
    echo ""

    # fuzz_asan.sh handles its own build with ASan flags
    FUZZ_EXIT=0
    "$PROJECT_ROOT/scripts/fuzz_asan.sh" 1000 || FUZZ_EXIT=$?

    # CLEANUP: ASan container runs as root, so we must clean up build artifacts
    # to prevent permission errors in subsequent operations
    log_info "Cleaning up ASan build artifacts..."
    cd "$DOCKER_DIR"
    docker compose exec -T -u root php83-asan bash -c "cd /ext && make clean 2>/dev/null || true && phpize --clean 2>/dev/null || true && rm -rf modules/firebird.so .libs/ .deps/ build/ autom4te.cache/" 2>/dev/null || true

    if [ $FUZZ_EXIT -eq 0 ]; then
        log_pass "Fuzzing completed"
        exit 0
    else
        log_fail "Fuzzing failed (exit code: $FUZZ_EXIT)"
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
            log_pass "$NAME passed"
            rm "$OUTPUT_FILE"
            return 0
        fi
    fi

    # If we get here, the command failed
    local EXIT_CODE=${PIPESTATUS[0]}
    echo "" # Ensure newline
    log_fail "$NAME failed"

    if [ "$FAIL_FAST" = true ]; then
        echo ""
        log_fail "╔═══════════════════════════════════════════════════════════════╗"
        log_fail "║ ✗ FAILED: $NAME                                        ║"
        log_fail "╠═══════════════════════════════════════════════════════════════╣"
        log_fail "║ Phase:   $PHASE                                  ║"
        log_fail "║ Command: $CMD                                      ║"
        log_fail "║ Exit:    $EXIT_CODE                                          ║"
        log_fail "╟───────────────────────────────────────────────────────────────╢"
        log_fail "║ Output (captured above):                                      ║"
        # We don't reprint the whole output since it was just streamed
        log_fail "║ (See output above for details)                                ║"
        log_fail "╚═══════════════════════════════════════════════════════════════╝"
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
log_info "═══ Phase 1: Host-side Quality Checks ═══"

# 1.1 Gitleaks (secret detection)
if [[ "$MODE" == "security" ]] || [[ "$MODE" == "full" ]]; then
    echo ""
    log_info ">> [1.1] Gitleaks - Secret Detection..."
    if command -v gitleaks &> /dev/null; then
        cd "$PROJECT_ROOT"
        # Use config file if available
        GITLEAKS_OPTS=""
        if [ -f ".gitleaks.toml" ]; then
            GITLEAKS_OPTS="--config=.gitleaks.toml"
        fi
        check_result "Phase 1" "Gitleaks" "gitleaks detect --source . --no-git $GITLEAKS_OPTS --no-banner"
    else
        log_warn "Gitleaks not installed. Install with: go install github.com/gitleaks/gitleaks/v8@latest"
    fi
fi

# 1.2 PHP Static Analysis (if composer is available)
echo ""
log_info ">> [1.2] PHPStan - PHP Static Analysis..."
cd "$PROJECT_ROOT"
if [ -f composer.json ]; then
    if [ ! -d vendor ]; then
        echo "Installing composer dependencies..."
        if command -v composer &> /dev/null; then
            composer install --dev --quiet 2>/dev/null || true
        else
            log_warn "Composer not found on host, will try in container"
        fi
    fi

    if [ -f vendor/bin/phpstan ]; then
        check_result "Phase 1" "PHPStan" "vendor/bin/phpstan analyse --configuration=phpstan.neon --no-progress"
    else
        log_warn "PHPStan not installed, skipping"
    fi
fi

# 1.3 PHP CodeSniffer
echo ""
log_info ">> [1.3] PHPCS - PHP Code Style..."
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
    echo ""
    log_info "═══ PHP-only mode complete ═══"
    exit $FAILED
fi

# ============================================================================
# PHASE 2: Container setup
# ============================================================================
echo ""
log_info "═══ Phase 2: Container Environment ═══"

cd "$DOCKER_DIR"
log_info ">> Starting container $CONTAINER..."
docker compose up -d "$CONTAINER"

# Pre-flight Cleanup: Ensure no root-owned artifacts from previous runs exist
log_info ">> Pre-flight cleanup (root)..."
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
log_info ">> Ensuring QA tools in container..."
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
    echo ""
    log_info "═══ Phase 3: Build Extension with Bear ═══"
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
    log_pass "Extension built successfully"
else
    log_warn "Skipping build (--skip-build)"
fi

# ============================================================================
# PHASE 4: Static Analysis (C/C++)
# ============================================================================
echo ""
log_info "═══ Phase 4: C/C++ Static Analysis ═══"

# 4.1 Clang-Tidy
echo ""
log_info ">> [4.1] Clang-Tidy..."
check_result "Phase 4" "Clang-Tidy" "docker compose exec -T \"$CONTAINER\" /ext/scripts/analysis/clang_tidy.sh"

# 4.2 Cppcheck
echo ""
log_info ">> [4.2] Cppcheck..."
check_result "Phase 4" "Cppcheck" "docker compose exec -T \"$CONTAINER\" /ext/scripts/analysis/cppcheck.sh"

# Exit if fast mode
if [ "$MODE" == "fast" ]; then
    echo ""
    log_info "═══ Fast mode complete ═══"
    exit $FAILED
fi

# ============================================================================
# PHASE 5: Unit Tests
# ============================================================================
echo ""
log_info "═══ Phase 5: Unit Tests ═══"

check_result "Phase 5" "Unit Tests" "docker compose exec -T \"$CONTAINER\" /ext/scripts/test.sh"

# Exit if standard mode
if [ "$MODE" == "standard" ]; then
    echo ""
    log_info "═══ Standard mode complete ═══"
    exit $FAILED
fi

# ============================================================================
# PHASE 6: Dynamic Analysis (Full/Security modes)
# ============================================================================
echo ""
log_info "═══ Phase 6: Dynamic Analysis (Memory Testing) ═══"
log_warn "Note: Using Valgrind for memory testing (ASan removed due to PHP compatibility issues)"

# 6.1 Valgrind Memory Check (Primary memory testing tool)
# Valgrind works properly with PHP extensions unlike ASan which requires
# special PHP builds due to RTLD_DEEPBIND conflicts
echo ""
log_info ">> [6.1] Valgrind Memory Check..."

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
echo ""
log_info ">> [6.2] UndefinedBehaviorSanitizer..."

# Check if sanitizers.sh supports ubsan mode
if [ -f "$PROJECT_ROOT/scripts/analysis/sanitizers.sh" ]; then
    check_result "Phase 6" "UndefinedBehaviorSanitizer" "docker compose exec -T \"$CONTAINER\" /ext/scripts/analysis/sanitizers.sh ubsan" || true
else
    log_warn "sanitizers.sh not found, skipping UBSan"
fi

# ============================================================================
# Summary
# ============================================================================
echo ""
log_info "╔══════════════════════════════════════════════════════════════╗"
if [ $FAILED -eq 0 ]; then
    log_pass "║               ALL QUALITY CHECKS PASSED                   ║"
else
    log_fail "║               SOME QUALITY CHECKS FAILED                  ║"
fi
log_info "╚══════════════════════════════════════════════════════════════╝"

exit $FAILED