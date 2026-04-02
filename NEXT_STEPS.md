# Next Steps - php-firebird

**Last Updated**: 2026-04-02

## Immediate (v10.6.x)

- [x] v10.6.1 released - musl bundle collision, zend_get_type_by_const portability, Windows release fix
- [ ] Clean up `docker/musl-debug/` scratch directory (untracked) - cosmetic housekeeping

## v11.0 Modernization (Next Milestone)

Tracked via GitHub issues #176-#179:

- [ ] #176 - Modernization planning
- [ ] #177 - Architecture improvements
- [ ] #178 - API surface review
- [ ] #179 - Documentation overhaul

## Technical Debt

- [ ] macOS x86_64 builds disabled (macos-13 runner deprecated) - evaluate macos-15 x86_64 when available
- [ ] Coverage threshold still at 54% - target 65%+ with v11.0 test improvements
