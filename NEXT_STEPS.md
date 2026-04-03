# Next Steps - php-firebird

**Last Updated**: 2026-04-03

## Immediate (v10.6.x)

- [x] v10.6.1 released - musl bundle collision, zend_get_type_by_const portability, Windows release fix
- [x] `docker/musl-debug/` scratch directory removed (dir did not exist, confirmed 2026-04-02)
- [x] v10.6.2 tagged - current stable release on `v10.6.x` branch
- [x] `doctrine-firebird-driver` issues #105 and #106 created and amended - FB 2.5-5.0 compatibility story, type mapping validation, PDO clarification, CI modernization (satwareAG/doctrine-firebird-driver team to action)

## v11.0 Modernization (Next Milestone)

Tracked via GitHub issues #175-#179 (milestone: v11.0 - Modernization, due 2026-12-31):

- [ ] #175 - [M2] Migrate all 84 procedural arginfo to typed return macros
- [ ] #176 - [M3] Resource-to-object migration for Layer 1 procedural functions (breaking change)
- [ ] #177 - [M12] Replace call_user_function() in OOP layer with direct C calls
- [ ] #178 - [L3] Remove dead FB_API_VER < 30 code paths
- [ ] #179 - [L2] Remove legacy gds32_ms fallback from config.w32

## Open Implementation Specs

These specs have been audited and confirmed open - implementation required in upcoming sessions:

- [ ] `spec-v10.3.7-security.md`: SQL injection fix in `fbird_create_database()` (C1), SPB buffer validation in `Firebird\Service` (C2), dynamic alloc for char[4096] buffers (M10), PHP 8.2 minimum gate in config.m4 (M7)
- [ ] `spec-v10.4-build-hardening.md`: Compiler hardening flags (`-Wall -Wextra -D_FORTIFY_SOURCE=2 -fstack-protector-strong`), `-std=gnu17`, connection pointer via struct field, LTO (`-flto=auto`)
- [ ] `spec-v10.4-supply-chain.md`: SLSA provenance attestations (H1), CycloneDX SBOM workflow (H2), Dependabot SHA-pinning (H3), release script version stamp validation (M4)

## Technical Debt

- [ ] macOS x86_64 builds disabled (macos-13 runner deprecated) - evaluate macos-15 x86_64 when available
- [ ] Coverage threshold still at 54% - target 65%+ with v11.0 test improvements
