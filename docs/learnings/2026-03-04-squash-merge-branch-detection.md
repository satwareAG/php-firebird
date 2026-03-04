---
description: >-
  Squash-merge creates a new commit SHA — git branch --merged and
  git log satware-main..branch cannot detect patch-level equivalence.
  Always verify with git diff before acting on "unmerged" branch warnings.
tags:
  - git
  - workflow
  - php-firebird
  - lessons-learned
last_updated: '2026-03-04'
---

# Squash-Merge Branch Detection Pitfall

## Problem

`git log satware-main..fix/transaction-mshutdown-uaf-78-79 --oneline` reported one
unmerged commit (`b68d85f`), and `git branch -vv` showed `[origin/...: gone]`.
This led to a task brief stating the fix was "NOT squash-merged into satware-main"
and needed a new PR.

## Root Cause

Squash-merge creates a **new commit SHA** on the target branch. Git tracks commit
identity, not patch content. So:

- `git log main..branch` → shows commits not reachable from main **by SHA**
- `git branch --merged` → checks if branch tip is an ancestor of main **by SHA**

Neither command detects that the *patch content* was already applied via squash-merge.

## Solution

Before acting on an "unmerged branch" warning, verify patch equivalence:

```bash
# Find the squash-merge commit on main with matching message
git log satware-main --oneline | grep -i "fix.*trans.*mshutdown\|#78\|#79"

# Confirm patches are identical (zero output = identical)
git diff <branch-commit> <main-squash-commit> -- <file>
```

In this case:
- Branch commit: `b68d85f` (Jane Alesi, 2026-03-03 10:59)
- Squash-merge on main: `5bcbc9f` (Michael Wegener, 2026-03-03 11:09, Co-authored-by: Jane Alesi)
- `git diff b68d85f 5bcbc9f` → **zero output** (patches identical)

## Fix Applied

1. Deleted stale local branch: `git branch -D fix/transaction-mshutdown-uaf-78-79`
2. Updated `NEXT_STEPS.md` to reflect correct merge status
3. No new PR needed — fix shipped in v7.1.0

## Prevention Pattern

When `NEXT_STEPS.md` or task context says "branch not merged":

1. `git log satware-main --oneline | grep <keywords>` — search by message
2. `git diff <branch-tip> <candidate-commit>` — verify patch identity
3. Only open a new PR if `git diff` shows differences

## References

- Commit `5bcbc9f` — squash-merge of fix on satware-main
- PR #81 — original merge (squash)
- Issues #78, #79 — UAF/SIGABRT in `_php_fbird_free_trans()` during MSHUTDOWN
