#!/bin/bash
# Issue #680: tag-gate delta test - run the CURRENT oracle suite against a
# PREVIOUS release tag build and report wire-behavior differences.
#
# The #671 regression shipped in v13.0.0 and survived 11 release trains
# because the pre-tag checklist never compared WIRE BEHAVIOR against the
# previously released build. This script makes that comparison mechanical:
#
#   scripts/test-delta.sh <prev-tag> [test-glob] [compose-lane]
#
# Steps:
#   1. Run the current oracle suite on the candidate tree (lane build)  -> manifest A
#   2. Check out <prev-tag> into a disposable git worktree (NOT the shared
#      tree - worktrees keep the 12-lane rule intact), build it in the same
#      compose lane via a volume override, copy the CURRENT test files in,
#      run the same glob there -> manifest B
#   3. Adjudicate:
#      - green(A) + red(B)   = behavior delta introduced by the candidate.
#        Each one MUST have an allowlist entry in scripts/delta-allowlist.txt
#        ("<test-file-basename> <reason: CHANGELOG/UPGRADING ref>") or the
#        gate FAILS. Blanket success is forbidden - the delta is the product.
#      - red(A) + green(B)   = fixed-in-candidate, listed, never blocks.
#      - red in both         = pre-existing, listed, never blocks.
#
# Exit codes: 0 = no unadjudicated delta; 1 = unadjudicated delta;
#             2 = usage/environment error.
#
# Negative control: scripts/gates/issue-680-delta-negative-control.sh
# (seeds the #671 regression into the prev-tag build and requires the delta
# run to catch it).

set -u

usage() {
    echo "usage: $0 <prev-tag> [test-glob] [compose-lane]" >&2
    echo "  prev-tag     release tag to build and compare against (e.g. v13.2.11)" >&2
    echo "  test-glob    test file(s) for the delta run (default: tests/issue671*)" >&2
    echo "  compose-lane compose service to build/run on (default: php84-fb3-dev)" >&2
    exit 2
}

