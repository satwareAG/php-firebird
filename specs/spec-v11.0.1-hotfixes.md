---
description: >-
  v11.0.1 hotfixes: README version badge/section, broken EVENT_TIMEOUT_RFC.md link,
  fbird_delete_user arginfo regression (3 required vs 2 accepted), and stale documentation.
tags: [hotfix, regression, documentation, arginfo, v11.0.1]
priority: 0
---

> **Status: IN REVIEW (2026-04-20)** — HF-1/HF-2 already landed on `satware-main`;
> HF-3 arginfo regression fixed on `hotfix/v11.0.1`; HF-4 doc updates applied.
> v11.0.1 patch targeting QA audit findings D1, D2, D3, C1, D4, D5, D7.

# Spec: v11.0.1 Hotfixes

## Goal

Ship a minimal patch release that corrects post-release defects discovered by the
2026-04-09 QA audit. All four P0 findings must be resolved before any further
development releases. This spec does not introduce new features or breaking changes.

## Version

`v11.0.1` — patch release off `satware-main`

## Priority

**P0 — Critical** (release quality defects visible to all users)

---

## Success Criteria

### HF-1: README version badge and "Current Version" section updated

- [x] `README.md:6` badge URL updated from `version-10.6.2-blue.svg` to `version-11.0.0-blue.svg`
- [x] `README.md:933` section header updated to reference v11.0.0
- [x] `README.md:962` wget download URL updated to v11.0.0 tarball
- [x] `README.md:966` tarball filename updated to v11.0.0
- [x] No remaining `10.6.2` version references in README.md (verified by `grep -n 10.6.2 README.md`)

### HF-2: Broken link to EVENT_TIMEOUT_RFC.md removed or replaced

- [x] `README.md:929` link to `docs/development/EVENT_TIMEOUT_RFC.md` is resolved (replaced with link to `docs/OO_WRAPPER_IMPLEMENTATION.md`)
- [x] Either the file exists at that path OR the link is replaced with a note/redirect
- [x] All internal `[*](docs/*.md)` links in README verified
- [x] No 404-producing local links remain in README.md

### HF-3: `fbird_delete_user` arginfo regression fixed

- [x] `firebird.c:340` arginfo updated to require **2** parameters (not 3)
- [x] `fbird_delete_user($svc, 'username')` succeeds without "Too few arguments" error
- [x] `fbird_delete_user($svc, 'username', 'password')` — NOTE: arginfo now declares max 2 params; passing a 3rd arg will trigger a PHP warning. The C impl never consumed a password, and the stubs (`fbird.stub.php`, `firebird-stubs.php`) already declare only 2 args, so there was no documented 3-arg contract to preserve.
- [x] New test `fbird_service_delete_user_argcount.phpt` verifies arginfo via Reflection (2 required, 2 total, no `password`)
- [x] `php --re fbird | grep -A3 delete_user` shows `required: 2`

### HF-4: Stale documentation updated to reflect v11.0.0 release

- [x] `NEXT_STEPS.md` header updated from "v11.0.0 release prep in progress" to "v11.0.0 RELEASED (2026-04-09)"
- [x] `NEXT_STEPS.md` stale "Tomorrow morning" work-log section from 2026-04-08 archived
- [x] `NEXT_STEPS.md` FAILING CI notes removed (CI is green for v11.0.0)
- [x] `docs/DEPRECATION-AUDIT.md` issues #176, #177, #178 marked CLOSED/SHIPPED with v11.0.0 reference
- [x] `implementation_plan.md` (root; spec path `docs/plans/implementation_plan.md` obsolete) receives `> **Status: COMPLETE** — All items shipped in v11.0.0 (2026-04-09)` banner
- [x] `implementation_plan.md` 5 stale TBD markers resolved (Blob/Service/EventPoller struct strategies documented as shipped in Phases G/H/I)

---

## User Stories

### US-1: Correct version badge (D1/D2)
> As a developer evaluating php-firebird, when I land on the GitHub README I should
> immediately see the current v11.0.0 release badge and download instructions,
> not the outdated v10.6.2 information that implies the OOP API migration was not shipped.

**Affects**: README.md lines 6, 933, 962, 966

