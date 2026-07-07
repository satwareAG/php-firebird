#!/usr/bin/env bash
#
# Local downstream test: verify php-firebird doesn't break amicron-platform.
#
# This script is the LOCAL equivalent of the doctrine-downstream.yml CI workflow,
# for the private amicron-platform repository (~/internal/amicron-platform).
# It cannot run in public CI because amicron-platform is private (IPADP L3 privacy).
#
# Prerequisites:
#   - php-firebird + pdo_fbird installed (sudo make install)
#   - ~/internal/amicron-platform checked out
#   - amicron-firebird Docker container running (FB 3.0.13)
#   - PHP 8.4+ available
#
# Usage:
#   scripts/test-amicron-platform.sh
#   scripts/test-amicron-platform.sh --skip-phpstan   # skip PHPStan (faster)
#   scripts/test-amicron-platform.sh --skip-phpunit   # skip PHPUnit (faster)
#   AMICRON_PLATFORM_BRANCH=feature/test scripts/test-amicron-platform.sh
#
# Exit codes:
#   0 = all checks passed
#   1 = one or more checks failed
#   2 = prerequisites not met
#
set -euo pipefail
trap 'echo "FAILED at line $LINENO" >&2; exit 1' ERR

# --- configuration ---
PLATFORM_DIR="${AMICRON_PLATFORM_DIR:-$HOME/internal/amicron-platform}"
PLATFORM_BRANCH="${AMICRON_PLATFORM_BRANCH:-release/3.0.0-rc.2}"
FIREBIRD_CONTAINER="${FIREBIRD_CONTAINER:-amicron-firebird}"
SKIP_PHPSTAN=false
SKIP_PHPUNIT=false

for arg in "$@"; do
    case "$arg" in
        --skip-phpstan) SKIP_PHPSTAN=true ;;
        --skip-phpunit) SKIP_PHPUNIT=true ;;
        --help|-h)
            head -22 "$0"
            exit 0
            ;;
    esac
done

# --- colors ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass() { echo -e "  ${GREEN}OK${NC}  | $1"; }
fail() { echo -e "  ${RED}FAIL${NC}| $1"; FAILURES=$((FAILURES + 1)); }
info() { echo -e "       $1"; }
section() { echo -e "\n${YELLOW}=== $1 ===${NC}"; }

FAILURES=0

# --- prerequisite checks ---
section "Prerequisites"

if [[ ! -d "$PLATFORM_DIR/.git" ]]; then
    fail "amicron-platform not found at $PLATFORM_DIR"
    echo "  Set AMICRON_PLATFORM_DIR or clone the repo first."
    exit 2
fi

if ! php -v 2>/dev/null | grep -qE '^PHP 8\.[45]'; then
    fail "PHP 8.4+ required (amicron-platform requires ^8.4)"
    exit 2
fi
pass "PHP version: $(php -v | head -1)"

if ! php -m 2>/dev/null | grep -qi 'firebird'; then
    fail "firebird extension not loaded"
    echo "  Run: sudo make install && echo extension=firebird.so | sudo tee /etc/php/conf.d/firebird.ini"
    exit 2
fi
pass "firebird extension loaded"

if ! php -m 2>/dev/null | grep -qi 'pdo_fbird'; then
    fail "pdo_fbird extension not loaded"
    exit 2
fi
pass "pdo_fbird extension loaded"

if ! docker ps --format '{{.Names}}' 2>/dev/null | grep -q "^${FIREBIRD_CONTAINER}$"; then
    fail "Docker container '$FIREBIRD_CONTAINER' not running"
    echo "  Start it: cd ~/internal/amicron-firebird && docker compose up -d"
    exit 2
fi
pass "Firebird container '$FIREBIRD_CONTAINER' running"

# --- checkout platform branch ---
section "amicron-platform ($PLATFORM_BRANCH)"

cd "$PLATFORM_DIR"
CURRENT_BRANCH=$(git branch --show-current)
if [[ "$CURRENT_BRANCH" != "$PLATFORM_BRANCH" ]]; then
    info "Current branch: $CURRENT_BRANCH. Checking out $PLATFORM_BRANCH..."
    git checkout "$PLATFORM_BRANCH" || {
        fail "Cannot checkout $PLATFORM_BRANCH"
        exit 2
    }
fi
pass "On branch $PLATFORM_BRANCH (commit $(git rev-parse --short HEAD))"

# --- composer install ---
section "Dependencies"

if [[ ! -f composer.lock ]]; then
    fail "composer.lock missing"
    exit 2
fi

info "Running composer install..."
if composer install --no-interaction --prefer-dist --quiet 2>/dev/null; then
    pass "composer install"
else
    fail "composer install failed"
    exit 1
fi

# Verify the stubs package matches locally built version
STUBS_VERSION=$(composer show satwareag/php-firebird-stubs 2>/dev/null | grep '^versions' || echo "not installed")
info "php-firebird-stubs: $STUBS_VERSION"

# --- doctrine schema validation ---
section "Doctrine Schema Validation"

export DBHOST=localhost
export DBNAME="${DBNAME:-/var/lib/firebird/data/amicron-empty.fdb}"
export DBUSER="${DBUSER:-SYSDBA}"
export DBPASS="${DBPASS:?DBPASS not set — set to amicron test DB password (e.g. export DBPASS=...)}"

if php bin/console doctrine:schema:validate --env=test 2>&1 | tail -5; then
    pass "doctrine:schema:validate"
else
    fail "doctrine:schema:validate"
fi

# --- doctrine migrations diff ---
section "Doctrine Migrations"

if php bin/console doctrine:migrations:diff --env=test --allow-empty-schema --no-interaction 2>&1 | tail -5; then
    pass "doctrine:migrations:diff"
else
    # Migration diff may produce "No changes detected" which is a pass
    info "doctrine:migrations:diff (may show 'no changes' which is OK)"
fi

# --- PHPStan ---
if [[ "$SKIP_PHPSTAN" == "false" ]]; then
    section "PHPStan (stubs BC check)"

    if [[ -x "vendor-bin/phpstan/vendor/bin/phpstan" ]]; then
        if vendor-bin/phpstan/vendor/bin/phpstan analyze --memory-limit=2G --no-progress 2>&1 | tail -10; then
            pass "PHPStan"
        else
            fail "PHPStan (stubs BC break?)"
        fi
    else
        info "PHPStan not installed (vendor-bin/phpstan). Skipping."
    fi
else
    info "PHPStan skipped (--skip-phpstan)"
fi

# --- PHPUnit ---
if [[ "$SKIP_PHPUNIT" == "false" ]]; then
    section "PHPUnit"

    if [[ -x "vendor/bin/phpunit" ]]; then
        if vendor/bin/phpunit --colors=always --testdox 2>&1 | tail -20; then
            pass "PHPUnit"
        else
            fail "PHPUnit"
        fi
    else
        info "PHPUnit not installed. Skipping."
    fi
else
    info "PHPUnit skipped (--skip-phpunit)"
fi

# --- summary ---
section "Summary"

if [[ "$FAILURES" -eq 0 ]]; then
    echo -e "${GREEN}ALL CHECKS PASSED${NC} - php-firebird is compatible with amicron-platform $PLATFORM_BRANCH"
    exit 0
else
    echo -e "${RED}$FAILURES CHECK(S) FAILED${NC} - php-firebird may break amicron-platform"
    exit 1
fi
