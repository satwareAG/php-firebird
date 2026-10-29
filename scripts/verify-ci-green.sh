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
# CI_GREEN_RERUN_TIMEOUT seconds (default 1500, per starved workflow,
# start-wait included) for the rerun to finish. The rerun needs a token
# with actions:write.
#
# Stale-run guard (council 2026-10-27): a run whose head SHA does not
# match the local branch head never passes (and is never rerun) - a green
# run of an older commit must not green-light the commit about to be
# tagged.
#
# doctrine-downstream dispatch (#667): no main-push runs exist for that
# workflow; when gating paths changed since the last release tag, the
# gate dispatches it on this HEAD and waits up to
# CI_GREEN_DOCTRINE_TIMEOUT seconds (default 1500) for green.
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
# Arguments: workflow, run id (fetched ONCE by the caller, so the
# classified run is the run that gets rerun - no re-fetch race).
# Returns 0 only if the rerun completed green within RERUN_TIMEOUT seconds
# (start-wait included in the budget).
retry_starved_run() {
    local wf="$1" run_id="$2" status state conclusion rerun_err waited=0
    is_queue_starvation "$run_id" || return 1

    log_warn "  ${wf}: queue-starvation signature (all failing jobs cancelled with zero steps) - auto-rerunning failed jobs once (#663)..."

    rerun_err=$(gh run rerun --failed "$run_id" 2>&1) || { log_fail "  ${wf}: rerun could not be started: ${rerun_err}"; return 1; }

    # Wait for the rerun attempt to start: the pre-rerun state still reads
    # status=completed for a while - never treat it as the rerun result.
    # Council S3: this wait counts against RERUN_TIMEOUT (uncounted, the
    # worst-case gate latency multiplied by the workflow count).
    local _
    for _ in $(seq 1 24); do
        status=$(gh run view "$run_id" --json status --jq '.status' 2>/dev/null || echo "")
        [ "$status" != "completed" ] && break
        waited=$((waited + 5))
        if [ "$waited" -ge "$RERUN_TIMEOUT" ]; then
            log_fail "  ${wf}: rerun did not start within ${RERUN_TIMEOUT}s"
            return 1
        fi
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
        --json status,conclusion,headSha,createdAt,databaseId 2>/dev/null | jq '.[0] // empty' || true)

    if [ -z "$RESULT" ]; then
        log_fail "  ${wf}: no run found on ${BRANCH}"
        ALL_PASSED=false
        continue
    fi

    STATUS=$(echo "$RESULT" | jq -r '.status')
    CONCLUSION=$(echo "$RESULT" | jq -r '.conclusion // "running"')
    SHA=$(echo "$RESULT" | jq -r '.headSha[0:8]')
    CREATED=$(echo "$RESULT" | jq -r '.createdAt[0:16]')
    RUN_ID=$(echo "$RESULT" | jq -r '.databaseId')

    # Stale-run guard (council 2026-10-27): a green (or starved-then-green)
    # run of an OLDER commit must not green-light the commit about to be
    # tagged. Skipped only when the branch head cannot be resolved locally.
    if LOCAL_SHA=$(git rev-parse --verify --short=8 "refs/heads/${BRANCH}" 2>/dev/null); then
        if [ "$SHA" != "$LOCAL_SHA" ]; then
            log_fail "  ${wf}: run is for ${SHA} but ${BRANCH} is at ${LOCAL_SHA} - stale CI, rerun/land CI for the current commit"
            ALL_PASSED=false
            continue
        fi
    else
        log_warn "  ${wf}: cannot verify run SHA against ${BRANCH} (not a git checkout or unknown branch)"
    fi

    if [ "$STATUS" != "completed" ]; then
        log_warn "  ${wf}: ${STATUS} (started ${CREATED})"
        ALL_PASSED=false
    elif [ "$CONCLUSION" != "success" ]; then
        if retry_starved_run "$wf" "$RUN_ID"; then
            log_pass "  ${wf}: success (queue-starvation rerun, ${SHA})"
        else
            log_fail "  ${wf}: ${CONCLUSION} (${SHA}, ${CREATED})"
            ALL_PASSED=false
        fi
    else
        log_pass "  ${wf}: success (${SHA}, ${CREATED})"
    fi
done

# ---------------------------------------------------------------------------
# doctrine-downstream gate (#667): the workflow has no main-push runs
# (PR/weekly-schedule/dispatch triggers only), so it cannot be SHA-checked
# like the workflows above. When gating-relevant paths changed since the
# last release tag, dispatch it on this HEAD and wait for green; otherwise
# the existing PR/weekly coverage stands.
DOCTRINE_TIMEOUT="${CI_GREEN_DOCTRINE_TIMEOUT:-1500}"
BASE_REF="${DOCTRINE_BASE:-$(git describe --tags --abbrev=0 2>/dev/null || echo "")}"
if [ -z "$BASE_REF" ]; then
    log_warn "doctrine-downstream: no release tag found - cannot diff; check manually"
