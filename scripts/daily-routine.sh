#!/usr/bin/env bash
# daily-routine.sh — EOD protocol automation for php-firebird
# Usage: bash scripts/daily-routine.sh eod
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

eod_command() {
    echo "============================================"
    echo "  php-firebird — End of Day Protocol"
    echo "  $(date '+%Y-%m-%d %H:%M')"
    echo "============================================"
    echo ""

    # 1. Branch & last commit
    local branch
    branch="$(git -C "$REPO_ROOT" branch --show-current)"
    local last_commit
    last_commit="$(git -C "$REPO_ROOT" log --oneline -1)"
    echo "Branch : $branch"
    echo "Commit : $last_commit"
    echo ""

    # 2. Git hygiene check
    echo "--- Git Status ---"
    local dirty
    dirty="$(git -C "$REPO_ROOT" status --short)"
    if [ -n "$dirty" ]; then
        echo "⚠️  Uncommitted changes detected:"
        echo "$dirty"
        echo ""
        echo "ACTION REQUIRED: commit or stash before EOD."
    else
        echo "✅ Working directory clean"
    fi
    echo ""

    # 3. EOD Master Checklist
    echo "--- EOD Master Checklist ---"
    echo "[ ] Validation  : All tests pass and linters are clean"
    echo "[ ] Hygiene     : Temporary files deleted, stale branches pruned"
    echo "[ ] Docs        : README, CHANGELOG, and NEXT_STEPS.md updated"
    echo "[ ] Git         : All work committed and pushed to remote"
    echo "[ ] Knowledge   : Learnings captured, Workflows updated if needed"
    echo "[ ] Cleanup     : Dev services stopped, temporary files cleared"
    echo ""

    # 4. Workflow references
    echo "--- Workflow References ---"
    echo "  Workflows/eod.hygiene-git.standards.md"
    echo "  Workflows/eod.knowledge-documentation.md"
    echo "  Workflows/eod.ops-automation.md"
    echo ""

    echo "============================================"
    echo "  Review checklist above and mark items ✅"
    echo "============================================"

    # Exit non-zero if dirty so CI/scripts can detect it
    if [ -n "$dirty" ]; then
        exit 1
    fi
}

case "${1:-}" in
    eod)
        eod_command
        ;;
    *)
        echo "Usage: $0 eod"
        echo ""
        echo "Commands:"
        echo "  eod   Run the End of Day protocol checklist"
        exit 1
        ;;
esac
