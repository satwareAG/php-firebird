#!/usr/bin/env bash
# =============================================================================
# scripts/release-replay.sh - P4 weather replay for release lanes (#725)
# =============================================================================
# The mechanical decision the v13.3.1 train made by hand (~3h wall-clock):
# classify a release-lane run through the #688-P4 closed enum and replay it
# ONLY when the failure is pool weather (cancelled with zero executed
# steps). Genuine red refuses with no rerun - the counted error stays
# authoritative (#721). Every replay pass waiver-logs to
# scripts/ci-weather-waivers.log (same format as the pre-tag gate).
#
# Usage: release-replay.sh <run-id> [lane-label]
#   lane-label defaults to "release" - use the workflow file name
#   (release-macos.yml etc.) so the waiver log stays per-lane countable.
#
# Replay is at the run's own SHA by construction (gh run rerun --failed
# re-runs the same revision); the marker/G8 bindings make a stale-SHA
# publish impossible (#722/#723).
#
# Exit codes: 0 replayed green (or replayable and DRY_RUN=1)
#             1 genuine red / classification none / rerun failed or timed out
#             2 usage or gh fetch error
# Requires: gh, jq.
# =============================================================================
set -uo pipefail
# shellcheck source=scripts/lib/logging.sh
source "$(dirname "$0")/lib/logging.sh"
# shellcheck source=scripts/lib/weather-replay.sh
source "$(dirname "$0")/lib/weather-replay.sh"

RERUN_TIMEOUT="${CI_GREEN_RERUN_TIMEOUT:-1500}"

usage() {
    echo "usage: $0 <run-id> [lane-label]" >&2
    exit 2
}

[ $# -ge 1 ] && [ $# -le 2 ] || usage
RUN_ID="$1"
LANE="${2:-release}"

command -v gh >/dev/null 2>&1 || { echo "gh not in PATH" >&2; exit 2; }
command -v jq >/dev/null 2>&1 || { echo "jq not in PATH" >&2; exit 2; }

PAYLOAD=$(gh run view "$RUN_ID" --json conclusion,jobs 2>/dev/null) || {
    log_error "run ${RUN_ID} not fetchable (gh exit $?)"; exit 2
}

SHAPE=$(printf '%s' "$PAYLOAD" | weather_classify)
if [ "$SHAPE" = "none" ]; then
    CONCLUSION="$(printf '%s' "$PAYLOAD" | jq -r '.conclusion')"
    log_fail "run ${RUN_ID} (${LANE}) is NOT pool weather (conclusion: ${CONCLUSION}) - genuine red is not replayable (#725/#721)"
    exit 1
fi

log_warn "run ${RUN_ID} (${LANE}): weather signature - ${SHAPE} - identical-SHA replay (rerun failed jobs once)..."

if [ "${DRY_RUN:-0}" = "1" ]; then
    log_dry_run "would rerun failed jobs of run ${RUN_ID} and waiver-log to ${WEATHER_LOG}"
    exit 0
fi

RERUN_ERR=$(gh run rerun --failed "$RUN_ID" 2>&1) || {
    log_error "rerun could not be started: ${RERUN_ERR}"; exit 1
}

# Wait for the rerun attempt to start (pre-rerun state still reads
# status=completed for a while), then for it to finish - both inside the
# bounded budget (same discipline as verify-ci-green.sh, #663).
WAITED=0
for _ in $(seq 1 24); do
    STATUS=$(gh run view "$RUN_ID" --json status --jq '.status' 2>/dev/null || echo "")
    [ "$STATUS" != "completed" ] && break
    WAITED=$((WAITED + 5))
    if [ "$WAITED" -ge "$RERUN_TIMEOUT" ]; then
        log_fail "rerun did not start within ${RERUN_TIMEOUT}s"
        exit 1
    fi
    sleep 5
done

while true; do
    STATE=$(gh run view "$RUN_ID" --json status,conclusion 2>/dev/null || echo "")
    STATUS="$(printf '%s' "$STATE" | jq -r '.status')"
    CONCLUSION="$(printf '%s' "$STATE" | jq -r '.conclusion')"
    [ "$STATUS" = "completed" ] && break
    WAITED=$((WAITED + 15))
    if [ "$WAITED" -ge "$RERUN_TIMEOUT" ]; then
        log_fail "rerun did not complete within ${RERUN_TIMEOUT}s"
        exit 1
    fi
    sleep 15
done

if [ "$CONCLUSION" = "success" ]; then
    log_pass "run ${RUN_ID} (${LANE}): success (weather replay, ${SHAPE})"
    weather_waiver_log "release-replay(${LANE})" "$RUN_ID" "$SHAPE"
    exit 0
fi

log_fail "run ${RUN_ID} (${LANE}): replay concluded '${CONCLUSION}' - this is no longer weather; RCA required"
exit 1
