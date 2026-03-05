# Implementation Plan: CI Matrix — All Supported Firebird Versions

**Spec**: `spec.md`
**Branch**: `feat/096-ci-matrix-all-supported-firebird`
**Estimated effort**: 1-2 hours

---

## [Overview]

Expand the GitHub Actions CI matrix from 4 to 7 combinations by adding Firebird 4.0 server entries for PHP 8.2 and 8.4, and adding a PHP 8.3 × Firebird 4.0 entry.

The CI currently tests PHP 8.2/8.4 × FB 3.0/5.0 (4 combinations). Firebird 4.0 is actively
supported and has unique `#if FB_API_VER >= 40` code paths (batch ops, time zones, DECFLOAT,
INT128) that are never compiled or tested. PHP 8.3 is required by constitution Article V but
absent from CI. The FB 4.0 client download logic already exists in the CI workflow — it just
has no matrix entry activating it.

No C code changes are needed. All `#if FB_API_VER >= 40` guards already exist. This is purely
a CI configuration + documentation update.

---

## [Types]

No type system changes — this is a CI/YAML and documentation change only.

N/A — no C structs, PHP types, or interfaces are modified.

---

## [Files]

Three files require modification; no new files are created.

### Modified files

**`.github/workflows/ci.yml`** — Add 3 new matrix entries:
- PHP 8.2 / Firebird 4.0 (new)
- PHP 8.3 / Firebird 4.0 (new)
- PHP 8.4 / Firebird 4.0 (new)
- Update matrix comment from "4 combinations" to "7 combinations"
- Verify `fb-client-version: '4.0'` case in `Install Firebird client library` step is correct

**`AGENTS.md`** — Update matrix table in "Key Source Files" section:
- Change matrix comment from "PHP 8.2/8.4 × FB 3.0/5.0 = 4 combinations" to
  "PHP 8.2/8.3/8.4 × FB 3.0/4.0/5.0 = 7 combinations"
- Update the matrix table to show all 7 entries

**`.specify/memory/constitution.md`** — Update Article V:
- Change "PHP 8.2, 8.3, 8.4" (already listed) to confirm FB 3.0, 4.0, 5.0 are all tested
- Add explicit statement that CI matrix covers all three supported Firebird versions

**`docs/product/project-brief.md`** — Update CI matrix section:
- Update matrix description to reflect 7 combinations

### New files

None.

### Deleted files

None.

---

## [Functions]

No function changes — CI YAML and documentation only.

N/A.

---

## [Classes]

No class changes.

N/A.

---

## [Dependencies]

No new dependencies.

The Firebird 4.0 client tarball (`Firebird-4.0.6.3221-0.amd64.tar.gz`) is already referenced
in the existing `Install Firebird client library` step. No new download URLs needed.

The `firebirdsql/firebird:4` Docker image is already used in `docker-compose.yml` as the
`firebird40` service. The CI uses `firebirdsql/firebird:4` as the service image.

---

## [Testing]

Validation is the CI run itself — all 7 matrix jobs must pass.

- Push the feature branch and open a PR against `satware-main`
- Verify all 7 CI jobs appear in the Actions tab
- Verify all 7 jobs pass (build + PHPT tests)
- No new `.phpt` tests needed (this is infrastructure, not C code)

---

## [Implementation Order]

Changes are applied in dependency order: branch first, CI second, docs third.

1. Create feature branch: `git checkout -b feat/096-ci-matrix-all-supported-firebird`
2. Edit `.github/workflows/ci.yml` — add 3 new matrix entries for FB 4.0
3. Edit `AGENTS.md` — update matrix table
4. Edit `.specify/memory/constitution.md` — update Article V
5. Edit `docs/product/project-brief.md` — update CI matrix section
6. Commit: `feat(ci): add Firebird 4.0 and PHP 8.3 to CI matrix (7 combinations)`
7. Push and open PR against `satware-main`
8. Verify all 7 CI jobs pass
9. Squash-merge PR
