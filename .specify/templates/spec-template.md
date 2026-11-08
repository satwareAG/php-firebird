# Spec: {Feature Name}

**Issue**: #{issue-number}
**Branch**: `{branch-name}`
**Date**: {YYYY-MM-DD}
**Status**: Draft | Review | Approved

---

## Intent

> One paragraph: WHAT this change achieves and WHY it is needed for v7.0.0.

---

## Background

> Context: current state, problem being solved, relationship to other issues.

---

## User Stories / Acceptance Goals

### Goal 1: {Primary Goal}

**Given** {precondition}
**When** {action}
**Then** {expected outcome}

**Success Criteria**:
- [ ] {measurable criterion 1}
- [ ] {measurable criterion 2}

### Goal 2: {Secondary Goal}

**Given** {precondition}
**When** {action}
**Then** {expected outcome}

---

## Abuse & Misuse Cases *(mandatory)*

<!--
  ACTION REQUIRED: Fill in negative scenarios for every required category
  below. A misuse case that is not written here - or not mapped to a
  defending requirement - ships as green code: no review or CI gate can
  enforce a requirement that was never stated. If a category genuinely does
  not apply, state why explicitly.
-->

Write each case as a Given/When/Then **negative** scenario and map it to the
requirement that defends against it (`FR-XXX`, or the named Success
Criterion in a Goal above when this spec does not number functional
requirements, e.g. `-> Goal 1 SC-2`).

**Required categories:**

1. **Unauthorized access** - cross-user / cross-tenant object access
   (IDOR/BOLA): what happens when user A requests user B's resources by
   manipulating identifiers?
2. **Input & injection** - malicious input through every user-controlled
   surface: what happens when a field receives crafted payloads (SQL,
   script, path traversal, command)?
3. **Rate & resource abuse** - unbounded calls or bulk operations: what
   happens when one client issues very many requests or requests very
   large resources?

These categories target OWASP Top 10 (2021) A04:2021, Insecure Design: the
requirement mapped to each case is the design-level defense that no review
or CI gate can substitute.

**Format:**

- **AMR-001**: Given [attacker authenticated as user A], **When** [they
  request resource `id` owned by user B], **Then** [system MUST deny access
  without leaking existence, and log the attempt] -> FR-XXX

---

## Out of Scope

> Explicitly list what this spec does NOT cover.

- {excluded item 1}
- {excluded item 2}

---

## Constitution Alignment

| Article | Requirement | How Met |
|---------|-------------|---------|
| II: Test-First | Tests before implementation | {how} |
| III: Memory Safety | ASan/UBSan clean | {how} |
| VII: Coverage Gate | ≥80% on touched files | {how} |

---

## Dependencies

- **Requires**: {other issues/PRs that must be merged first}
- **Blocks**: {issues that cannot start until this merges}

---

## Open Questions

> Mark any ambiguities. Resolve before approving.

- [ ] {question 1} — Recommended: {answer}
