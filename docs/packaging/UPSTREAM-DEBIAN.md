# Upstream Debian Submission - Tracking

Status of the effort to get `php-firebird` into the official Debian
archive (which then auto-syncs to Ubuntu). Work items live in issues on
the `v13.4.0 - Upstream Debian Submission` milestone.

## ITP bug

| Field | Value |
|-------|-------|
| Bug | **#1152131** |
| Title | `ITP: php-firebird -- Modern Firebird database extension for PHP` |
| URL | <https://bugs.debian.org/1152131> |
| Filed | 2026-10-25, by Michael Wegener `<mw@satware.com>` |
| Owner | satware AG `<info@satware.com>` |
| License declared | `PHP-3.01` (per the Debian ftp-master PHP-license position; the mandatory "This product includes PHP software" statement lives in `packaging/debian/copyright`) |
| CC | debian-devel@lists.debian.org |

## Sequence (per the milestone)

1. ~~File ITP with Debian wnpp~~ - done 2026-10-25, bug #1152131 (issue #506).
2. Debian Policy compliance polish (#507) - in progress; gates the actual
   upload, not the ITP (Debian Developers Reference 5.1).
3. salsa.debian.org git packaging repo `php-team/php-firebird.git` (#508) -
   only after the ITP exists.
4. Sponsor search / mentor review: watch the bug for responses; a sponsor
   is required for the first upload (no debian.org account needed for the
   sponsorship route).

## Notes

- Per-PHP-version binary packages (`php8.2-firebird` ... `php8.5-firebird`)
  conflict with / replace the obsolete `php8.X-interbase` packages.
- ITP filing order and wnpp mechanics verified against the Debian
  Developers Reference during the 2026-10-25 review window (council run 2
  record on issue #506).
- Update this file when the bug receives a maintainer response, a sponsor,
  or a `Closes:` in the first uploaded changelog (which closes the ITP).
