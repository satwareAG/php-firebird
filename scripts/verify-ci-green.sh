#!/usr/bin/env bash
# verify-ci-green.sh — Pre-tag CI verification
#
# Verifies that all required CI workflows have passed on the current
# HEAD of main before tagging a release. Run BEFORE git tag.
#
# Queue-starvation auto-retry (#663): if a workflow failed ONLY because a
# job was cancelled while queued with zero steps executed (the macOS
# build-only job's runner-starvation signature), the script auto-reruns
# the failed jobs ONCE (gh run rerun --failed) and waits up to
# CI_GREEN_RERUN_TIMEOUT seconds (default 1500) for the rerun to finish.
# The rerun needs a token with actions:write.
#
# Usage: bash scripts/verify-ci-green.sh [branch]
# Default branch: main
#
# Exit codes:
#   0 = all workflows green
#   1 = one or more workflows failed or not found
set -euo pipefail

source "$(dirname "$0")/lib/logging.sh"

BRANCH="${1:-main}"
REQUIRED_WORKFLOWS=("ci.yml" "code-quality.yml" "sanitizers.yml" "coverage.yml")

# macOS runner allocation took ~14 min in the #663 incident; 1500s gives
# headroom without hanging the gate forever.
RERUN_TIMEOUT="${CI_GREEN_RERUN_TIMEOUT:-1500}"

# True when the run's failure is the #663 queue-starvation signature:
# every non-success job was cancelled while queued (zero steps executed).
is_queue_starvation() {
    local run_id="$1" verdict
    verdict=$(gh run view "$run_id" --json jobs --jq '
        [.jobs[] |
         select(.conclusion != "success" and .conclusion != "skipped"
                and .conclusion != "neutral" and .conclusion != null)] as $bad
        | ($bad | length) > 0
        and all($bad[]; .conclusion == "cancelled"
                and ([.steps[]? | select(.conclusion != null)] | length) == 0)' 2>/dev/null)
    [ "$verdict" = "true" ]
}

# One bounded auto-retry for queue-starvation false-reds (#663).
# Returns 0 only if the rerun completed green within RERUN_TIMEOUT seconds.
retry_starved_run() {
    local wf="$1" branch="$2" run_id status state conclusion waited=0
    run_id=$(gh run list --workflow "$wf" --branch "$branch" --limit 1 \
        --json databaseId --jq '.[0].databaseId' 2>/dev/null) || return 1
    [ -n "$run_id" ] || return 1
    is_queue_starvation "$run_id" || return 1

    log_warn "  ${wf}: queue-starvation signature (job cancelled with zero steps) - auto-rerunning failed jobs once (#663)..."

    gh run rerun --failed "$run_id" >/dev/null 2>&1 || { log_fail "  ${wf}: rerun could not be started"; return 1; }

    # Wait for the rerun attempt to start: the pre-rerun state still reads
    # status=completed for a while - never treat it as the rerun result.
    for _ in $(seq 1 24); do
        status=$(gh run view "$run_id" --json status --jq '.status' 2>/dev/null || echo "")
        [ "$status" != "completed" ] && break
        sleep 5
    done

    while [ "$waited" -lt "$RERUN_TIMEOUT" ]; do
        state=$(gh run view "$run_id" --json status,conclusion 2>/dev/null || echo "")
        if [ "$(echo "$state" | jq -r '.status // empty')" = "completed" ]; then
            conclusion=$(echo "$state" | jq -r '.conclusion // empty')
            if [ "$conclusion" = "success" ]; then
                return 0
            fi
            log_fail "  ${wf}: rerun finished '${conclusion:-unknown}'"
            return 1
        fi
        sleep 15
        waited=$((waited + 15))
    done
    log_fail "  ${wf}: rerun still running after ${RERUN_TIMEOUT}s"
    return 1
}

log_info "Checking CI status on ${BRANCH}..."
echo ""

ALL_PASSED=true

for wf in "${REQUIRED_WORKFLOWS[@]}"; do
    # Get the latest run for this workflow on the specified branch
    RESULT=$(gh run list --workflow "$wf" --branch "$BRANCH" --limit 1 \
        --json status,conclusion,headSha,createdAt 2>/dev/null | jq '.[0] // empty' || true)

    if [ -z "$RESULT" ]; then
        log_fail "  ${wf}: no run found on ${BRANCH}"
        ALL_PASSED=false
        continue
    fi

    STATUS=$(echo "$RESULT" | jq -r '.status')
    CONCLUSION=$(echo "$RESULT" | jq -r '.conclusion // "running"')
    SHA=$(echo "$RESULT" | jq -r '.headSha[0:8]')
    CREATED=$(echo "$RESULT" | jq -r '.createdAt[0:16]')

    if [ "$STATUS" != "completed" ]; then
        log_warn "  ${wf}: ${STATUS} (started ${CREATED})"
        ALL_PASSED=false
    elif [ "$CONCLUSION" != "success" ]; then
        if retry_starved_run "$wf" "$BRANCH"; then
            log_pass "  ${wf}: success (queue-starvation rerun, ${SHA})"
        else
            log_fail "  ${wf}: ${CONCLUSION} (${SHA}, ${CREATED})"
            ALL_PASSED=false
        fi
    else
        log_pass "  ${wf}: success (${SHA}, ${CREATED})"
    fi
done

echo ""
if [ "$ALL_PASSED" = "true" ]; then
    log_pass "All CI workflows green on ${BRANCH}. Safe to tag."
    exit 0
else
    log_error "CI not green on ${BRANCH}. Fix failures before tagging."
    exit 1
fi
