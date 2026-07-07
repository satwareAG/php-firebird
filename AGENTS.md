# AGENTS.md

## About php-firebird

**php-firebird** is a modernized PHP extension providing native connectivity to Firebird databases (3.0, 4.0, 5.0+). It is a high-performance replacement for the legacy `ext/interbase`, written in C/C++17 using the modern Firebird OO API.

The project follows **Spec-Driven Development (SDD)** and targets high-quality API standards for modern PHP (8.2+).

---

## API Architecture

The extension provides three distinct layers:

| Layer | Type | Namespace / Prefix | Purpose |
|-------|------|-------------------|---------|
| **Layer 1** | Procedural | `fbird_*` | Low-level, high-performance C API functions |
| **Layer 2** | OOP | `Firebird\*` | Native PHP classes (`Connection`, `Transaction`, etc.) |
| **Layer 3** | PDO | `pdo_fbird` | Separate extension providing `fbird:` DSN prefix |

---

## Integration Standards

### SDD/TDD Workflow
1. **Spec**: Update `specs/` before changing code.
2. **Test**: Write `.phpt` (native) or `phpunit` (OOP) tests first.
3. **Pillars**: Memory safety (ASAN), type safety (C++17/PHP 8.2+).

### IPADP Conformance
This project targets **L3 conformance** (Discovery).
- **Metadata**: `specs/metadata.json`
- **Linkages**: Relates to `doctrine-firebird-driver` (downstream).

---

## Technical Context for Agents

- **Build System**: `phpize` + `configure`.
- **Primary Source**: `firebird.c` (Zend API entry point).
- **Modern Logic**: `firebird_utils.cpp` / `fbird_classes.c`.
- **PDO Driver**: `pdo_fbird/`.

Agents MUST maintain consistency between the three layers when introducing new features.

---

## Release Procedure

### Pre-Tag Checklist

1. **Verify CI green**: `bash scripts/verify-ci-green.sh` — all 5 workflows must pass (ci, code-quality, sanitizers, coverage, doctrine-downstream)
2. **Run local test matrix**: `bash scripts/test_matrix.sh` — all 16 containers must pass (PHP 8.2-8.5 × FB 3.0/4.0/5.0/2.5)
3. **Run amicron-platform downstream**: `DBPASS=masterkey bash scripts/test-amicron-platform.sh` — verify customer stack
3. **Verify version stamps**: `bash scripts/check-version-stamps.sh`
4. **Update CHANGELOG.md** with release notes
5. **Bump version**: Update `VERSION.txt` and `stubs/*.php` `@version` tags

### Tagging Rules

- **NEVER force-push a tag.** Once `git push origin v*` succeeds, the tag is immutable.
- If a release is broken: delete tag + release entirely, fix, re-tag with a NEW tag (e.g., `v11.0.2`).
- Tag format: `vMAJOR.MINOR.PATCH` (e.g., `v11.0.1`)

### Release Pipeline Architecture

```
Tag push v*
  |
  +-- CI, Code Quality, Sanitizers, Coverage (validation, already passed)
  |
  +-- Release (Linux)   -> prepare -> build -> test-bundles -> upload (draft:true)
  +-- Release (macOS)   -> prepare -> build -> test-bundles -> upload (draft:true)
  +-- Release (Windows) -> get-matrix -> build -> upload (draft:true)
  |
  +-- Split Stubs       -> split stubs/ to satwareAG/php-firebird-stubs
  |
  +-- Publish Release   -> waits for all 3 platform workflows
                           -> generates SLSA attestations
                           -> generates checksums
                           -> publishes release (draft:false)
```

**Key rules**:
- Platform workflows upload with `draft: true` via `softprops/action-gh-release@v3`
- The `publish-release.yml` workflow is the ONLY one that flips `draft: false`
- SLSA attestations are generated centrally in `publish-release.yml`
- The split-stubs workflow deletes any pre-existing tag in the stubs repo before re-creating
