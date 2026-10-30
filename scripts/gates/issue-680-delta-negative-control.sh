#!/bin/bash
# Issue #680 negative control: prove the tag-gate delta run CATCHES a seeded
# wire-behavior regression. Unlazy rule: an absence check is only honest after
# the same logic fails against a known positive fixture.
#
# Seeds the actual #671 regression (default connect sends
# isc_dpb_force_write=0) into a local pseudo-tag derived from the previous
# release tag, runs scripts/test-delta.sh, and PASSES only if the delta run
# flags the #671 oracle as an unadjudicated behavior delta (gate exit 1).
#
# Usage: scripts/gates/issue-680-delta-negative-control.sh [prev-tag]
#   (default: the highest v13.2.* tag)
# Leaves no traces: the seed tag and worktree are removed on exit.

set -u

REPO="$(git rev-parse --show-toplevel)"
cd "$REPO"
PREV_TAG="${1:-$(git tag -l 'v13.2.*' | sort -V | tail -1)}"
SEED_TAG="${PREV_TAG}-680seed"
[ -n "$PREV_TAG" ] || { echo "no previous tag found" >&2; exit 2; }

WORKTREE=$(mktemp -d /tmp/opencode/php-firebird/delta-nc-XXXXXX)
cleanup() {
    git tag -d "$SEED_TAG" >/dev/null 2>&1
    git worktree remove --force "$WORKTREE" >/dev/null 2>&1
    rm -rf "$WORKTREE"
}
trap cleanup EXIT

echo ">>> [nc] pseudo-tag $SEED_TAG from $PREV_TAG with the #671 regression seeded"
git worktree add --detach "$WORKTREE" "$PREV_TAG" >/dev/null 2>&1 || { echo "worktree failed" >&2; exit 2; }

# THE SEED: the exact pre-#671 behavior - default connect sends force_write=0
sed -i 's/zend_long largs\[\] = { buffers, dialect, sync };/zend_long largs[] = { buffers, dialect, 0 }; \/* #680 seeded regression *\//' \
    "$WORKTREE/fbird_connection.c"
if ! grep -q "#680 seeded regression" "$WORKTREE/fbird_connection.c"; then
    echo "SEED FAILED: pattern not found in $PREV_TAG/fbird_connection.c - inspect the tag" >&2
    exit 2
fi
git -C "$WORKTREE" add fbird_connection.c
git -C "$WORKTREE" -c user.email=gate@local -c user.name=gate commit -qm "#680 negative control: seed #671 regression"
git -C "$WORKTREE" tag "$SEED_TAG"

echo ">>> [nc] delta run against the seeded build (must FLAG the #671 oracle)"
out=$(bash "$REPO/scripts/test-delta.sh" "$SEED_TAG" "tests/issue671*" php84-fb3-dev 2>&1)
rc=$?

echo "$out" | sed 's/^/    /'
echo ""

if [ "$rc" -ne 1 ]; then
    echo "NEGATIVE CONTROL BROKEN: expected delta gate exit 1 on a seeded regression, got $rc" >&2
    exit 1
fi
if ! echo "$out" | grep "UNADJUDICATED.*issue671" >/dev/null; then
    echo "NEGATIVE CONTROL BROKEN: delta run did not flag the #671 oracle as an unadjudicated delta" >&2
    exit 1
fi

echo "ISSUE680_NEGATIVE_CONTROL_OK"
