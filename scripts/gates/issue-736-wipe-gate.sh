#!/bin/bash
# =============================================================================
# #736 credential-wipe canary gate (FR-004, epic #728)
#
# FR-004 has NO wire delta (bytes on the wire unchanged, no accepted-input
# change => no #680 entry per spec), so the enforceable artifact is SOURCE:
# every credential staging buffer must be scrubbed with fbird_memzero()
# immediately after its hand-off. This gate fails closed:
#   - gate errors (missing files) exit nonzero
#   - --selftest seeds the pre-fix regression into a temp copy and MUST fail
#     there (proves the gate can fail - negative control per #680 doctrine)
#
# Usage: scripts/gates/issue-736-wipe-gate.sh [--selftest]
# =============================================================================
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"

check_tree() {
    # $1 = repo root to check
    local root="$1"
    local ok=0
    # shim exists
    grep -q 'static inline void fbird_memzero' "$root/php_firebird.h" || { echo "FAIL: fbird_memzero shim missing from php_firebird.h"; ok=1; }
    # procedural service attach: scrub right after the fbsvc_attach hand-off
    grep -q 'fbird_memzero(buf, sizeof(buf));' "$root/fbird_service.c" || { echo "FAIL: fbird_service.c buf[350] staging not wiped"; ok=1; }
    # OO FirebirdService::__construct: scrub after its hand-off
    grep -q 'fbird_memzero(buf, sizeof(buf));' "$root/fbird_class_service.c" || { echo "FAIL: fbird_class_service.c buf[256] staging not wiped"; ok=1; }
    # connect path: MD5 context scrubbed after finalization
    grep -q 'fbird_memzero(&hash_context, sizeof(hash_context));' "$root/fbird_connection.c" || { echo "FAIL: fbird_connection.c MD5 context not wiped"; ok=1; }
    # scrub must sit AFTER the hand-off (not before it)
    for f in fbird_service.c fbird_class_service.c; do
        if grep -q 'fbird_memzero' "$root/$f" && [ "$(grep -n 'fbird_memzero' "$root/$f" | head -1 | cut -d: -f1)" -lt "$(grep -n 'fbsvc_attach' "$root/$f" | head -1 | cut -d: -f1)" ]; then
            echo "FAIL: $f wipes the staging buffer BEFORE the attach hand-off (credentials still assembled afterwards)"; ok=1
        fi
    done
    return $ok
}

if [ "${1:-}" = "--selftest" ]; then
    # Negative control: build a temp copy of the PRE-FIX tree (scrub lines
    # stripped) and require the gate to FAIL there.
    TMP="$(mktemp -d /tmp/opencode/php-firebird/issue736-negative.XXXXXX)"
    trap 'rm -rf "$TMP"' EXIT
    cp -r "$REPO/php_firebird.h" "$REPO/fbird_service.c" "$REPO/fbird_class_service.c" "$REPO/fbird_connection.c" "$TMP/"
    sed -i '/FR-004 (#736): the SPB staging buffer holds the DBA password/,+2d' "$TMP/fbird_service.c"
    sed -i '/FR-004 (#736): wipe the SPB staging buffer after the hand-off/,+1d' "$TMP/fbird_class_service.c"
    sed -i '/FR-004 (#736): the MD5 context still holds the accumulated credential/,+1d' "$TMP/fbird_connection.c"
    if check_tree "$TMP" >/dev/null 2>&1; then
        echo "ISSUE736_GATE_SELFTEST_FAIL: gate passed a seeded pre-fix tree (gate cannot fail)"
        exit 1
    fi
    echo "ISSUE736_GATE_SELFTEST_OK (negative control: seeded pre-fix tree correctly fails)"
    exit 0
fi

if check_tree "$REPO"; then
    echo "ISSUE736_GATE_OK"
    exit 0
else
    exit 1
fi
