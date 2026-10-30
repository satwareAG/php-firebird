#!/bin/bash
# Branch provenance check (#688-P5, council run 5 six-point program).
#
# 2026-10-29 near-miss (council run 5): a hotfix branch was cut from a STALE
# local main and caught by accident via a .gitignore diff. This script makes
# that class fail loud: the current branch must CONTAIN the freshly fetched
# origin/<base> tip (i.e., it was cut from the freshest base, not a stale
# local one).
#
# Usage:
#   ./scripts/branch-provenance.sh [base-branch]     # default: main
#
# Exit codes: 0 = provenance verified; 1 = STALE BASE (the branch does not
# contain the remote tip); 2 = usage/environment error.
#
# Wiring (AGENTS.md): run before starting work on any branch, and wire as a
# pre-push hook: a push of a branch that does not contain origin/main's tip
# is refused (the provenance failure surfaces at the latest safe moment).

set -euo pipefail

BASE_BRANCH="${1:-main}"
BRANCH="$(git rev-parse --abbrev-ref HEAD)"

git fetch origin "$BASE_BRANCH" --quiet

REMOTE_TIP="$(git rev-parse --short "origin/${BASE_BRANCH}")"
MERGE_BASE="$(git merge-base HEAD "origin/${BASE_BRANCH}" 2>/dev/null || echo none)"

echo "branch provenance: branch=${BRANCH} base=${BASE_BRANCH}"
echo "  origin/${BASE_BRANCH} tip : ${REMOTE_TIP}"
echo "  merge-base(HEAD, origin/${BASE_BRANCH}): ${MERGE_BASE}"

if [ "$MERGE_BASE" = "none" ]; then
    echo "PROVENANCE FAIL: the branch shares no history with ${BASE_BRANCH} - wrong base?" >&2
    exit 1
fi

if [ "$(git rev-parse --short "$MERGE_BASE")" = "$REMOTE_TIP" ]; then
    echo "PROVENANCE OK: the branch contains origin/${BASE_BRANCH}'s tip (fresh base)"
    exit 0
fi

# The merge-base != the remote tip: the branch was cut from a stale base.
# Distinguish: is the remote tip an ANCESTOR of the merge-base (local base is
# NEWER than remote - a fast-forward is available) or NOT (diverged)?
if git merge-base --is-ancestor "origin/${BASE_BRANCH}" "$MERGE_BASE"; then
    echo "PROVENANCE WARN: the branch is FRESH relative to origin/${BASE_BRANCH} but the base is behind - fast-forward ${BASE_BRANCH} first (git pull) then re-verify" >&2
    exit 0
fi

echo "PROVENANCE FAIL: the branch does NOT contain origin/${BASE_BRANCH}'s tip - stale base (the 2026-10-29 near-miss class; cut from the fetched tip instead)" >&2
exit 1
