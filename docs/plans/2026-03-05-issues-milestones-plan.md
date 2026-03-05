# Implementation Plan: Issues, Milestones, and Project Hygiene

**Created**: 2026-03-05
**Author**: Jane Alesi / Cline
**Scope**: php-firebird + doctrine-firebird-driver repositories

---

## [Overview]

Execute all pending project hygiene tasks across the php-firebird and doctrine-firebird-driver repositories: close completed milestones, create the v7.3.0 milestone with issues, triage and assign doctrine-firebird-driver open issues to milestones, and update NEXT_STEPS.md to reflect the current state.

The v7.2.0 release shipped on 2026-03-04. All breaking changes are merged. PR #96 (CI matrix expansion) is in flight and passing. The project now needs: (1) milestone closure for completed work, (2) a v7.3.0 milestone with actionable issues for the next release cycle, (3) doctrine-firebird-driver issue triage assigning the 12 open issues to appropriate milestones, and (4) NEXT_STEPS.md updated to reflect the new state.

The doctrine-firebird-driver has 5 Sprint milestones (1-5) that are all complete (0 open issues each, except Sprint 5 which has 1 open: issue #78 DBAL 4.x tracking). These Sprint milestones should be closed. A new `v3.11.0` milestone should be created and populated with the actionable issues.

---

## [Types]

No code type changes — this is pure project management (GitHub API calls + markdown updates).

All changes are GitHub API operations (milestones, issues, labels) and markdown file updates.

---

## [Files]

Four files require modification; no new source files are created.

### Modified files

**`NEXT_STEPS.md`** (php-firebird repo) — Full rewrite to reflect post-PR-#96 state:
- Remove completed Priority 1/2 items (already done or in flight)
- Add PR #96 status (CI passing, ready to merge)
- Add v7.3.0 milestone planning section
- Add doctrine-firebird-driver v3.11.0 milestone section

**`docs/product/project-brief.md`** (php-firebird repo) — Update "Next Development Focus":
- Replace v7.3.0 placeholder with concrete issue list
- Add doctrine-firebird-driver v3.11.0 roadmap

### GitHub API operations (not files)

**php-firebird milestones**:
- Milestone #2 (v7.2.0): already closed — verify
- Create milestone: `v7.3.0` with description and due date

**php-firebird issues to create** (for v7.3.0 milestone):
- Issue: "feat(ci): PHP 8.3 full row coverage (FB 3.0 + FB 5.0)" — extend PHP 8.3 from 1 to 3 entries
- Issue: "feat(ci): PHP 8.5 day-1 support" — add PHP 8.5 Dockerfile + CI entry
- Issue: "chore: update FB client versions in CI (3.0.12→latest, 5.0.2→5.0.3)" — pin to latest patch releases
- Issue: "docs: add NEXT_STEPS.md v7.3.0 section" — documentation

**doctrine-firebird-driver milestones**:
- Close Sprint 1 (id:1) — 0 open issues
- Close Sprint 2 (id:2) — 0 open issues
- Close Sprint 3 (id:3) — 0 open issues
- Close Sprint 4 (id:4) — 0 open issues
- Close Sprint 5 (id:5) — 1 open issue (#78, keep open, move to new milestone)
- Create milestone: `v3.11.0` — Configurable LIKE CAST Length + actionable issues
- Create milestone: `v4.0.0-planning` — DBAL 4.x forward-compatibility tracking

**doctrine-firebird-driver issue assignments** (assign to milestones):
- #20 (Release v3.11.0 - Configurable LIKE CAST Length) → v3.11.0 milestone
- #47 (Simplify test suite) → v3.11.0 milestone (now actionable)
- #44 (BLOB streaming workaround) → v3.11.0 milestone (investigate)
- #42 (fbird_escape_string) → v3.11.0 milestone
- #43 (INT128/DECFLOAT mappings) → v3.11.0 milestone
- #46 (IBatch API functional tests) → v3.11.0 milestone
- #50 (schema test deadlocks) → v3.11.0 milestone (bug)
- #51 (PHPUnit timeouts) → v3.11.0 milestone
- #53 (phpunit.sh TTY exit code) → v3.11.0 milestone
- #48 (SQLSTATE docs) → v3.11.0 milestone
- #78 (DBAL 4.x tracking) → v4.0.0-planning milestone
- #38 (Windows CI) → v4.0.0-planning milestone (blocked by php-firebird Windows DLLs)

---

## [Functions]

No function changes — all operations are GitHub API calls via `gh api`.

**GitHub API patterns used**:
```bash
# Close a milestone
gh api --method PATCH repos/OWNER/REPO/milestones/NUMBER -f state=closed

# Create a milestone
gh api --method POST repos/OWNER/REPO/milestones \
  -f title="v3.11.0" \
  -f description="..." \
  -f state=open

# Assign issue to milestone
gh api --method PATCH repos/OWNER/REPO/issues/NUMBER \
  -F milestone=MILESTONE_NUMBER
```

---

## [Classes]

No class changes.

N/A.

---

## [Dependencies]

No new dependencies.

All operations use `gh api` (already installed) and standard bash.

---

## [Testing]

Validation is visual inspection of GitHub UI and API responses.

After each milestone operation:
- `gh api repos/OWNER/REPO/milestones?state=all --jq '.[] | "\(.number) [\(.state)] \(.title)"'`

After each issue assignment:
- `gh issue view NUMBER --repo OWNER/REPO --json milestone`

After NEXT_STEPS.md update:
- `cat NEXT_STEPS.md | head -50` to verify content

---

## [Implementation Order]

Operations ordered to avoid dependency conflicts: close old milestones first, create new ones, then assign issues.

1. **Verify PR #96 CI status** — confirm all 7 jobs pass before proceeding
2. **Close php-firebird v7.2.0 milestone** (already closed per API — verify)
3. **Create php-firebird v7.3.0 milestone** with description
4. **Create php-firebird issues** for v7.3.0 (PHP 8.3 full row, PHP 8.5, FB client version updates)
5. **Assign php-firebird issues** to v7.3.0 milestone
6. **Close doctrine-firebird-driver Sprint milestones 1-4** (all 0 open issues)
7. **Move issue #78** off Sprint 5 before closing Sprint 5
8. **Close doctrine-firebird-driver Sprint 5 milestone**
9. **Create doctrine-firebird-driver v3.11.0 milestone**
10. **Create doctrine-firebird-driver v4.0.0-planning milestone**
11. **Assign doctrine-firebird-driver issues** to v3.11.0 and v4.0.0-planning milestones
12. **Update NEXT_STEPS.md** to reflect new state
13. **Commit NEXT_STEPS.md** on current branch or satware-main (docs-only change)
14. **Merge PR #96** once all CI jobs pass: `gh pr merge 96 --repo satwareAG/php-firebird --squash --delete-branch`