elif ! DOCTRINE_DIFF=$(git diff --name-only "${BASE_REF}..HEAD" -- \
        '*.c' '*.cpp' '*.h' '*.hpp' 'config.m4' 'config.w32' \
        'pdo_fbird' 'stubs' '.github/workflows/doctrine-downstream.yml' 2>/dev/null); then
    log_warn "doctrine-downstream: could not diff ${BASE_REF}..HEAD - check manually"
elif [ -z "$DOCTRINE_DIFF" ]; then
    log_pass "  doctrine-downstream: no gating paths changed since ${BASE_REF} - skipped"
else
    DOCTRINE_COUNT=$(echo "$DOCTRINE_DIFF" | wc -l)

    # #678 reuse pre-check: a completed+success doctrine run on THIS exact
    # HEAD proves the gating paths are downstream-green - dispatching again
    # and waiting 1500s for it (the 13.2.11 false red) is pointless. Any
    # event counts (pull_request / schedule / workflow_dispatch): the wire
    # surface checked is identical for a given SHA.
    d_target_sha=$(git rev-parse "origin/$BRANCH" 2>/dev/null || git rev-parse "$BRANCH" 2>/dev/null || git rev-parse HEAD)
    d_run_id=$(gh run list --workflow doctrine-downstream.yml \
        --json databaseId,headSha,status,conclusion,createdAt --limit 40 2>/dev/null \
        | jq -r --arg sha "$d_target_sha" \
          '[.[] | select(.headSha == $sha and .conclusion == "success")] | sort_by(.createdAt) | last | .databaseId // empty')
    if [ -n "$d_run_id" ]; then
        log_pass "  doctrine-downstream: reusing existing success run #$d_run_id on this HEAD - dispatch skipped (#678)"
    else
    log_info "doctrine-downstream: ${DOCTRINE_COUNT} gating path(s) changed since ${BASE_REF} - dispatching on ${BRANCH}..."
    if ! gh workflow run doctrine-downstream.yml --ref "$BRANCH" >/dev/null 2>&1; then
        log_fail "  doctrine-downstream: dispatch failed"
        ALL_PASSED=false
    else
        d_waited=0
        d_run_id=""
        # wait for the dispatch run to appear (counted against the budget).
        # #678: --limit 1 missed the dispatched run whenever older dispatch
        # runs existed on the branch - widen to 10 and match the SHA there.
        while [ "$d_waited" -lt "$DOCTRINE_TIMEOUT" ]; do
            d_run_id=$(gh run list --workflow doctrine-downstream.yml --branch "$BRANCH" \
                --event workflow_dispatch --limit 10 --json databaseId,headSha 2>/dev/null \
                | jq -r --arg sha "$d_target_sha" \
                  '[.[] | select(.headSha == $sha)] | sort_by(.databaseId) | last | .databaseId // empty')
            [ -n "$d_run_id" ] && break
            sleep 5
            d_waited=$((d_waited + 5))
        done
        if [ -z "$d_run_id" ]; then
            log_fail "  doctrine-downstream: dispatch run did not appear within ${DOCTRINE_TIMEOUT}s"
            ALL_PASSED=false
        else
            while [ "$d_waited" -lt "$DOCTRINE_TIMEOUT" ]; do
                state=$(gh run view "$d_run_id" --json status,conclusion 2>/dev/null || echo "")
                if [ "$(echo "$state" | jq -r '.status // empty')" = "completed" ]; then
                    d_conclusion=$(echo "$state" | jq -r '.conclusion // empty')
                    if [ "$d_conclusion" = "success" ]; then
                        log_pass "  doctrine-downstream: success (dispatch)"
                    else
                        log_fail "  doctrine-downstream: dispatch finished '${d_conclusion:-unknown}'"
                        ALL_PASSED=false
                    fi
                    break
                fi
                sleep 15
                d_waited=$((d_waited + 15))
            done
            if [ "$d_waited" -ge "$DOCTRINE_TIMEOUT" ]; then
                log_fail "  doctrine-downstream: still running after ${DOCTRINE_TIMEOUT}s"
                ALL_PASSED=false
            fi
        fi
    fi
fi
fi

echo ""
if [ "$ALL_PASSED" = "true" ]; then
    log_pass "All CI workflows green on ${BRANCH}. Safe to tag."
    exit 0
else
    log_error "CI not green on ${BRANCH}. Fix failures before tagging."
    exit 1
fi
