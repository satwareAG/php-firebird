#!/usr/bin/env bash
# verify-ci-green.sh — Pre-tag CI verification
#
# Verifies that all required CI workflows have passed on the current
# HEAD of satware-main before tagging a release. Run BEFORE git tag.
#
# Usage: bash scripts/verify-ci-green.sh [branch]
# Default branch: satware-main
#
# Exit codes:
#   0 = all workflows green
#   1 = one or more workflows failed or not found
set -euo pipefail

BRANCH="${1:-satware-main}"
REQUIRED_WORKFLOWS=("ci.yml" "code-quality.yml" "sanitizers.yml" "coverage.yml")

# Colors
if [[ -t 1 ]]; then
    GREEN='\033[0;32m'
    RED='\033[0;31m'
    YELLOW='\033[1;33m'
    NC='\033[0m'
else
    GREEN='' RED='' YELLOW='' NC=''
fi

echo -e "Checking CI status on ${YELLOW}${BRANCH}${NC}..."
echo ""

ALL_PASSED=true

for wf in "${REQUIRED_WORKFLOWS[@]}"; do
    # Get the latest run for this workflow on the specified branch
    RESULT=$(gh run list --workflow "$wf" --branch "$BRANCH" --limit 1 \
        --json status,conclusion,headSha,createdAt 2>/dev/null | jq '.[0] // empty' || true)

    if [ -z "$RESULT" ]; then
        echo -e "  ${RED}FAIL${NC}  ${wf}: no run found on ${BRANCH}"
        ALL_PASSED=false
        continue
    fi

    STATUS=$(echo "$RESULT" | jq -r '.status')
    CONCLUSION=$(echo "$RESULT" | jq -r '.conclusion // "running"')
    SHA=$(echo "$RESULT" | jq -r '.headSha[0:8]')
    CREATED=$(echo "$RESULT" | jq -r '.createdAt[0:16]')

    if [ "$STATUS" != "completed" ]; then
        echo -e "  ${YELLOW}WAIT${NC}  ${wf}: ${STATUS} (started ${CREATED})"
        ALL_PASSED=false
    elif [ "$CONCLUSION" != "success" ]; then
        echo -e "  ${RED}FAIL${NC}  ${wf}: ${CONCLUSION} (${SHA}, ${CREATED})"
        ALL_PASSED=false
    else
        echo -e "  ${GREEN}OK${NC}    ${wf}: success (${SHA}, ${CREATED})"
    fi
done

echo ""
if [ "$ALL_PASSED" = "true" ]; then
    echo -e "${GREEN}All CI workflows green on ${BRANCH}. Safe to tag.${NC}"
    exit 0
else
    echo -e "${RED}CI not green on ${BRANCH}. Fix failures before tagging.${NC}"
    exit 1
fi
