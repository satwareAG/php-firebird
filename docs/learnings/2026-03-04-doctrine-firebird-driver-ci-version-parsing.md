---
description: >-
  Firebird Docker images return plain numeric version strings that broke
  doctrine-firebird-driver CI — root cause, fix pattern, and prevention.
tags:
  - ci-cd
  - doctrine-firebird-driver
  - firebird
  - php
  - version-parsing
last_updated: '2026-03-04'
---

# 2026-03-04: Firebird Docker Images Return Plain Version Strings

## Problem

All 6 CI matrix jobs (PHP 8.3/8.4 × Firebird 3.0/4.0/5.0) on `doctrine-firebird-driver`
`3.0.x` branch were failing with 598+ errors per job:

```text
Doctrine\DBAL\Exception: Invalid platform version "5.0.3.1683" specified.
The platform version has to be specified in the format:
"LI|WI-V<major_version>.<minor_version>.<patch_version>.<build_version>".
```

## Root Cause

`FirebirdDriver::createDatabasePlatformForVersion()` used a regex that only matched
the legacy Firebird version string format:

```php
// Only matched: "LI-V3.0.13.33818", "WI-V4.0.5.3116", "LI-T3.0.0.29316"
preg_match('/^(LI|WI)-([VT])(?P<major>\d+).../', $version, $versionParts)
```

Newer `firebirdsql/firebird` Docker images (v3, v4, v5) return **plain numeric** version
strings without the `LI|WI-V` prefix:

| Docker image | Version string returned |
|---|---|
| `firebirdsql/firebird:3` | `"3.0.13.33818"` |
| `firebirdsql/firebird:4` | `"4.0.5.3116"` |
| `firebirdsql/firebird:5` | `"5.0.3.1683"` |

The legacy format was used by older Firebird installations (pre-Docker era). The Docker
images from `firebirdsql/firebird` use a different version reporting format.

## Fix

Extended the version parser to accept both formats with an `elseif` branch:

```php
// Accept legacy format: "LI-V3.0.13.33818", "WI-V4.0.5.3116"
if (preg_match('/^(LI|WI)-([VT])(?P<major>\d+).../', $version, $versionParts) === 1) {
    // extract major/minor/patch/build
} elseif (
    // Accept plain numeric format: "5.0.3.1683", "3.0.13.33818"
    preg_match('/^(?P<major>\d+)(?:\.(?P<minor>\d+)(?:\.(?P<patch>\d+)(?:\.(?P<build>\d+))?)?)?$/',
        $version, $versionParts) === 1
) {
    // extract major/minor/patch/build
} else {
    throw Exception::invalidPlatformVersionSpecified(...);
}
```

## Prevention

- When writing version parsers for database drivers, always test against Docker image
  version strings — they often differ from native installation formats.
- Add test cases for both legacy and Docker version string formats in
  `VersionAwarePlatformDriverTest`.
- The `firebirdsql/firebird` Docker images are the standard CI testing environment;
  their version format should be treated as the primary format going forward.

## References

- PR #83: https://github.com/satwareAG/doctrine-firebird-driver/pull/83
- Issue #82: https://github.com/satwareAG/doctrine-firebird-driver/issues/82
- Fix: `src/Driver/FirebirdDriver.php` — `createDatabasePlatformForVersion()`
- Tests: `tests/Test/Driver/VersionAwarePlatformDriverTest.php`
