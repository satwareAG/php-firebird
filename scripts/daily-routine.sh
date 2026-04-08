#!/usr/bin/env bash
# daily-routine.sh — EOD protocol automation for php-firebird
# Usage: bash scripts/daily-routine.sh eod
set -euo pipefail

REPO_ROOT="${REPO_ROOT:-"$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"}"

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

morning_health_check() {
    echo "============================================"
    echo "  php-firebird — Morning Health Check"
    echo "  $(date '+%Y-%m-%d %H:%M')"
    echo "============================================"
    echo ""
    
    echo "--- Git Status ---"
    git fetch origin --quiet || echo "⚠️ Failed to fetch from origin"
    local status
    status="$(git status --short)"
    if [ -n "$status" ]; then
        echo "⚠️ Working directory is not clean:"
        echo "$status"
    else
        echo "✅ Working directory clean"
    fi
    echo ""

    echo "--- Docker Services ---"
    if command -v docker >/dev/null 2>&1; then
        docker compose ps || echo "⚠️ Cannot query docker services"
    else
        echo "⚠️ Docker is not installed or not in PATH"
    fi
    echo ""
    
    echo "--- Dependencies ---"
    echo "✅ Assumed OK (Native extension, no composer.json required at root for compilation)"
    echo ""
}

morning_full() {
    echo "============================================"
    echo "  php-firebird — Morning Protocol (15 Min)"
    echo "  $(date '+%Y-%m-%d %H:%M')"
    echo "============================================"
    echo ""
    
    morning_health_check
    
    echo "--- Phase 3: Priorities ---"
    echo "MoSCoW Top 3 Priorities:"
    echo "1. [MUST] Implement #176 Phase I (Service migration to objects)"
    echo "2. [MUST/SHOULD] Review PRs and check CI status"
    echo "3. [SHOULD] Clean up technical debt"
    echo ""
    
    echo "============================================"
    echo "  Protocol complete. Starting deep work!"
    echo "============================================"
}

case "${1:-}" in
    eod)
        eod_command
        ;;
    health)
        morning_health_check
        ;;
    morning-full|morning)
        morning_full
        ;;
    *)
        echo "Usage: $0 {eod|health|morning|morning-full}"
        echo ""
        echo "Commands:"
        echo "  eod            Run the End of Day protocol checklist"
        echo "  health         Run the morning health check"
        echo "  morning-full   Run the full 15-minute morning protocol"
        exit 1
        ;;
esac
