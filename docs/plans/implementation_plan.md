# Implementation Plan

Phase 0: Remove dead `firebird_legacy_wrappers.*` code from the php-firebird C extension build.

This phase removes two files (`firebird_legacy_wrappers.h` and `firebird_legacy_wrappers.c`) and their build reference in `config.m4`. Investigation confirmed zero callers of any function declared in the legacy wrappers header across all 19 .c files (including PDO driver). The PDO driver uses identically-named functions declared in `firebird_utils.h`, not from this header. Removing these files reduces build surface by ~500 LOC with zero runtime impact. No PHP API changes, no stub changes, no test changes needed.

[Types]
No type system changes in this phase. All affected symbols are internal C functions with `void *` or `int` return types, never exposed to PHP.

[Files]
Two source files deleted, one build config modified.

- **DELETE** `firebird_legacy_wrappers.h` - Header declaring ~40 unused wrapper functions (fbb_*, fbsvc_*, fbe_*, fbm_*, fbs_*, fbxpb_*, fbu_*_tz, fbc_get_attachment, fbt_get_handle). Zero inclusion from any .c file.
- **DELETE** `firebird_legacy_wrappers.c` - Implementation of the above wrappers. Zero callers from any .c file.
- **DELETE** `firebird_legacy_wrappers.loT` - Libtool object tracker for the deleted .c file.
- **MODIFY** `config.m4` - Remove `firebird_legacy_wrappers.lo` from the `PHP_FIREBIRD_SOURCES` variable assignment. The line currently reads something like `PHP_FIREBIRD_SOURCES="firebird.c fbird_batch.c ... firebird_legacy_wrappers.c ..."`. Remove only the `firebird_legacy_wrappers.lo` token.

[Functions]
No new or modified functions. Functions removed are internal-only, never PHP-exported.

- **REMOVE** All ~40 functions declared in `firebird_legacy_wrappers.h` (file deleted). No migration needed - zero callers.
- **REMOVE** All ~40 function implementations in `firebird_legacy_wrappers.c` (file deleted).

[Classes]
No class changes. This is a C extension; PHP classes defined in `fbird_classes.c` are unaffected.

[Dependencies]
No dependency changes. The deleted files only reference Firebird client headers (ibase.h) which remain included by other files.

[Testing]
Run existing `.phpt` test suite via Docker to confirm zero behavioral change.

- **Test command**: `docker compose -f docker/docker-compose.yml run --rm php83-dev /ext/scripts/test.sh`
- **Expected result**: All tests pass identically to pre-deletion baseline.
- **No new tests needed**: Removing dead code cannot introduce regressions.
- **QA step**: Compare test output before and after deletion (diff expected to be empty).

[Implementation Order]
Three sequential baby steps executed by separate sub-agents for developer, tester, and QA roles.

1. **Developer agent**: Delete `firebird_legacy_wrappers.h`, `firebird_legacy_wrappers.c`, `firebird_legacy_wrappers.loT`. Edit `config.m4` to remove `firebird_legacy_wrappers.lo` from `PHP_FIREBIRD_SOURCES`. Commit with message `refactor: remove dead firebird_legacy_wrappers code`.
2. **Tester agent**: Run `docker compose -f docker/docker-compose.yml run --rm php83-dev /ext/scripts/test.sh`. Capture full output to `/tmp/phase0-test-output.txt`. Report pass/fail count.
3. **QA agent**: Verify commit is clean (no extra files staged), verify `grep -r "firebird_legacy_wrappers" --include="*.c" --include="*.h" --include="*.m4" .` returns zero results, verify test output shows no failures vs baseline.