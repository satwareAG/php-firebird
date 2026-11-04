#!/usr/bin/env bash
# =============================================================================
# scripts/gates/issue718-artifact-retention-gate.sh - oracle for #718 (L2.G1)
# =============================================================================
# Asserts the matrix runner's cleanup contract:
#   phase 1 (archive mode, used between lanes + final cleanup): failure
#     evidence (*.diff/.out/.exp/.log/.mem + top-level generated *.sh/*.php)
#     is RETAINED under artifacts/test-failures/<run>/ and the tree is clean
#     for the next lane.
#   phase 2 (control, plain mode = stale-at-start): artifacts are actually
#     deleted and NOT silently archived.
#
# Success-only marker: prints "issue718 retention gate: PASS" and exits 0
# ONLY when both phases pass; exits 1 with a FAIL line otherwise.
#
# Pre-fix RED shape: clean_test_artifacts ignores "--archive", deletes the
# seeded evidence outright -> phase 1 fails with "not retained".
# =============================================================================
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RET_BASE="$REPO_ROOT/artifacts/test-failures"

fail() {
    echo "issue718 retention gate: FAIL - $1"
    exit 1
}

# shellcheck source=../lib/clean-artifacts.sh
source "$REPO_ROOT/scripts/lib/clean-artifacts.sh"

FILES="fake_failure_001.diff fake_failure_001.out fake_failure_001.exp fake_failure_001.log fake_failure_001.mem fake_failure_001.sh fake_failure_001.php"

seed() {
    mkdir -p "$REPO_ROOT/tests"
    for f in $FILES; do
        : > "$REPO_ROOT/tests/$f"
    done
}

retained_count() {
    find "$RET_BASE" -name "$1" 2>/dev/null | wc -l
}

echo "== phase 1: final/between-lane cleanup (archive mode) retains evidence =="
BEFORE=$(find "$RET_BASE" -type f 2>/dev/null | wc -l)
seed
clean_test_artifacts "$REPO_ROOT" --archive
for f in $FILES; do
    [ "$(retained_count "$f")" -ge 1 ] \
        || fail "$f not retained under artifacts/test-failures/ (archive mode deleted it outright or ignored the flag)"
    [ ! -e "$REPO_ROOT/tests/$f" ] \
        || fail "$f still present in tests/ after archive mode (tree not cleaned for next lane)"
done
AFTER=$(find "$RET_BASE" -type f 2>/dev/null | wc -l)
[ "$AFTER" -gt "$BEFORE" ] || fail "retention dir did not grow (archive mode archived nothing)"
echo "   archived $((AFTER - BEFORE)) artifact(s)"

echo "== phase 2 (control): stale-at-start cleanup (plain mode) deletes, does not archive =="
BEFORE=$(find "$RET_BASE" -type f 2>/dev/null | wc -l)
seed
clean_test_artifacts "$REPO_ROOT"
for f in $FILES; do
    [ ! -e "$REPO_ROOT/tests/$f" ] \
        || fail "plain cleanup no longer deletes stale artifacts ($f survived)"
done
AFTER=$(find "$RET_BASE" -type f 2>/dev/null | wc -l)
[ "$AFTER" -eq "$BEFORE" ] \
    || fail "plain mode grew the retention dir ($BEFORE -> $AFTER): it archived instead of deleting"

echo "issue718 retention gate: PASS"
