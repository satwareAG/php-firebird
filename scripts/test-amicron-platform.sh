#!/usr/bin/env bash
#
# Local downstream test: verify php-firebird doesn't break amicron-platform.
#
# This script is the LOCAL equivalent of the doctrine-downstream.yml CI workflow,
# run against a local private checkout of the amicron-platform repository.
# It cannot run in public CI because amicron-platform is private (IPADP L3 privacy).
#
# Prerequisites:
#   - php-firebird + pdo_fbird installed (sudo make install)
#   - AMICRON_PLATFORM_DIR pointing at a local amicron-platform checkout
#   - amicron-platform-firebird3 Docker container running (FB 3.0.14, host port 3053)
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
PLATFORM_DIR="${AMICRON_PLATFORM_DIR:-}"
PLATFORM_BRANCH="${AMICRON_PLATFORM_BRANCH:-3.x-next}"
FIREBIRD_CONTAINER="${FIREBIRD_CONTAINER:-amicron-platform-firebird3}"
DBHOST="${DBHOST:-127.0.0.1}"
DBPORT="${DBPORT:-3053}"
DBNAME="${DBNAME:-/var/lib/firebird/data/database.fdb}"
SKIP_PHPSTAN=false
SKIP_PHPUNIT=false

for arg in "$@"; do
    case "$arg" in
        --skip-phpstan) SKIP_PHPSTAN=true ;;
        --skip-phpunit) SKIP_PHPUNIT=true ;;
        --help|-h)
            sed -n '1,/^# ---/p' "$0"
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

if [[ -z "$PLATFORM_DIR" ]]; then
    fail "AMICRON_PLATFORM_DIR not set"
    echo "  Point it at your local amicron-platform checkout. The path is"
    echo "  environment-specific: this public repo never records internal"
    echo "  filesystem paths (IPADP privacy boundary)."
    exit 2
fi

if [[ ! -d "$PLATFORM_DIR/.git" ]]; then
    fail "amicron-platform not found at $PLATFORM_DIR"
    echo "  Verify AMICRON_PLATFORM_DIR points at the repository root."
    exit 2
fi

if ! php -v 2>/dev/null | grep -E '^PHP 8\.[45]' >/dev/null; then
    fail "PHP 8.4+ required (amicron-platform requires ^8.4)"
    exit 2
fi
pass "PHP version: $(php -v | head -1)"

if ! php -m 2>/dev/null | grep -i 'firebird' >/dev/null; then
    fail "firebird extension not loaded"
    echo "  Run: sudo make install && echo extension=firebird.so | sudo tee /etc/php/conf.d/firebird.ini"
    exit 2
fi
pass "firebird extension loaded"

if ! php -m 2>/dev/null | grep -i 'pdo_fbird' >/dev/null; then
    fail "pdo_fbird extension not loaded"
    exit 2
fi
pass "pdo_fbird extension loaded"

if ! docker ps --format '{{.Names}}' 2>/dev/null | grep "^${FIREBIRD_CONTAINER}$" >/dev/null; then
    fail "Docker container '$FIREBIRD_CONTAINER' not running"
    echo "  Start the Firebird service from your AMICRON_PLATFORM_DIR compose stack:"
    echo "    docker compose up -d firebird"
    exit 2
fi
pass "Firebird container '$FIREBIRD_CONTAINER' running"

# --- checkout platform branch ---
section "amicron-platform ($PLATFORM_BRANCH)"

cd "$PLATFORM_DIR"

CURRENT_BRANCH=$(git branch --show-current)
if [[ "$CURRENT_BRANCH" != "$PLATFORM_BRANCH" ]]; then
    # Guard against clobbering in-flight work in the shared checkout (#332:
    # attribute before repair - a dirty tree means another lane may be live).
    # Only enforced when a switch is actually needed: on an already-current
    # branch a dirty tree is the operator's WIP and is simply tested as-is.
    if [[ -n "$(git status --porcelain)" ]]; then
        fail "amicron-platform worktree is dirty - refusing to switch branches"
        echo "  Commit, stash, or clean $PLATFORM_DIR first (attribute before repair)."
        exit 2
    fi
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
if composer install --no-interaction --prefer-dist --quiet 2>&1 | tail -5; then
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

# Platform consumes DB* env vars via satag/amicron-entity-bundle and
# tests/bootstrap.php: DBHOST/DBPORT/DBNAME/DBUSER/DBPASSWORD. Defaults match
# the amicron-platform compose topology (host-mapped port 3053). php-firebird
# convention (DBPASS) feeds the platform's DBPASSWORD.
if [[ -z "${DBPASS:-}" && -z "${DBPASSWORD:-}" ]]; then
    fail "DBPASS not set - set to amicron test DB password (e.g. export DBPASS=masterkey)"
    exit 2
fi
export DBHOST DBPORT DBNAME
export DBUSER="${DBUSER:-SYSDBA}"
export DBPASSWORD="${DBPASSWORD:-$DBPASS}"

if php bin/console doctrine:schema:validate --env=test 2>&1 | tail -5; then
    pass "doctrine:schema:validate"
else
    fail "doctrine:schema:validate"
fi

# --- doctrine migrations diff ---
section "Doctrine Migrations"

if php bin/console doctrine:migrations:diff --env=test --allow-empty-diff --no-interaction 2>&1 | tail -5; then
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

    # 646: the suite's mailer tests default to the compose-internal DSN
    # (smtp://mailer:1025); 'mailer' is unresolvable from the host and 1025
    # is not host-mapped, so SymfonyMailerTest fails on every host-run gate.
    # Honor an explicit override, else use a null transport (no delivery).
    export MAILER_DSN="${MAILER_DSN:-null://null}"

    if [[ -x "vendor/bin/phpunit" ]]; then
        # 676: tee the full run to a log. On red, print the failure blocks
        # ("There was/were N failure(s)/error(s)/warning(s)") - with --testdox
        # they sit ABOVE the testdox list, so the old `| tail -20` cut exactly
        # the identity off long runs. Log file keeps everything for re-reading.
        phpunit_log="${TMPDIR:-/tmp}/amicron-phpunit-$$.log"
        if vendor/bin/phpunit --colors=always --testdox 2>&1 | tee "$phpunit_log"; then
            pass "PHPUnit"
            rm -f "$phpunit_log"
        else
            fail "PHPUnit - failure summary below (full log: $phpunit_log)"
            if grep -q 'There w' "$phpunit_log"; then
                # failure blocks: "There was/were N failure(s)/error(s)..." to
                # EOF. All limiters consume their full input (sed -n '1,Np'),
                # never head - an early-closing head under pipefail SIGPIPEs
                # the upstream sed and the ERR trap aborts the summary.
                sed -n '/There w/,$p' "$phpunit_log" \
                    | sed -e 's/\x1b\[[0-9;]*m//g' -e 's/\r$//' \
                    | sed -n '1,80p'
            else
                # marker-less shapes (testdox-inline failures, crash): every
                # failure-ish line, then the log tail for context
                { grep -aE 'FAIL|Failed asserting|FAILURES!|ERRORS!' "$phpunit_log" || true; } \
                    | sed -e 's/\x1b\[[0-9;]*m//g' -e 's/\r$//' \
                    | sed -n '1,40p'
                tail -15 "$phpunit_log" | sed -e 's/\x1b\[[0-9;]*m//g'
            fi
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
