# Next Steps - php-firebird

**Last Updated**: 2026-04-07 (SOD spec audit)

## Immediate (v10.6.x)

- [x] v10.6.1 released - musl bundle collision, zend_get_type_by_const portability, Windows release fix
- [x] `docker/musl-debug/` scratch directory removed (dir did not exist, confirmed 2026-04-02)
- [x] v10.6.2 tagged - current stable release on `v10.6.x` branch
- [x] `doctrine-firebird-driver` issues #105 and #106 created and amended - FB 2.5-5.0 compatibility story, type mapping validation, PDO clarification, CI modernization (satwareAG/doctrine-firebird-driver team to action)
- [x] v10.6.2 published - all release pipelines green, 100+ assets uploaded, #213 closed (2026-04-07)

## v11.0 Modernization (Next Milestone)

Tracked via GitHub issues #175-#179 (milestone: v11.0 - Modernization, due 2026-12-31):

- [ ] #175 - [M2] Migrate all 84 procedural arginfo to typed return macros
- [ ] #176 - [M3] Resource-to-object migration for Layer 1 procedural functions (breaking change)
- [ ] #177 - [M12] Replace call_user_function() in OOP layer with direct C calls
- [ ] #178 - [L3] Remove dead FB_API_VER < 30 code paths
- [ ] #179 - [L2] Remove legacy gds32_ms fallback from config.w32

## Open Implementation Specs

All previously open specs confirmed RELEASED and closed (2026-04-07 audit):

- [x] `spec-v10.3.7-security.md`: All criteria met - shipped in v10.3.7 (commit 6c937a2)
- [x] `spec-v10.4-build-hardening.md`: All criteria met - shipped in v10.4.x (commit 99c7c0f)
- [x] `spec-v10.4-supply-chain.md`: All criteria met - SLSA, SBOM, Dependabot, version stamps shipped

## Technical Debt

- [ ] macOS x86_64 builds disabled (macos-13 runner deprecated) - evaluate macos-15 x86_64 when available
- [ ] Coverage threshold still at 54% - target 65%+ with v11.0 test improvements

## 2026-04-07 - v10.6.2 Published

Release published live: https://github.com/satwareAG/php-firebird/releases/tag/v10.6.2
Issue #213 closed. All pipelines (Linux/macOS/Windows/Stubs/Coverage/Sanitizers) completed successfully.
