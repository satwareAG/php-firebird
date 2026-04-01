# Next Steps - php-firebird

**Last Updated**: 2026-04-01

## Immediate (v10.6.x)

- [ ] Investigate 8 musl x86_64 build failures at "Create Bundle" step (rerun submitted, run 23851923952)
- [ ] Investigate Windows release upload failure (rerun submitted, run 23851923945)
- [ ] If musl reruns fail: inspect logs, fix in `build-precompiled.sh`, tag v10.6.1
- [ ] Clean up `docker/musl-debug/` scratch directory (untracked)

## v11.0 Modernization (Next Milestone)

Tracked via GitHub issues #176-#179:

- [ ] #176 - Modernization planning
- [ ] #177 - Architecture improvements
- [ ] #178 - API surface review
- [ ] #179 - Documentation overhaul

## Technical Debt

- [ ] macOS x86_64 builds disabled (macos-13 runner deprecated) - evaluate macos-15 x86_64 when available
- [ ] Coverage threshold still at 54% - target 65%+ with v11.0 test improvements
