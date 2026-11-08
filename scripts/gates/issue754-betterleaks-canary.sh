#!/usr/bin/env bash
# =============================================================================
# scripts/gates/issue754-betterleaks-canary.sh - canary oracle for #754
# =============================================================================
# Asserts the .betterleaks.toml contract (betterleaks 1.9.0):
#   1. config loads (no #1546-class FTL) and keeps the scan live
#      ([extend] useDefault = true present - allowlist-only config class
#      silently disables the whole scan, harness blind-gate incident #1032)
#   2. suppression: the six Firebird-default-credential FP paths from #754
#      scan clean with the repo config
#   3. canary pair:
#      a) planted digit-bearing AKIA-style id FLAGS via the aws-access-key-id
#         parity rule (1.9.0 default ruleset is digit-blind on AKIA ids)
#      b) canary secret in a NON-allowlisted path FLAGS (default rules)
#      c) non-masterkey canary under EVERY formerly-exempt relative path
#         FLAGS (#759 tightened semantics: content-only suppression means a
#         genuine secret in those files is visible again)
#      d) masterkey canary under a formerly-exempt mirror path does NOT
#         flag (proves the ^masterkey$ content rule suppresses what the
#         dropped path entries used to)
#
# Success-only marker: prints "issue754 betterleaks canary gate: PASS" and
# exits 0 ONLY when all checks pass; exits 1 with a FAIL line otherwise.
#
# RED shape (pre-config): check 3c fails (canary under allowlisted relative
# path flags, exit 1) and check 2 fails with findings.
# Tightened RED shape (#759): under the OLD per-path config, check 3c fails
# (canary suppressed) and check 3d trivially passes - the flip of 3c is the
# tightened-semantics proof.
# Requires: betterleaks in PATH.
# =============================================================================
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CONFIG="$REPO_ROOT/.betterleaks.toml"
TMP="$(mktemp -d /tmp/issue754-canary.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

fail() {
    echo "issue754 betterleaks canary gate: FAIL - $1"
    exit 1
}

command -v betterleaks >/dev/null 2>&1 || fail "betterleaks not in PATH"
[ -f "$CONFIG" ] || fail ".betterleaks.toml missing at repo root"

# --- check 1: config is live, not a blind gate ------------------------------
grep -q 'useDefault[[:space:]]*=[[:space:]]*true' "$CONFIG" \
    || fail "config lost [extend] useDefault = true (allowlist-only config = scan nothing)"
grep -q '^\[allowlist\]' "$CONFIG" \
    || fail "config lost the singular top-level [allowlist] table"
if grep -q '^\[\[rules\.allowlist\]\]' "$CONFIG"; then
    fail "config uses the gitleaks 8.19+ nested [[rules.allowlist]] array table - betterleaks 1.9.0 rejects it FTL (#1546)"
fi

# --- check 2: the #754 FP paths scan clean ----------------------------------
FP_FILES=(tests/config.inc tests/fbird_ini_defaults.phpt tests/fbird_service_ini_defaults.phpt .github/workflows/sanitizers.yml .github/workflows/windows-smoke.yml src/cpp/fb_connection.hpp stubs/firebird-classes.php scripts/verify-bundle.sh)
for f in "${FP_FILES[@]}"; do [ -f "$REPO_ROOT/$f" ] || fail "expected FP fixture missing from repo: $f"; done
betterleaks dir --no-banner --config "$CONFIG" --report-format json \
    --report-path "$TMP/fp.json" -- "${FP_FILES[@]}" >/dev/null 2>&1
RC=$?
[ "$RC" -eq 0 ] || fail "allowlisted FP paths still flag (betterleaks exit $RC): $(jq -r '.[] | .File + ":" + (.StartLine|tostring)' "$TMP/fp.json" 2>/dev/null | tr '\n' ' ')"
[ "$(jq length "$TMP/fp.json" 2>/dev/null)" = "0" ] || fail "FP report not empty"

# --- check 3a: planted digit-bearing AKIA id FLAGS via parity rule ----------
# Canary pieces are concatenated at RUNTIME so the full 20-char token never
# appears verbatim in this committed script - a global content allowlist for
# it would also suppress the TMP-file scan below (self-suppression trap).
mkdir -p "$TMP/planted"
A1='AKIA'
A2='Q2W3E4R5T6Y7U8I0P'
printf '%s%s\n' "$A1" "$A2" > "$TMP/planted/aws-canary.txt"
betterleaks dir --no-banner --config "$CONFIG" --report-format json \
    --report-path "$TMP/aws.json" -- "$TMP/planted/aws-canary.txt" >/dev/null 2>&1
RC=$?
[ "$RC" -ne 0 ] || fail "planted AKIA canary did NOT flag - aws-access-key-id parity rule is dead"
jq -e '.[] | select(.RuleID == "aws-access-key-id")' "$TMP/aws.json" >/dev/null 2>&1 \
    || fail "AKIA canary flagged but not by aws-access-key-id (rule id mismatch)"

# --- check 3b: canary in a NON-allowlisted path FLAGS ------------------------
mkdir -p "$TMP/outside"
P1='xK9mP2vQ8wR4t'
P2='Z6nJ5cE'
# shellcheck disable=SC2016  # $password must stay literal: canary content
printf '$password = "%s";\n' "${P1}${P2}" > "$TMP/outside/canary.txt"
betterleaks dir --no-banner --config "$CONFIG" --report-format json \
    --report-path "$TMP/outside.json" -- "$TMP/outside/canary.txt" >/dev/null 2>&1
RC=$?
[ "$RC" -ne 0 ] || fail "canary secret in a non-allowlisted path did NOT flag - scan is blind"

# --- check 3c (#759 tightened): non-masterkey canary under every formerly-
# --- exempt relative path FLAGS ----------------------------------------------
FORMERLY_EXEMPT=(tests/config.inc tests/fbird_ini_defaults.phpt tests/fbird_service_ini_defaults.phpt .github/workflows/sanitizers.yml .github/workflows/windows-smoke.yml src/cpp/fb_connection.hpp stubs/firebird-classes.php scripts/verify-bundle.sh)
mkdir -p "$TMP/mirror"
for f in "${FORMERLY_EXEMPT[@]}"; do
    mkdir -p "$TMP/mirror/$(dirname "$f")"
    # shellcheck disable=SC2016  # $password must stay literal: canary content
    printf '$password = "%s";\n' "${P1}${P2}" > "$TMP/mirror/$f"
    betterleaks dir --no-banner --config "$CONFIG" --report-format json \
        --report-path "$TMP/mirror.json" -- "$TMP/mirror/$f" >/dev/null 2>&1
    RC=$?
    [ "$RC" -ne 0 ] || fail "non-masterkey canary under formerly-exempt path ($f) did NOT flag - a per-path exemption is still hiding real secrets (#759 not applied?)"
done

# --- check 3d (#759): masterkey canary under a formerly-exempt mirror path ---
# --- does NOT flag (content rule suppresses what the dropped paths used to) --
MK1='master'
MK2='key'
# shellcheck disable=SC2016  # $password must stay literal: canary content
printf '$password = "%s";\n' "${MK1}${MK2}" > "$TMP/mirror/tests/config.inc"
betterleaks dir --no-banner --config "$CONFIG" --report-format json \
    --report-path "$TMP/mk.json" -- "$TMP/mirror/tests/config.inc" >/dev/null 2>&1
RC=$?
[ "$RC" -eq 0 ] || fail "masterkey canary under formerly-exempt path still flags - the ^masterkey$ content rule lost the #754 suppression"

echo "issue754 betterleaks canary gate: PASS"