[ $# -ge 1 ] || usage
PREV_TAG="$1"
GLOB="${2:-tests/issue671*}"
LANE="${3:-php84-fb3-dev}"
COMPOSE="docker compose -f docker/docker-compose.yml"
ALLOWLIST="$(git rev-parse --show-toplevel)/scripts/delta-allowlist.txt"
WORKTREE="$(mktemp -d /tmp/opencode/php-firebird/delta-XXXXXX)"
trap 'git worktree remove --force "$WORKTREE" 2>/dev/null; rm -rf "$WORKTREE"' EXIT

[ "$(git tag -l "$PREV_TAG")" = "$PREV_TAG" ] || { echo "tag $PREV_TAG not found" >&2; exit 2; }
# Multi-glob support: at least ONE whitespace-separated pattern must match
# (patterns without matches on the candidate tree are skipped downstream).
matched=0
FILES=""
for pat in $GLOB; do
    if ls $pat >/dev/null 2>&1; then
        matched=1
        FILES="$FILES $(ls $pat | tr '\n' ' ')"
    fi
done
[ "$matched" -eq 1 ] || { echo "no test files match $GLOB" >&2; exit 2; }
# test.sh aborts on any argument that does not resolve - feed it exact files
GLOB="$FILES"

echo ">>> [delta] candidate run on $LANE: $GLOB"
if ! out_a=$($COMPOSE up -d "$LANE" >/dev/null 2>&1; $COMPOSE exec -T "$LANE" /ext/scripts/build.sh >/dev/null 2>&1; $COMPOSE exec -T "$LANE" /ext/scripts/test.sh $GLOB 2>&1); then
    echo "candidate run failed - fix the candidate before running a delta" >&2
    echo "$out_a" | grep -E "\[FAIL\]" | sed -n '1,10p' >&2
    exit 2
fi
manifest_a=$(echo "$out_a" | grep -oE "tests/[a-zA-Z0-9_./-]+\.phpt" | sort -u)

echo ">>> [delta] worktree at $WORKTREE ($PREV_TAG)"
git worktree add --detach "$WORKTREE" "$PREV_TAG" >/dev/null 2>&1 || { echo "worktree add failed" >&2; exit 2; }

echo ">>> [delta] previous-tag build on $LANE (volume override)"
$COMPOSE run --rm -v "$WORKTREE:/ext" "$LANE" /ext/scripts/build.sh >/dev/null 2>&1 \
    || { echo "previous-tag build failed" >&2; exit 2; }

# Copy the CURRENT oracle files into the previous-tag worktree (they are
# untracked there by construction - the previous tag predates them).
for f in $GLOB; do
    mkdir -p "$WORKTREE/$(dirname "$f")"
    cp "$f" "$WORKTREE/$f"
done
# The oracles require their runtime helpers from the CURRENT tree as well
for h in firebird.inc functions.inc config.inc skipif.inc clean.inc cross_version.inc; do
    [ -f "tests/$h" ] && cp "tests/$h" "$WORKTREE/tests/$h"
done

echo ">>> [delta] previous-tag run: $GLOB"
out_b=$($COMPOSE run --rm -v "$WORKTREE:/ext" "$LANE" /ext/scripts/test.sh $GLOB 2>&1)
manifest_b=$(echo "$out_b" | grep -oE "tests/[a-zA-Z0-9_./-]+\.phpt" | sort -u)

pass_a=$(echo "$out_a" | grep -E "\[1;32mPASS" | grep -oE "tests/[a-zA-Z0-9_./-]+\.phpt" | sort -u)
pass_b=$(echo "$out_b" | grep -E "\[1;32mPASS" | grep -oE "tests/[a-zA-Z0-9_./-]+\.phpt" | sort -u)

echo ""
echo "=== Delta report ($PREV_TAG -> candidate, lane $LANE) ==="
status=0
changed=""
for t in $(echo "$pass_a $pass_b" | tr ' ' '\n' | sort -u); do
    in_a=$(echo "$pass_a" | grep -Fc "$t" || true)
    in_b=$(echo "$pass_b" | grep -Fc "$t" || true)
    if [ "$in_a" -ne "$in_b" ]; then
        changed="$changed $t"
    fi
done

if [ -n "$changed" ]; then
    echo "Behavior deltas vs $PREV_TAG (each requires adjudication - the delta is the product):"
    for t in $changed; do
        in_a=$(echo "$pass_a" | grep -Fc "$t" || true)
        base=$(basename "$t")
        if [ -f "$ALLOWLIST" ] && grep -E "^${base}[[:space:]]" "$ALLOWLIST" >/dev/null; then
            reason=$(grep -E "^${base}[[:space:]]" "$ALLOWLIST" | head -1 | cut -f2-)
            echo "  ADJUDICATED $t - $reason"
        else
            if [ "$in_a" -gt 0 ]; then
                echo "  UNADJUDICATED $t (green on candidate, red on $PREV_TAG - intended change?)"
            else
                echo "  UNADJUDICATED $t (green on $PREV_TAG, red on candidate - CANDIDATE REGRESSION, do not ship)"
            fi
            status=1
        fi
    done
else
    echo "No behavior deltas."
fi

pre_red=""
for t in $manifest_a; do
    if echo "$pass_a" | grep -F "$t" >/dev/null; then continue; fi
    if echo "$pass_b" | grep -F "$t" >/dev/null; then continue; fi
    pre_red="$pre_red $t"
done
[ -n "$pre_red" ] && echo "Pre-existing failures in BOTH builds (informational):$pre_red"

if [ "$status" -ne 0 ]; then
    echo "" >&2
    echo "GATE FAIL: unadjudicated behavior delta(s) - add scripts/delta-allowlist.txt entries with CHANGELOG/UPGRADING references, or fix the candidate" >&2
fi
exit $status
