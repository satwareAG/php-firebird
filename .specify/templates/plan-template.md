# Plan: {Feature Name}

**Issue**: #{issue-number}
**Spec**: `.specify/specs/{dir}/spec.md`
**Date**: {YYYY-MM-DD}
**Status**: Draft | Approved

---

## Constitution Check

> Verify spec satisfies all applicable articles before planning.

| Article | Gate | Status |
|---------|------|--------|
| I: C Extension First | New files in C, registered in config.m4+config.w32 | ⬜ |
| II: Test-First | Failing tests written before implementation | ⬜ |
| III: Memory Safety | ASan/UBSan/Valgrind plan defined | ⬜ |
| VI: Atomic Commits | Each commit ≤200 LOC, single concern | ⬜ |
| VII: Coverage Gate | ≥80% on all touched files | ⬜ |
| VIII: Docker Testing | Tests run in docker/ environment | ⬜ |

---

## Technical Approach

> HOW the spec will be implemented. Choose 1-3 approaches, justify the chosen one.

### Chosen Approach

{Description of the technical solution}

### Alternatives Considered

| Approach | Pros | Cons | Decision |
|----------|------|------|----------|
| {A} | {pros} | {cons} | Rejected |
| {B} | {pros} | {cons} | **Chosen** |

---

## Files Affected

| File | Change Type | Description |
|------|-------------|-------------|
| `{file.c}` | Create / Modify / Delete | {what changes} |
| `tests/{test}.phpt` | Create | {what is tested} |
| `config.m4` | Modify | {if new .c files added} |

---

## Implementation Sequence

> Ordered list — each step is one atomic commit.

1. **Step 1**: {description} → `{commit message}`
2. **Step 2**: {description} → `{commit message}`
3. **Step 3**: {description} → `{commit message}`

---

## Test Plan

| Test File | Coverage Target | What It Tests |
|-----------|----------------|---------------|
| `tests/coverage/{name}.phpt` | {function names} | {scenarios} |

### Sanitizer Validation

```bash
# Run after each implementation step
docker compose run --rm php83-dev /ext/scripts/run-sanitizer.sh
```

---

## Risks

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| {risk} | Low/Med/High | Low/Med/High | {mitigation} |

---

## Definition of Done

- [ ] All tasks in `tasks.md` completed
- [ ] Tests pass in Docker (`make test`)
- [ ] Coverage ≥80% on touched files (lcov)
- [ ] ASan + UBSan clean
- [ ] Valgrind: no definite leaks
- [ ] PR approved and merged to `satware-main`
