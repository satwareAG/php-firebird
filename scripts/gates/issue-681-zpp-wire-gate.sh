#!/bin/bash
# Issue #681 gate: zpp default/forwarding changes require wire-observation tests.
#
# Fails when a diff changes zpp argument parsing, arginfo, DPB/SPB/TPB buffer
# building, OR the connect-path forwarding files WITHOUT a corresponding
# tests/*.phpt change in the same diff. Parsed-but-unforwarded arguments and
# hardcoded DPB literals (#595, #671 family) are invisible to return-value
# tests; this gate forces the wire-observation test to ride the same PR.
#
# Detection layers (council block-2 review, seats S2+S5):
#   1. token grep on diff lines: zend_parse_parameters / arginfo macros /
#      isc_dpb_ / isc_spb_ / isc_tpb_ items
#   2. forwarding-locus grep: the #671 regression line itself
#      (`zend_long largs[] = { buffers, dialect, sync };`) carries NONE of
#      the tokens above - so `largs[]` / `args[SYNC]` patterns are flagged
#      directly
#   3. connect-path files: ANY change to the DPB-assembly source files
#      counts as wire-surface, closing the rename/reformat evasion class
#
# Usage:
#   scripts/gates/issue-681-zpp-wire-gate.sh [git-diff-spec]   (default origin/main...HEAD)
#   scripts/gates/issue-681-zpp-wire-gate.sh --diff-dir DIR    (worktree vs its HEAD; selftest helper)
#   scripts/gates/issue-681-zpp-wire-gate.sh --selftest        (negative controls, run before trusting)
#
# Fail-closed: a git error while diffing FAILS the gate - "cannot evaluate"
# is never "passed" (council S2).
#
# Shell rules (#676): no grep -q under pipefail; every grep is non-quiet
# (>/dev/null) or reads files directly.

set -u

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null)" || REPO_ROOT=$(pwd)
SELF="$REPO_ROOT/scripts/gates/issue-681-zpp-wire-gate.sh"

# Connect-path files: edits here are wire-surface by definition
CONNECT_PATH_FILES='^(fbird_connection\.c|pdo_fbird/pdo_fbird_driver\.c|src/cpp/fb_connection\.hpp|src/cpp/fb_dpb_builder\.hpp)$'
# Forwarding-locus patterns (the actual #671 line shape)
FORWARDING_PAT='largs\[\]|xdrs\[|args\[SYNC\]'

fail() { echo "ISSUE681_GATE_FAIL: $1" >&2; exit 1; }
ok() { echo "ISSUE681_GATE_OK"; exit 0; }

# $1 = repo dir, $2 = diff args for git -C dir diff
evaluate() {
    local dir="$1"; shift
    local changed rc=0
    changed=$(git -C "$dir" diff --name-only "$@" 2>/dev/null) || rc=$?
    if [ "$rc" -ne 0 ]; then
        fail "git diff failed - cannot evaluate the gate (fail closed)"
    fi
    [ -z "$changed" ] && { ok; }

    local code_changed
    code_changed=$(echo "$changed" | grep -E '\.(c|cpp|h|hpp)$' | grep -vE '^tests/' || true)
    [ -z "$code_changed" ] && { ok; }

    # layer 3: connect-path files are wire-surface by definition
    if echo "$code_changed" | grep -E "$CONNECT_PATH_FILES" >/dev/null; then
        :
    else
        # layers 1+2: token grep on the changed lines of other code files
        local diffbody
        diffbody=$(git -C "$dir" diff HEAD -- $code_changed 2>/dev/null || git -C "$dir" diff "$@" -- $code_changed 2>/dev/null)
        if ! printf '%s' "$diffbody" | grep -E '^[+-].*(zend_parse_parameters|ZEND_BEGIN_ARG|ZEND_ARG_INFO|isc_dpb_|isc_spb_|isc_tpb_|largs\[\]|xdrs\[|args\[SYNC\])' >/dev/null; then
            ok
        fi
    fi

    if ! echo "$changed" | grep -E '^tests/.*\.phpt$' >/dev/null; then
        fail "zpp/arginfo/DPB-SPB-TPB/connect-path surface changed without a tests/*.phpt change in the same diff (#681: wire-observation test required; return-value tests cannot see parsed-but-unforwarded arguments)"
    fi
    ok
}

