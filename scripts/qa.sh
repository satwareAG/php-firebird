#!/bin/bash
# scripts/qa.sh
# Comprehensive local QA workflow with all quality checks
# Usage: ./scripts/qa.sh [options]
#
# Options:
#   --container NAME   Container to use (default: php83-dev)
#   --mode MODE        fast|standard|full|security|fuzz (default: standard)
#   --skip-build       Skip C extension build (use existing)
#   --php-only         Only run PHP analysis (PHPStan, PHPCS)
#   --fail-fast        Stop at first error with detailed output
#   --help             Show this help message
#
# Modes:
#   fast     - Static analysis only (clang-tidy, cppcheck, PHPStan)
#   standard - Fast + unit tests
#   full     - Standard + sanitizers (ASan, UBSan) + Valgrind
#   security - Full + Gitleaks secret scanning
#   fuzz     - Run fuzzing with ASan

set -e

# Defaults
CONTAINER="php83-dev"
MODE="standard"
SKIP_BUILD=false
PHP_ONLY=false
FAIL_FAST=false

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
            head -20 "$0" | tail -16
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
echo ""

FAILED=0

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
        CPPFLAGS='-I/usr/include/firebird' ./configure --with-firebird=/usr
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
# PHASE 6: Dynamic Analysis (Full/Security/Fuzz modes)
# ============================================================================
echo -e "\n${BLUE}═══ Phase 6: Dynamic Analysis (Sanitizers) ═══${NC}"

# 6.0 Fuzzing (Fuzz mode only)
if [ "$MODE" == "fuzz" ]; then
    echo -e "\n${BLUE}>> [6.0] Fuzzing (ASan)...${NC}"
    check_result "Phase 6" "Fuzzing" "$PROJECT_ROOT/scripts/fuzz_asan.sh 1000"
    exit $FAILED
fi

# 6.0 Fuzzing (Full mode)
if [ "$MODE" == "full" ]; then
    echo -e "\n${BLUE}>> [6.0] Fuzzing (ASan)...${NC}"
    # Run fewer iterations in full mode to keep total runtime reasonable
    check_result "Phase 6" "Fuzzing" "$PROJECT_ROOT/scripts/fuzz_asan.sh 500"
fi

# 6.1 AddressSanitizer + UBSan
echo -e "\n${BLUE}>> [6.1] Running Sanitizers (ASan + UBSan)...${NC}"
# Use the dedicated ASan container for ASan tests if available
if docker compose ps --services | grep -q "php83-asan"; then
    echo -e "${BLUE}   Using dedicated ASan container (php83-asan)...${NC}"
    # Ensure it's running
    docker compose up -d php83-asan
    check_result "Phase 6" "AddressSanitizer" "docker compose exec -T php83-asan /ext/scripts/analysis/sanitizers.sh asan"
    
    # CLEANUP: ASan container runs as root, so we must clean up build artifacts as root
    # to prevent permission errors in subsequent steps (like UBSan running as user)
    echo -e "${BLUE}   Cleaning up ASan build artifacts (root)...${NC}"
    docker compose exec -T -u root php83-asan bash -c "cd /ext && make clean 2>/dev/null || true && phpize --clean 2>/dev/null || true && rm -rf modules/firebird.so .libs/ .deps/ build/ autom4te.cache/"
else
    echo -e "${YELLOW}⚠ Dedicated ASan container not found, skipping ASan (requires custom build)${NC}"
fi

# Run UBSan in the standard container (it usually works fine with LD_PRELOAD or standard build)
check_result "Phase 6" "UndefinedBehaviorSanitizer" "docker compose exec -T \"$CONTAINER\" /ext/scripts/analysis/sanitizers.sh ubsan"

# 6.2 Valgrind (optional, very slow)
echo -e "\n${BLUE}>> [6.2] Valgrind Memory Check...${NC}"
# Rebuild without sanitizers first
docker compose exec -T "$CONTAINER" bash -c "
    cd /ext
    make clean 2>/dev/null || true
    phpize --clean 2>/dev/null || true
    phpize
    ./configure --with-firebird=/usr
    make -j\$(nproc)
"
# Valgrind is often treated as non-blocking warning, but with fail-fast we might want to stop
if [ "$FAIL_FAST" = true ]; then
    check_result "Phase 6" "Valgrind" "docker compose exec -T \"$CONTAINER\" /ext/scripts/analysis/valgrind.sh"
else
    if docker compose exec -T "$CONTAINER" /ext/scripts/analysis/valgrind.sh; then
        echo -e "${GREEN}✓ Valgrind passed${NC}"
    else
        echo -e "${YELLOW}⚠ Valgrind found issues (review recommended)${NC}"
    fi
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
