#!/bin/bash
# =============================================================================
# scripts/lib/weather-replay.sh - shared P4 weather classifier + waiver log
# =============================================================================
# Single implementation of the #688-P4 closed-enum weather classification,
# consumed by scripts/verify-ci-green.sh (pre-tag CI gate) and
# scripts/release-replay.sh (release lanes, #725).
#
# Source this file:
#   source "$(dirname "$0")/lib/weather-replay.sh"
#
# weather_classify < run.json
#   Reads a `gh run view --json conclusion,jobs` payload on stdin and prints
#   one of: queue-starvation | whole-run-cancelled | none
#   Exit 0 iff the shape is replayable (the first two), 1 for `none`.
#   Fixture-testable by design: the gh fetch stays in the callers, the
#   verdict logic lives here (audit F1, #725).
#
# weather_waiver_log <lane> <run_id> <shape>
#   Appends to scripts/ci-weather-waivers.log (same format as #688).
# =============================================================================
set -uo pipefail

WEATHER_LOG="${WEATHER_LOG:-scripts/ci-weather-waivers.log}"

weather_classify() {
    local payload shape
    payload=$(cat)
    # bad = jobs that did not succeed (skipped/neutral are not failures)
    # shape 1 (#663): bad jobs exist and ALL of them are cancelled with zero
    # executed steps (steps[] absent or all conclusions null) - the run was
    # pool-starved, nothing actually ran.
    shape=$(printf '%s' "$payload" | jq -r '
        [.jobs[]? |
         select(.conclusion != "success" and .conclusion != "skipped"
                and .conclusion != "neutral" and .conclusion != null)] as $bad
        | if ($bad | length) > 0
           and all($bad[]; .conclusion == "cancelled"
                   and ((.steps // []) | map(select(.conclusion != null)) | length) == 0)
          then "queue-starvation"
          elif (.conclusion == "cancelled")
           and ((.jobs // []) | length == 0
                or all(.jobs[]; .conclusion == "cancelled"
                       and ((.steps // []) | map(select(.conclusion != null)) | length) == 0))
          then "whole-run-cancelled"
          else "none"
          end' 2>/dev/null) || shape="none"
    [ -z "$shape" ] && shape="none"
    printf '%s' "$shape"
    [ "$shape" != "none" ]
}

weather_waiver_log() {
    local lane="$1" run_id="$2" shape="$3"
    printf '%s | %s | run %s | shape: %s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')" "$lane" "$run_id" "$shape" >> "$WEATHER_LOG"
}