if [ "${1:-}" = "--diff-dir" ]; then
    [ -n "${2:-}" ] || fail "--diff-dir requires a directory"
    evaluate "$2" HEAD
fi

if [ "${1:-}" = "--selftest" ]; then
    tmp=$(mktemp -d) || fail "no tmpdir"
    trap 'rm -rf "$tmp"' EXIT
    git -C "$REPO_ROOT" rev-parse HEAD >/dev/null 2>&1 || fail "selftest needs the real repo (baseline file)"
    mkdir -p "$tmp/repo/tests"
    cat > "$tmp/repo/fbird_connection.c" <<'EOF2'
PHP_FUNCTION(fbird_connect_dummy)
{
    char *sync;
    size_t sync_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s!", &sync, &sync_len) == FAILURE) {
        RETURN_FALSE;
    }
}

static void connect_impl(zend_long buffers, zend_long dialect, zend_long sync)
{
    zend_long largs[] = { buffers, dialect, sync };
    (void)largs;
}
EOF2
    echo "--TEST-- seeded wire oracle" > "$tmp/repo/tests/.keep"
    git -C "$tmp/repo" init -q .
    git -C "$tmp/repo" add -A
    git -C "$tmp/repo" -c user.email=t@t -c user.name=t commit -qm base

    # Case 1: zpp format-string change, NO test change -> must FAIL
    sed -i 's/"s!"/"s"/' "$tmp/repo/fbird_connection.c"
    out=$("$SELF" --diff-dir "$tmp/repo" 2>&1); rc=$?
    if [ "$rc" -ne 1 ] || ! echo "$out" | grep "ISSUE681_GATE_FAIL" >/dev/null; then
        echo "SELFTEST BROKEN: zpp-only diff was NOT flagged (RED missing)" >&2
        exit 1
    fi
    # Case 2: same zpp change WITH a staged new .phpt -> must PASS with marker
    echo "--TEST-- wire oracle for seeded change" > "$tmp/repo/tests/seeded_wire.phpt"
    git -C "$tmp/repo" add tests/seeded_wire.phpt
    out=$("$SELF" --diff-dir "$tmp/repo" 2>&1); rc=$?
    if [ "$rc" -ne 0 ] || ! echo "$out" | grep "ISSUE681_GATE_OK" >/dev/null; then
        echo "SELFTEST BROKEN: diff WITH test change was flagged or marker missing (false red)" >&2
        exit 1
    fi
    git -C "$tmp/repo" -c user.email=t@t -c user.name=t commit -qm withtest

    # Case 3 (council S5 evasion): ONLY the largs[] forwarding line changes,
    # no zpp tokens on any diff line, NO test -> must FAIL
    sed -i 's/zend_long largs\[\] = { buffers, dialect, sync };/zend_long largs[] = { buffers, dialect, 0 };/' "$tmp/repo/fbird_connection.c"
    out=$("$SELF" --diff-dir "$tmp/repo" 2>&1); rc=$?
    if [ "$rc" -ne 1 ] || ! echo "$out" | grep "ISSUE681_GATE_FAIL" >/dev/null; then
        echo "SELFTEST BROKEN: largs[]-only forwarding diff (the literal #671 line shape) was NOT flagged (RED missing)" >&2
        exit 1
    fi

    echo "ISSUE681_SELFTEST_OK"
    exit 0
fi

# Real PR mode
DIFFSPEC="${1:-origin/main...HEAD}"
evaluate "$REPO_ROOT" "$DIFFSPEC"
