# EOD Workflow 2: Knowledge & Documentation

**Purpose**: Preserve context, update docs, and ensure smooth resume/handoff for the next session.

---

## Checklist

- [ ] `CHANGELOG.md` updated with today's changes
- [ ] `NEXT_STEPS.md` reflects current state and open items
- [ ] ADRs created for any significant architectural decisions
- [ ] README updated if public API or setup changed
- [ ] Learnings captured (`.specify/memory/` or inline comments)
- [ ] Workflows updated if new patterns were discovered

---

## Steps

### 1. Update CHANGELOG.md

Follow [Keep a Changelog](https://keepachangelog.com) format:

```markdown
## [Unreleased]

### Added
- ...

### Changed
- ...

### Fixed
- ...
```

Move entries to a versioned section on release.

### 2. Update NEXT_STEPS.md

Ensure the file reflects:
- **Last updated** date
- **Current version** and branch
- Completed items marked ✅
- New open items added with context
- Validation baseline updated if tests were run

### 3. Capture Architectural Decisions

If a significant decision was made today (new pattern, API change, dependency choice):

```bash
# Create ADR
mkdir -p .specify/specs
cat > .specify/specs/adr-$(date +%Y%m%d)-{topic}.md << 'EOF'
# ADR: {Title}
Date: $(date +%Y-%m-%d)
Status: Accepted

## Context
...

## Decision
...

## Consequences
...
EOF
```

### 4. Update README if Needed

Check if any of the following changed:
- Public PHP API (`fbird_*` functions)
- Build/install instructions
- Docker setup
- CI matrix

### 5. Capture Learnings

Add to `.specify/memory/` any reusable patterns, gotchas, or decisions:

```bash
# Append to learnings log
echo "$(date +%Y-%m-%d): <learning>" >> .specify/memory/learnings.md
```

### 6. Update Workflows

If today's work revealed a better process, update the relevant workflow file in `Workflows/`.

---

## References

- [Keep a Changelog](https://keepachangelog.com)
- [Architecture Decision Records](https://adr.github.io)
- [Netflix Full Cycle Developers](https://netflixtechblog.com/full-cycle-developers-at-netflix-a08c31f83249)
