---
description: >-
  v13.4.0 security hardening train: closes the injection-drift and
  credential-hygiene gaps found by the full three-layer security audit
  (upstream-verified against Firebird v5.0.4/v4.0.7/v3.0.14 and php-8.5.12)
  and prioritized by the 5-seat paid council DEBATE (2026-10-31, run
  council-phpfirebird-sec-20261031). Signature-neutral P1 items ship in one
  train with #681 wire-observation tests and #680 delta adjudication; P2 pairs
  identifier tightening with the upstream-mandated 31-to-63 widening; P3
  deletes the masterkey fallback on audit evidence; P4 documents transport
  posture and the privileged-API surface.
tags: [security, v13.4, injection, credentials, pdo-conformance, doctrine, council]
priority: 1
---

# Spec: v13.4.0 Security Hardening (#728)

## Issue

The security audit (2026-10-31, evidence in #728) found internal inconsistencies
with the repo's own hardening standard: `fbird_gen_id` validates identifiers via
`is_valid_identifier()` while `fbird_list_fields()`, `fbird_meta_data()`,
`fbird_savepoint()` and `_fbird_drop_table()` interpolate names raw;
`fbird_drop_db(string)` carries a hardcoded `SYSDBA`/`masterkey` fallback;
`fbird_blob_export()` exposes URL stream wrappers. All findings were verified
against upstream release tags and converged into P1-P4 by a 5-seat council
(3 rounds, one seat lost to provider limits mid-run, SPLIT item recorded).

## User Scenarios

- US1 (P1, security maintainer): an application passes request-derived strings
  to metadata or DDL helpers; the driver never lets them alter SQL semantics.
- US2 (P1, host operator): a co-loaded script cannot exfiltrate blob data via
  URL wrappers or scrape credentials from extension staging memory.
- US3 (P2, FB5 application): 63-character Firebird 4.0+/5.0 identifiers are
  accepted by generators and savepoints; malformed names are rejected loudly.
- US4 (P3, upgrader): `fbird_drop_db()` string overload fails clearly without
  explicit credentials instead of silently trying legacy defaults.
- US5 (P1, PDO user): DSN dialect garbage errors at connect; `:name` inside a
  double-quoted identifier survives preprocessing unchanged.

## Requirements

- **FR-001**: `fbird_list_fields()` and `fbird_meta_data()` MUST bind the
  relation-name value via XSQLDA parameter markers; string interpolation of
  caller input into RDB$ metadata SQL MUST be gone; statement construction MUST
  reject truncation instead of silently cutting. (#729)
- **FR-002**: `_fbird_drop_table()` MUST emit unquoted case-folded identifiers
  for charset-safe unquoted names and quoted (doubled-quote) identifiers only
  where required; raw interpolation MUST be gone. Doctrine's unquoted
  `test_table` call site MUST stay green. (#730)
- **FR-003**: `fbird_blob_export()` MUST reject non-local (URL) stream wrappers
  with a clear error; local-path behavior MUST be unchanged. (#733)
- **FR-004**: DPB/SPB credential staging buffers in `fbird_service_attach()`,
  `FirebirdService::__construct()` and the connect path MUST be wiped after
  last use on success AND error paths. Caller-owned zval strings are documented
  residual. (#736)
- **FR-005**: PDO `fbird:` DSN `dialect` MUST be validated against {1,2,3};
  the named-param preprocessor MUST track double-quoted identifier state so
  `:name` inside delimited identifiers is never substituted. (#734)
- **FR-006**: savepoint and generator/sequence identifier handling MUST
  validate characters via `is_valid_identifier()` and MUST accept up to 63
  bytes under `FB_API_VER >= 40` (31 otherwise), matching upstream
  `METADATA_IDENTIFIER_CHAR_LEN` (v3.0.14=31, v5.0.4=63). (#732)
- **FR-007**: `fbird_drop_db()` string overload MUST NOT fall back to hardcoded
  credentials; omitted credentials MUST fail with a clear error. Resource and
  object paths are out of scope here (v14 privileged-surface item, #728). (#731)
- **FR-008**: docs/SECURITY.md MUST document transport posture (SRP default,
  WireCrypt server default Required, trusted auth = Win_Sspi/Windows-only) and
  the privileged-API surface with least-privilege + disable_functions guidance;
  no INI knob in v13.x. (#735)

### Observed Surface (consumer-visible FRs)

Every FR above changes what consumer input is accepted or what the wire sees.
Invariant-Gate Contracts (executable paths, verified to exist and run in their
declared lane by CI):

- FR-001: observer `tests/security/metadata_injection.phpt` (new; lane
  php84-fb3-dev via scripts/test_matrix.sh); rejecting fixture = quote-breakout
  and oversized table names; choke point = the bound-parameter prepare in
  fbird_metadata.c; gate = scripts/gates/issue-681-zpp-wire-gate.sh in CI
  (code-quality.yml) because the change touches the zpp/DPB-adjacent surface.
- FR-002: observer `tests/security/drop_table_quoting.phpt` (new; lane
  php84-fb3-dev + doctrine-firebird-driver suite); rejecting fixture =
  injection-shaped and lowercase unquoted names; RDB$RELATIONS inventory before
  and after is the observed surface.
- FR-003: observer `tests/security/blob_export_wrappers.phpt` (new; lane
  php84-fb3-dev); rejecting fixtures = `ftp://` and `php://output` destinations.
- FR-005: observer `tests/pdo_fbird/dsn_dialect_validation.phpt` and
  `tests/pdo_fbird/quoted_identifier_params.phpt` (new; lane php85-fb5-dev);
  rejecting fixtures = `dialect=99` DSN and `"my:col"` identifier SQL.
- FR-006: observer `tests/security/identifier_length.phpt` (new; lanes
  php84-fb3-dev AND php84-fb5-dev to prove the FB_API_VER split); rejecting
  fixture = 32-63 byte name under FB3 (rejected) vs FB5 (accepted).
- FR-007: observer `tests/security/drop_db_no_fallback.phpt` (new; lane
  php84-fb3-dev); rejecting fixture = string-mode call without credentials.
- All: `scripts/delta-allowlist.txt` entries (FR-001, FR-002, FR-003, FR-005,
  FR-006 tightening+widening, FR-007) adjudicated against the previous release
  tag via `scripts/test-delta.sh`; `scripts/branch-provenance.sh main` gate on
  the work branch; upstream facts pinned to tags (v5.0.4 constants.h:66,
  config.cpp:615; php-8.5.12 md5.h, php_streams.h).

## Key Entities

- Identifier contract: unquoted (uppercase-folded, charset-safe) vs quoted
  (doubled embedded quotes) vs invalid (rejected). One helper:
  `is_valid_identifier()` extended with the version-aware length gate.
- Privileged-API surface: service attach, backup/restore/maintain, user admin,
  kill attachment, force drop, drop db - documented, gated only by docs in
  v13.x; build-time/packaging gating deferred to v14 (#728).

## Open Questions

- SPLIT (unresolved, on record): priority of #593-596 ASAN-only latents vs
  injection-first. Flip condition: any of them reproducing as release-build
  memory corruption reachable from userland/server input jumps the P1 queue.

## Downstream Coordination (IPADP)

- satwareAG/doctrine-firebird-driver: confirm masterkey-fallback independence
  (audit says zero callers), case-fold contract for `fbird_drop_table_force`
  callers, security focus in their specs.
- satwareAG/pdo-fbird: FR-005 lands in-tree first; PIE split sync gated on it.
- satwareAG/php-firebird-stubs: phpdoc contract notes post-FR-007 (credentials
  required for the string overload). Zero signature drift in v13.4.0.
- Private consumers: redacted per specs/metadata.json; coordinated via #728.

## Assumptions & Handoffs

- Assumption: upstream tags v5.0.4/v4.0.7/v3.0.14 and php-8.5.12 remain the
  support matrix constants for this train (verified 2026-10-31).
- Next Step: `/spec.plan` per FR workstream; implementation issues #729-#736
  carry the task breakdown; TDD red first per repo doctrine.
