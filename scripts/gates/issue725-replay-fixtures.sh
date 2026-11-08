#!/usr/bin/env bash
# =============================================================================
# scripts/gates/issue725-replay-fixtures.sh - fixture matrix for the P4
# weather classifier (#725, audit F1): the production classifier
# (lib/weather-replay.sh weather_classify) must separate pool weather from
# genuine red on the four fixture classes that motivated the policy, and the
# negative control proves the oracle can fail.
#
# Success-only marker: prints "issue725 replay fixture gate: PASS" and exits
# 0 ONLY when every class matches. Exit 1 with a FAIL line otherwise.
# =============================================================================
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck source=scripts/lib/weather-replay.sh
source "$SCRIPT_DIR/scripts/lib/weather-replay.sh"

fail() { echo "issue725 replay fixture gate: FAIL - $1"; exit 1; }
command -v jq >/dev/null 2>&1 || fail "jq not available"

TMP="$(mktemp -d /tmp/issue725-fixtures.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

expect_shape() {
    local name="$1" file="$2" want="$3" got
    got=$(weather_classify < "$file") || true
    [ "$got" = "$want" ] || fail "fixture ${name}: expected '${want}', got '${got}'"
}

# --- fixture 1: pool cancellation (the #725 macOS signature) -----------------
# run failed, two bad jobs, both cancelled, zero executed steps
cat > "$TMP/pool-cancelled.json" <<'JSON'
{"conclusion": "failure", "jobs": [
  {"name": "build-a", "conclusion": "success", "steps": [{"name": "s1", "conclusion": "success"}]},
  {"name": "build-b", "conclusion": "cancelled", "steps": [{"name": "s1", "conclusion": null}, {"name": "s2", "conclusion": null}]},
  {"name": "build-c", "conclusion": "cancelled", "steps": []}
]}
JSON
expect_shape "pool-cancelled" "$TMP/pool-cancelled.json" "queue-starvation"

# --- fixture 2: whole-run cancellation - run cancelled BEFORE jobs were ------
# --- created (0 jobs). A cancelled run whose bad jobs all have zero steps ---
# --- classifies queue-starvation (checked first, same action either way) -----
cat > "$TMP/whole-cancelled.json" <<'JSON'
{"conclusion": "cancelled", "jobs": []}
JSON
expect_shape "whole-cancelled" "$TMP/whole-cancelled.json" "whole-run-cancelled"

# --- fixture 2b: cancelled with only skipped jobs = not replayable -----------
cat > "$TMP/cancelled-skipped.json" <<'JSON'
{"conclusion": "cancelled", "jobs": [
  {"name": "build-a", "conclusion": "skipped", "steps": []}
]}
JSON
expect_shape "cancelled-skipped" "$TMP/cancelled-skipped.json" "none"

# --- fixture 2c: cancelled mid-flight (executed steps lost) = NOT weather ----
cat > "$TMP/cancelled-midflight.json" <<'JSON'
{"conclusion": "cancelled", "jobs": [
  {"name": "build-a", "conclusion": "cancelled", "steps": [{"name": "s1", "conclusion": "success"}]}
]}
JSON
expect_shape "cancelled-midflight" "$TMP/cancelled-midflight.json" "none"

# --- fixture 3: MIXED failure/cancellation = genuine red, NOT replayable -----
cat > "$TMP/mixed-red.json" <<'JSON'
{"conclusion": "failure", "jobs": [
  {"name": "build-a", "conclusion": "failure", "steps": [{"name": "s1", "conclusion": "success"}, {"name": "s2", "conclusion": "failure"}]},
  {"name": "build-b", "conclusion": "cancelled", "steps": []}
]}
JSON
expect_shape "mixed-red" "$TMP/mixed-red.json" "none"

# --- fixture 4: missing steps field entirely (API shape variance) ------------
cat > "$TMP/no-steps-field.json" <<'JSON'
{"conclusion": "failure", "jobs": [
  {"name": "build-a", "conclusion": "cancelled"},
  {"name": "build-b", "conclusion": "cancelled"}
]}
JSON
expect_shape "no-steps-field" "$TMP/no-steps-field.json" "queue-starvation"

# --- fixture 5: startup failure is NOT in the enum (documented extension) ----
cat > "$TMP/startup-failure.json" <<'JSON'
{"conclusion": "failure", "jobs": [
  {"name": "build-a", "conclusion": "startup_failure", "steps": []}
]}
JSON
expect_shape "startup-failure" "$TMP/startup-failure.json" "none"

# --- fixture 6: green run - nothing to replay --------------------------------
cat > "$TMP/all-success.json" <<'JSON'
{"conclusion": "success", "jobs": [
  {"name": "build-a", "conclusion": "success", "steps": [{"name": "s1", "conclusion": "success"}]}
]}
JSON
expect_shape "all-success" "$TMP/all-success.json" "none"

# --- negative control: ONE executed step flips pool-cancelled to none --------
# The oracle must be able to fail: a cancelled job that DID execute means
# real work was lost mid-flight - not pool weather.
jq '.jobs[1].steps[0].conclusion = "success"' "$TMP/pool-cancelled.json" > "$TMP/mutated.json"
expect_shape "mutated-pool-cancelled" "$TMP/mutated.json" "none"

echo "issue725 replay fixture gate: PASS"
