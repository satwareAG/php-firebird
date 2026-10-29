#!/bin/bash
# Issue #681 gate: zpp default/forwarding changes require wire-observation tests.
#
# Fails when a diff changes zpp argument parsing, arginfo, or DPB/SPB/TPB
# buffer building WITHOUT a corresponding tests/*.phpt change in the same
# diff. Parsed-but-unforwarded arguments and hardcoded DPB literals (#595,
# #671 family) are invisible to return-value tests; this gate forces the
# wire-observation test to ride the same PR.
#
# Usage:
#   scripts/gates/issue-681-zpp-wire-gate.sh [git-diff-spec]   (default origin/main...HEAD)
#   scripts/gates/issue-681-zpp-wire-gate.sh --diff-dir DIR    (worktree vs its HEAD; selftest helper)
#   scripts/gates/issue-681-zpp-wire-gate.sh --selftest        (negative control, run before trusting)
#
# Shell rules (#676): no grep -q under pipefail; every grep is non-quiet
# (>/dev/null) or reads files directly.

set -u

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null)" || REPO_ROOT=$(pwd)
SELF="$REPO_ROOT/scripts/gates/issue-681-zpp-wire-gate.sh"

fail() { echo "ISSUE681_GATE_FAIL: $1" >&2; exit 1; }
ok() { echo "ISSUE681_GATE_OK"; exit 0; }

run_gate_on_repo() { # $1 = repo dir, uses worktree-vs-HEAD diff
    local dir="$1"
    local changed diffbody
    changed=$(git -C "$dir" diff --name-only HEAD 2>/dev/null)
    [ -z "$changed" ] && { ok; }
    local code_changed
    code_changed=$(echo "$changed" | grep -E '\.(c|cpp|h|hpp)$' | grep -vE '^tests/' || true)
    [ -z "$code_changed" ] && { ok; }
    diffbody=$(git -C "$dir" diff HEAD -- $code_changed 2>/dev/null)
    if ! printf '%s' "$diffbody" | grep -E '^[+-].*(zend_parse_parameters|ZEND_BEGIN_ARG|ZEND_ARG_INFO|isc_dpb_|isc_spb_|isc_tpb_)' >/dev/null; then
        ok
    fi
    if ! echo "$changed" | grep -E '^tests/.*\.phpt$' >/dev/null; then
        fail "zpp/arginfo/DPB-SPB-TPB surface changed without a tests/*.phpt change in the same diff (#681: wire-observation test required; return-value tests cannot see parsed-but-unforwarded arguments)"
    fi
    ok
}

if [ "${1:-}" = "--selftest" ]; then
    tmp=$(mktemp -d) || fail "no tmpdir"
    trap 'rm -rf "$tmp"' EXIT
    git -C "$REPO_ROOT" rev-parse HEAD >/dev/null 2>&1 || fail "selftest needs the real repo (baseline file)"
    # Baseline: copy the real gate into a throwaway repo with a clean file
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
EOF2
    echo "--TEST-- seeded wire oracle" > "$tmp/repo/tests/.keep"
    git -C "$tmp/repo" init -q .
    git -C "$tmp/repo" add -A
    git -C "$tmp/repo" -c user.email=t@t -c user.name=t commit -qm base
    # Case 1: zpp format change, NO test change -> must FAIL
    sed -i 's/"s!"/"s"/' "$tmp/repo/fbird_connection.c"
    out=$("$SELF" --diff-dir "$tmp/repo" 2>&1)
    if ! echo "$out" | grep "ISSUE681_GATE_FAIL" >/dev/null; then
        echo "SELFTEST BROKEN: zpp-only diff was NOT flagged (RED missing)" >&2
        exit 1
    fi
    # Case 2: same zpp change WITH a new .phpt -> must PASS
    # (staged so it appears in the worktree-vs-HEAD diff, like a committed PR file)
    echo "--TEST-- wire oracle for seeded change" > "$tmp/repo/tests/seeded_wire.phpt"
    git -C "$tmp/repo" add tests/seeded_wire.phpt
    out=$("$SELF" --diff-dir "$tmp/repo" 2>&1)
    if echo "$out" | grep "ISSUE681_GATE_FAIL" >/dev/null; then
        echo "SELFTEST BROKEN: diff WITH test change was flagged (false red)" >&2
        exit 1
    fi
    echo "ISSUE681_SELFTEST_OK"
    exit 0
fi

if [ "${1:-}" = "--diff-dir" ]; then
    [ -n "${2:-}" ] || fail "--diff-dir requires a directory"
    run_gate_on_repo "$2"
fi

# Real PR mode
DIFFSPEC="${1:-origin/main...HEAD}"
changed=$(git -C "$REPO_ROOT" diff --name-only "$DIFFSPEC" 2>/dev/null)
[ -z "$changed" ] && { ok; }
code_changed=$(echo "$changed" | grep -E '\.(c|cpp|h|hpp)$' | grep -vE '^tests/' || true)
[ -z "$code_changed" ] && { ok; }
diffbody=$(git -C "$REPO_ROOT" diff "$DIFFSPEC" -- $code_changed 2>/dev/null)
if ! printf '%s' "$diffbody" | grep -E '^[+-].*(zend_parse_parameters|ZEND_BEGIN_ARG|ZEND_ARG_INFO|isc_dpb_|isc_spb_|isc_tpb_)' >/dev/null; then
    ok
fi
if ! echo "$changed" | grep -E '^tests/.*\.phpt$' >/dev/null; then
    fail "zpp/arginfo/DPB-SPB-TPB surface changed without a tests/*.phpt change in the same diff (#681: wire-observation test required)"
fi
ok
