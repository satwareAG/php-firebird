#!/bin/bash
# scripts/host/pre-commit-hook.sh
# Git pre-commit hook for quality checks
#
# Installation:
#   ln -sf ../../scripts/host/pre-commit-hook.sh .git/hooks/pre-commit
#   chmod +x .git/hooks/pre-commit
#
# Or copy to .git/hooks/pre-commit

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}Running pre-commit quality checks...${NC}"

# Get project root
PROJECT_ROOT="$(git rev-parse --show-toplevel)"
cd "$PROJECT_ROOT"

FAILED=0

# ============================================================================
# 1. Gitleaks - Secret Detection (CRITICAL)
# ============================================================================
echo -n "Checking for secrets (gitleaks)... "
if command -v gitleaks &> /dev/null; then
    # Build gitleaks options (use config if available)
    GITLEAKS_OPTS=""
    if [ -f "$PROJECT_ROOT/.gitleaks.toml" ]; then
        GITLEAKS_OPTS="--config=$PROJECT_ROOT/.gitleaks.toml"
    fi
    # Check staged changes only
    if gitleaks protect --staged $GITLEAKS_OPTS --no-banner 2>/dev/null; then
        echo -e "${GREEN}OK${NC}"
    else
        echo -e "${RED}FAILED${NC}"
        echo -e "${RED}✗ Secrets detected in staged files! Remove before committing.${NC}"
        FAILED=1
    fi
else
    echo -e "${YELLOW}SKIP (gitleaks not installed)${NC}"
fi

# ============================================================================
# 2. PHPStan - PHP Static Analysis (if PHP files changed)
# ============================================================================
PHP_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.php$' || true)
if [ -n "$PHP_FILES" ]; then
    echo -n "Checking PHP files (PHPStan)... "
    if [ -f vendor/bin/phpstan ]; then
        if echo "$PHP_FILES" | xargs vendor/bin/phpstan analyse --configuration=phpstan.neon --no-progress 2>/dev/null; then
            echo -e "${GREEN}OK${NC}"
        else
            echo -e "${RED}FAILED${NC}"
            FAILED=1
        fi
    else
        echo -e "${YELLOW}SKIP (PHPStan not installed)${NC}"
    fi
fi

# ============================================================================
# 3. C/C++ Static Analysis (if C files changed - quick check only)
# ============================================================================
C_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(c|cpp|h)$' || true)
if [ -n "$C_FILES" ]; then
    echo -n "Checking C/C++ files (cppcheck quick)... "
    if command -v cppcheck &> /dev/null; then
        # Quick check - errors only, no inconclusive
        if echo "$C_FILES" | xargs cppcheck --error-exitcode=1 --quiet 2>/dev/null; then
            echo -e "${GREEN}OK${NC}"
        else
            echo -e "${RED}FAILED${NC}"
            echo -e "${RED}✗ Run './scripts/host/qa_full.sh --mode fast' for details${NC}"
            FAILED=1
        fi
    else
        echo -e "${YELLOW}SKIP (cppcheck not found on host)${NC}"
    fi
fi

# ============================================================================
# 4. Check for debug/test artifacts
# ============================================================================
echo -n "Checking for debug artifacts... "
STAGED_FILES=$(git diff --cached --name-only)

# Check for console.log, var_dump, print_r, etc.
DEBUG_PATTERNS='var_dump|print_r|console\.log|error_log.*DEBUG|dd\(|dump\('
if echo "$STAGED_FILES" | xargs -r grep -l -E "$DEBUG_PATTERNS" 2>/dev/null | head -5; then
    echo -e "${YELLOW}WARNING: Debug statements found (review before release)${NC}"
else
    echo -e "${GREEN}OK${NC}"
fi

# ============================================================================
# Summary
# ============================================================================
if [ $FAILED -ne 0 ]; then
    echo ""
    echo -e "${RED}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║  Pre-commit checks FAILED. Fix issues before committing.     ║${NC}"
    echo -e "${RED}║  To bypass: git commit --no-verify                           ║${NC}"
    echo -e "${RED}╚══════════════════════════════════════════════════════════════╝${NC}"
    exit 1
fi

echo -e "${GREEN}✓ All pre-commit checks passed${NC}"
exit 0
