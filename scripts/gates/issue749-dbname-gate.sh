#!/usr/bin/env bash
# =============================================================================
# scripts/gates/issue749-dbname-gate.sh - wire-surface oracle for #749
# =============================================================================
# Asserts the amicron-platform downstream gate defaults (#750 lane L1.G1/G2):
#   1. DBNAME default = dedicated test DB (/var/lib/firebird/data/test.fdb),
#      never the live dev database.fdb that the platform worker holds 24/7.
#   2. FIREBIRD_CONTAINER is forwarded on the DB* export line so the platform
#      bootstrap's container-side gbak restore path resolves the right stack.
#
# Success-only marker: prints "issue749 gate: PASS" and exits 0 ONLY when all
# assertions pass; exits 1 with a FAIL line otherwise.
#
# Usage:
#   scripts/gates/issue749-dbname-gate.sh              # both checks
#   scripts/gates/issue749-dbname-gate.sh --dbname     # G1 only
#   scripts/gates/issue749-dbname-gate.sh --container  # G2 only
#   scripts/gates/issue749-dbname-gate.sh --file <path>  # assert an arbitrary
#       blob instead of the live script (negative-control hook: the pre-fix
#       version must FAIL the same assertions).
# =============================================================================
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TARGET="$REPO_ROOT/scripts/test-amicron-platform.sh"

MODE=""
while [ $# -gt 0 ]; do
    case "$1" in
        --dbname) MODE="dbname" ;;
        --container) MODE="container" ;;
        --file)
            TARGET="${2:-}"
            [ -f "$TARGET" ] || { echo "issue749 gate: FAIL - --file target not found: $TARGET"; exit 1; }
            shift
            ;;
        *)
            echo "issue749 gate: FAIL - unknown argument: $1"
            exit 1
            ;;
    esac
    shift
done

fail() {
    echo "issue749 gate: FAIL - $1"
    exit 1
}

check_dbname() {
    grep -F 'DBNAME="${DBNAME:-/var/lib/firebird/data/test.fdb}"' "$TARGET" >/dev/null \
        || fail "DBNAME default is not the dedicated test DB (/var/lib/firebird/data/test.fdb)"
    if grep -F 'data/database.fdb' "$TARGET" >/dev/null; then
        fail "DBNAME default still references the live dev DB (data/database.fdb)"
    fi
}

check_container() {
    grep -Eq '^export DBHOST DBPORT DBNAME FIREBIRD_CONTAINER$' "$TARGET" \
        || fail "FIREBIRD_CONTAINER not forwarded on the DB* export line"
}

case "$MODE" in
    dbname) check_dbname ;;
    container) check_container ;;
    "") check_dbname; check_container ;;
esac

echo "issue749 gate: PASS"
