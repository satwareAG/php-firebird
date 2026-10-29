---
description: >-
  v13.3.0 build-identity channel: fail-loud interpreter/ABI/asset-identity
  verification for every Windows smoke and release publish (#688-P1, council
  run 5 six-point program). The extension emits its build identity
  (PHP_VERSION_ID, ZEND_MODULE_API_NO, artifact sha256) at load; smoke and
  publish jobs join on it; PR-time negative controls prove the guard can fail.
tags: [release-harness, windows, supply-chain, ci, v13.3, build-identity]
priority: 1
---

# Spec: v13.3.0 Build-Identity Channel (#688-P1)

## Issue
#688 P1 (this spec formalizes the program's first point; #686 is the seed incident)

## Branch
`feat/688-p1-build-identity` (not yet cut)

## Date
2026-10-29 (drafted via /spec.specify per the session objective; pre-workload council verdicts recorded on #700)

## Status
DRAFT - awaiting the #703-#708 merge wave (this spec touches release-windows.yml and the smoke scripts those PRs stabilize; cut the branch after they land)

---

## Intent

The v13.2.11/v13.2.12 deployment review proved a class of failure where a
Windows artifact built by the WRONG interpreter (or a smoke run against a
stale/wrong extension) reached the release lane and passed every value-level
check: nothing in the pipeline compared WHO the binary claims to be with WHAT
is being smoked/published. P1 closes that with a build-identity channel:

1. The extension emits its identity at module load (PHP_VERSION_ID,
   ZEND_MODULE_API_NO, sha256 of the built binary) into the job log.
2. Every consumer (smoke job, publish job) fails loudly unless the identity it
   observes matches the identity recorded for the artifact it received.
3. PR-time negative controls prove the guard fails when Setup PHP is removed
   or a wrong-interpreter PHP is injected (council run 5: "a named test is
   still a string that fails no build" - the controls are the answer).

## User Scenarios (BDD)

- **Given** the release lane built `php_firebird.dll` with PHP 8.4.11 (ZEND
  MODULE API 20240924), **when** the pre-publish-smoke job provisions PHP
  8.3.x on the runner, **then** the smoke job FAILS at the interpreter-guard
  step with the observed identity in the error (no smoke runs against the
  wrong interpreter).
- **Given** a smoke leg passes, **when** the publish job joins evidence,
  **then** the publish refuses unless the sha256 of the asset it downloads
  equals the sha256 the smoke legs recorded for the same asset name.
- **Given** a PR modifies the smoke harness and REMOVES the Setup PHP step
  (or swaps its version), **when** the PR-time negative-control workflow runs,
  **then** the seeded wrong-interpreter smoke is observed to FAIL (proving
  the guard fires) and the harness change is flagged.

## Requirements

- **FR-001**: `php_firebird.dll` MUST emit `IDENTITY php=<PHP_VERSION_ID>
  api=<ZEND_MODULE_API_NO> sha256=<binary-digest>` as its first log line at
  MINSHOT (module init) when `FBIRD_IDENTITY_LOG=1` is set in the environment.
- **FR-002**: every Windows smoke/publish job MUST run an interpreter-guard
  step BEFORE any smoke: it parses the emitted identity, compares
  PHP_VERSION_ID + ZEND_MODULE_API_NO against the job's matrix inputs, and
  fails with both observed and expected values on mismatch.
- **FR-003**: the publish job MUST compare the downloaded asset's sha256
  against the identity record produced by the build job (join key: asset
  name) and fail on mismatch.
- **FR-004**: a PR-time negative-control job MUST (a) seed a wrong-interpreter
  PHP and assert the guard fails, (b) remove the identity-log env and assert
  the guard fails closed, (c) mutate a copied asset byte and assert the
  sha256 join fails.
- **FR-005**: the identity channel MUST NOT change default runtime behavior
  without `FBIRD_IDENTITY_LOG=1` (no new output in normal operation).

### Observed Surface (consumer-visible FRs)

- FR-001's output is observed in the CI job log (the channel IS the product
  here); FR-002/003's failures are observed as failed jobs with both values
  in the error text. The observing tests are the negative controls (FR-004).

### Constraints

- **TC-001**: guard step adds < 5s to each smoke job (string compare, no
  network).
- **TC-002**: the sha256 join covers every released asset family
  (php_firebird-*, php_pdo_fbird-* per PHP version x TS/NTS x arch).
- **TC-003**: coverage of the guard logic itself >= 90% (it is a fail-closed
  security control).

## Success Criteria

- **SC-001**: negative controls (FR-004 a/b/c) each observed RED before the
  guard lands and GREEN after (per unlazy; the tranche precedent).
- **SC-002**: the 12-lane Linux matrix + Windows build/smoke lanes stay green
  with the identity logging enabled in CI only.
- **SC-003**: the next Windows tag's smoke + publish jobs carry identity-join
  evidence in their logs (observable post-merge).

## Relation to the program

- P1 of six (#688). P4's replay test supersedes #678's heuristic; P2's
  manifest oracle is the follow-up consumer of this channel's sha256 record.
- #685 (macOS pool weather) is orthogonal (P4 scope).

## Council provenance

Council run 5 (DEBATE, 4 seats, unanimous six-point program) + block-4/6
councils re-confirmed P1 priority after the PR wave. The build-identity
channel design surface was flagged by the block-2 council as the reason P1
was not in-session executable ("no failing-capable oracle without a
build-identity channel") - this spec IS that channel's contract.
