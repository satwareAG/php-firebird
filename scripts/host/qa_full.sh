#!/bin/bash
# scripts/host/qa_full.sh
# Comprehensive local QA workflow with all quality checks
# Usage: ./scripts/host/qa_full.sh [options]
#
# Options:
#   --container NAME   Container to use (default: php83-dev)
#   --mode MODE        fast|standard|full|security (default: standard)
#   --skip-build       Skip C extension build (use existing)
#   --php-only         Only run PHP analysis (PHPStan, PHPCS)
#   --help             Show this help message
#
# Modes:
#   fast     - Static analysis only (clang-tidy, cppcheck, PHPStan)
#   standard - Fast + unit tests
#   full     - Standard + sanitizers (ASan, UBSan) + Valgrind
#   security - Full + Gitleaks secret scanning

set -e

# Defaults
CONTAINER="php83-dev"
MODE="standard"
SKIP_BUILD=false
PHP_ONLY=false

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

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DOCKER_DIR="$PROJECT_ROOT/docker"

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║          PHP Firebird - Full Quality Assurance               ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "Container: ${YELLOW}$CONTAINER${NC}"
echo -e "Mode:      ${YELLOW}$MODE${NC}"
echo ""

FAILED=0

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
        if gitleaks detect --source . --no-git $GITLEAKS_OPTS --no-banner 2>/dev/null; then
            echo -e "${GREEN}✓ No secrets detected${NC}"
        else
            echo -e "${RED}✗ Secrets detected! Review and remove before committing${NC}"
            FAILED=1
        fi
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
        if vendor/bin/phpstan analyse --configuration=phpstan.neon --no-progress; then
            echo -e "${GREEN}✓ PHPStan passed${NC}"
        else
            echo -e "${RED}✗ PHPStan found issues${NC}"
            FAILED=1
        fi
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
    if vendor/bin/phpcs $PHPCS_OPTS src/ --report=summary; then
        echo -e "${GREEN}✓ PHPCS passed${NC}"
    else
        echo -e "${YELLOW}⚠ PHPCS found style issues (non-blocking)${NC}"
    fi
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

# Install QA tools in container if missing
echo -e "${BLUE}>> Ensuring QA tools in container...${NC}"
docker compose exec -u root "$CONTAINER" bash -c "
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
    docker compose exec "$CONTAINER" bash -c "
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
if docker compose exec "$CONTAINER" /ext/scripts/container/analysis/clang_tidy.sh; then
    echo -e "${GREEN}✓ Clang-Tidy passed${NC}"
else
    echo -e "${RED}✗ Clang-Tidy found issues${NC}"
    FAILED=1
fi

# 4.2 Cppcheck
echo -e "\n${BLUE}>> [4.2] Cppcheck...${NC}"
if docker compose exec "$CONTAINER" /ext/scripts/container/analysis/cppcheck.sh; then
    echo -e "${GREEN}✓ Cppcheck passed${NC}"
else
    echo -e "${RED}✗ Cppcheck found issues${NC}"
    FAILED=1
fi

# Exit if fast mode
if [ "$MODE" == "fast" ]; then
    echo -e "\n${BLUE}═══ Fast mode complete ═══${NC}"
    exit $FAILED
fi

# ============================================================================
# PHASE 5: Unit Tests
# ============================================================================
echo -e "\n${BLUE}═══ Phase 5: Unit Tests ═══${NC}"

if docker compose exec "$CONTAINER" /ext/scripts/container/test.sh; then
    echo -e "${GREEN}✓ Unit tests passed${NC}"
else
    echo -e "${RED}✗ Unit tests failed${NC}"
    FAILED=1
fi

# Exit if standard mode
if [ "$MODE" == "standard" ]; then
    echo -e "\n${BLUE}═══ Standard mode complete ═══${NC}"
    exit $FAILED
fi

# ============================================================================
# PHASE 6: Dynamic Analysis (Full/Security modes)
# ============================================================================
echo -e "\n${BLUE}═══ Phase 6: Dynamic Analysis (Sanitizers) ═══${NC}"

# 6.1 AddressSanitizer + UBSan
echo -e "\n${BLUE}>> [6.1] Running Sanitizers (ASan + UBSan)...${NC}"
if docker compose exec "$CONTAINER" /ext/scripts/container/analysis/sanitizers.sh all; then
    echo -e "${GREEN}✓ Sanitizer tests passed${NC}"
else
    echo -e "${RED}✗ Sanitizer tests found issues${NC}"
    FAILED=1
fi

# 6.2 Valgrind (optional, very slow)
echo -e "\n${BLUE}>> [6.2] Valgrind Memory Check...${NC}"
# Rebuild without sanitizers first
docker compose exec "$CONTAINER" bash -c "
    cd /ext
    make clean 2>/dev/null || true
    phpize --clean 2>/dev/null || true
    phpize
    ./configure --with-firebird=/usr
    make -j\$(nproc)
"
if docker compose exec "$CONTAINER" /ext/scripts/container/analysis/valgrind.sh; then
    echo -e "${GREEN}✓ Valgrind passed${NC}"
else
    echo -e "${YELLOW}⚠ Valgrind found issues (review recommended)${NC}"
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