### US-2: Working documentation links (D3)
> As a developer following the Event API documentation links in README.md, when I click
> the link to `docs/development/EVENT_TIMEOUT_RFC.md` I should not receive a 404.
> Dead links undermine trust in the documentation quality.

**Affects**: README.md:929

### US-3: Correct `fbird_delete_user` signature (C1)
> As a PHP developer calling `fbird_delete_user($svc, 'john')`, the function must work
> with two arguments as documented. The current arginfo declares a third required
> `password` parameter that the C implementation does not actually require, causing
> every valid 2-arg call to throw "Too few arguments (2 given, 3 required)".

**Affects**: `firebird.c:340`, `fbird_service.c:175`

### US-4: Accurate documentation state (D4/D5/D7)
> As a contributor reading NEXT_STEPS.md and DEPRECATION-AUDIT.md, I should see that
> v11.0.0 has shipped and tracked issues are closed. Stale "in-progress" markers and
> open issue links create confusion about what is done vs. still pending.

**Affects**: NEXT_STEPS.md, docs/DEPRECATION-AUDIT.md, docs/plans/implementation_plan.md

---

## Implementation Notes

### Arginfo Fix (HF-3) — Most Technical Item

The regression is in `firebird.c` around line 340. The current declaration is:

```c
/* CURRENT — WRONG: requires 3 args */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_delete_user, 0, 3, _IS_BOOL, 0)
    ZEND_ARG_INFO(0, service_handle)
    ZEND_ARG_INFO(0, user_name)
    ZEND_ARG_INFO(0, password)        /* ← this param is NOT consumed by zend_parse_parameters */
ZEND_END_ARG_INFO()
```

The C implementation in `fbird_service.c:175` parses only `"zs"` (two params):
```c
if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs",
    &svc_arg, &uname, &uname_len) == FAILURE) {
    RETURN_FALSE;
}
```

The fix is to change `0, 3` → `0, 2` in the arginfo and remove the `password` entry:

```c
/* FIXED: requires 2 args */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_delete_user, 0, 2, _IS_BOOL, 0)
    ZEND_ARG_INFO(0, service_handle)
    ZEND_ARG_INFO(0, user_name)
ZEND_END_ARG_INFO()
```

> **Note**: The IBSurgeon Firebird API does not require a password for service-based user
> deletion (the service handle carries the privilege). The third parameter was a copy-paste
> error from `arginfo_fbird_add_user` which does require a password.

**Verification**:
```bash
php -r 'var_dump(fbird_delete_user($svc, "testuser"));'
# Must not throw: Fatal error: fbird_delete_user(): Argument #3 must be of type string, none given
php --re fbird | grep -A 10 "delete_user"
# Must show: Required: 2
```

---

## Affected Files

| File | Change |
|------|--------|
| `README.md` | Lines 6, 929, 933, 962, 966 — version + broken link |
| `firebird.c` | Line 340 — arginfo_fbird_delete_user min_args 3→2, remove password ZEND_ARG_INFO |
| `NEXT_STEPS.md` | Update status header, remove stale 2026-04-08 work log |
| `docs/DEPRECATION-AUDIT.md` | Mark #176/#177/#178 as CLOSED with v11.0.0 |
| `docs/plans/implementation_plan.md` | Add COMPLETE banner, resolve 5 TBD markers |
| `tests/` | Add `fbird_service_delete_user_argcount.phpt` |

---

## Risks

| Risk | Mitigation |
|------|------------|
| Third arg `password` might be used by some callers | PHP will ignore extra args; no runtime breakage |
| README badge URL format change | Test badge renders on GitHub before merge |
| EVENT_TIMEOUT_RFC.md might have valid content planned | Check git history; if planned, create stub doc |

## Release Process

1. Create branch `hotfix/v11.0.1` from `satware-main` at commit `3088145`
2. Apply all 4 hotfixes in a single commit per item for clean `git log`
3. Run full CI (ci.yml + code-quality.yml) — all must pass
4. Tag `v11.0.1`, trigger `release-linux.yml` and `release-windows.yml`
5. Update GitHub Release notes describing the arginfo regression fix prominently
