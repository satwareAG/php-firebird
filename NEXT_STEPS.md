# Next Steps - php-firebird

**Last Updated**: 2026-04-02

## Immediate (v10.6.x)

- [x] v10.6.1 released - musl bundle collision, zend_get_type_by_const portability, Windows release fix
- [x] `docker/musl-debug/` scratch directory removed (dir did not exist, confirmed 2026-04-02)

## v11.0 Modernization (Next Milestone)

Tracked via GitHub issues #175-#179 (milestone: v11.0 - Modernization, due 2026-12-31):

- [ ] #175 - [M2] Migrate all 84 procedural arginfo to typed return macros
- [ ] #176 - [M3] Resource-to-object migration for Layer 1 procedural functions (breaking change)
- [ ] #177 - [M12] Replace call_user_function() in OOP layer with direct C calls
- [ ] #178 - [L3] Remove dead FB_API_VER < 30 code paths
- [ ] #179 - [L2] Remove legacy gds32_ms fallback from config.w32

## Technical Debt

- [ ] macOS x86_64 builds disabled (macos-13 runner deprecated) - evaluate macos-15 x86_64 when available
- [ ] Coverage threshold still at 54% - target 65%+ with v11.0 test improvements
