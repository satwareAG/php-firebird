# Implementation Plan: Comment Cleanup

**Status:** ✅ Completed (2026-01-03)

[Overview]
Remove all noise comments from php-firebird source files following "Good Code Needs No Documentation" paradigm.

This plan targets ~220 inline comments across 11 files. The cleanup removes section dividers, "Step X.Y:" development artifacts, "C++17:" feature annotations, "Phase X:" markers, and comments that restate what code does. Valuable "why" comments explaining technical rationale will be preserved. Work follows Baby Steps methodology with `scripts/qa.sh --mode fast` validation after each file.

[Types]
No type changes required.

This is a pure refactoring task removing comments only.

[Files]
Source files to clean, ordered by comment count.

**Files to modify (in priority order):**
1. `firebird_utils.cpp` - 166 comments remaining (section dividers, step/phase/C++17 markers)
2. `firebird_utils_internal.h` - 12 comments
3. `fbird_metadata.c` - 12 comments  
4. `firebird.c` - 11 comments
5. `fbird_result.c` - 7 comments
6. `fbird_blobs.c` - 7 comments
7. `php_fbird_includes.h` - 3 comments
8. `fbird_query_exec.c` - 2 comments
9. `firebird_utils.h` - 1 comment
10. `fbird_service.c` - 1 comment
11. `fbird_query_prepare.c` - 1 comment

**Patterns to remove:**
- `// =======...=======` section dividers
- `// Step X.Y:` development artifacts
- `// Phase X:` milestone markers
- `// C++17:` feature annotations
- Comments that merely restate what code does

**Patterns to KEEP:**
- Comments explaining "why" (technical rationale)
- Comments about Firebird-specific behavior
- API compatibility notes
- NOLINT directives

[Functions]
No function changes required.

This is a pure comment removal refactoring.

[Classes]
No class changes required.

This is a pure comment removal refactoring.

[Dependencies]
No dependency changes required.

[Testing]
Run `scripts/qa.sh --mode fast` after each file cleanup.

**Validation steps after each commit:**
1. `scripts/qa.sh --mode fast` - Static analysis (clang-tidy, cppcheck, PHPStan)
2. `make -j4` - Verify clean build
3. `git diff --stat` to confirm only comments removed

**Final validation:**
- `scripts/qa.sh --mode standard` - Full test suite after all changes

[Implementation Order]
Process files in descending order of comment count, committing after each.

1. **firebird_utils.cpp** (166 comments) - Remove section dividers, step/phase/C++17 markers
2. **Commit + qa.sh --mode fast**
3. **firebird_utils_internal.h** (12 comments) - Review and clean header
4. **Commit + qa.sh --mode fast**
5. **fbird_metadata.c** (12 comments) - Clean inline comments
6. **Commit + qa.sh --mode fast**
7. **firebird.c** (11 comments) - Clean inline comments
8. **Commit + qa.sh --mode fast**
9. **fbird_result.c** (7 comments) - Clean inline comments
10. **Commit + qa.sh --mode fast**
11. **fbird_blobs.c** (7 comments) - Clean inline comments
12. **Commit + qa.sh --mode fast**
13. **Remaining 5 files** (php_fbird_includes.h, fbird_query_exec.c, firebird_utils.h, fbird_service.c, fbird_query_prepare.c) - Batch cleanup
14. **Final commit + qa.sh --mode standard**
