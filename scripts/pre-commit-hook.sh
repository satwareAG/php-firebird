#!/bin/bash
# scripts/pre-commit-hook.sh
# Git pre-commit hook for quality checks
#
# Installation:
#   ln -sf ../../scripts/pre-commit-hook.sh .git/hooks/pre-commit
#   chmod +x .git/hooks/pre-commit
#
# Or copy to .git/hooks/pre-commit

set -e

source "$(dirname "$0")/lib/logging.sh"

log_warn "Running pre-commit quality checks..."

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
        log_pass "OK"
    else
        log_fail "FAILED"
        log_error "✗ Secrets detected in staged files! Remove before committing."
        FAILED=1
    fi
else
    log_warn "SKIP (gitleaks not installed)"
fi

# ============================================================================
# 2. PHPStan - PHP Static Analysis (if PHP files changed)
# ============================================================================
# Exclude stub files (PHPStan metadata, not code to analyze)
PHP_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.php$' | grep -v '\.stub\.php$' || true)
if [ -n "$PHP_FILES" ]; then
    echo -n "Checking PHP files (PHPStan)... "
    if [ -f vendor/bin/phpstan ]; then
        if echo "$PHP_FILES" | xargs vendor/bin/phpstan analyse --configuration=phpstan.neon --no-progress 2>/dev/null; then
            log_pass "OK"
        else
            log_fail "FAILED"
            FAILED=1
        fi
    else
        log_warn "SKIP (PHPStan not installed)"
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
            log_pass "OK"
        else
            log_fail "FAILED"
            log_error "✗ Run './scripts/qa.sh --mode fast' for details"
            FAILED=1
        fi
    else
        log_warn "SKIP (cppcheck not found on host)"
    fi
fi

# ============================================================================
# 4. Check for debug/test artifacts
# ============================================================================
echo -n "Checking for debug artifacts... "
STAGED_FILES=$(git diff --cached --name-only)

# Check for console.log, var_dump, print_r, etc.
# Use \b for word boundaries to avoid false positives like _add(
DEBUG_PATTERNS='\bvar_dump\(|\bprint_r\(|console\.log\(|error_log.*DEBUG|\bdd\(|\bdump\('
if echo "$STAGED_FILES" | xargs -r grep -l -E "$DEBUG_PATTERNS" 2>/dev/null | head -5; then
    log_warn "WARNING: Debug statements found (review before release)"
else
    log_pass "OK"
fi

# ============================================================================
# Summary
# ============================================================================
if [ $FAILED -ne 0 ]; then
    echo ""
    log_error "╔══════════════════════════════════════════════════════════════╗"
    log_error "║  Pre-commit checks FAILED. Fix issues before committing.     ║"
    log_error "║  To bypass: git commit --no-verify                           ║"
    log_error "╚══════════════════════════════════════════════════════════════╝"
    exit 1
fi

log_pass "✓ All pre-commit checks passed"
exit 0
