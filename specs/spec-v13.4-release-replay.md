# Spec: P4 Weather Replay for Release Lanes (#725)

**Issue**: #725
**Branch**: `ci725-release-replay`
**Date**: 2026-11-08
**Status**: Approved

---

## Intent

Extend the #688-P4 closed-enum weather-replay policy (today only in `scripts/verify-ci-green.sh`, the pre-tag gate) to the release lanes. The v13.3.1 train lost ~3h wall-clock to three macOS pool cancellations that were replayed by hand; the mechanical signature (cancelled + zero executed steps) is trivially detectable, so the replay decision must be mechanical too - never a human judgment call in the release path.

## Contract (closed enum, unchanged semantics)

| Classification | Definition | Action |
|---|---|---|
| `queue-starvation` | >=1 non-success job, ALL non-success jobs `cancelled` with zero executed steps | Replay failed jobs ONCE (`gh run rerun --failed`), bounded wait, waiver-logged |
| `whole-run-cancelled` | run `conclusion == cancelled` and zero jobs or all jobs cancelled with zero executed steps | Same as above |
| `none` (genuine red / startup failure / success) | anything else | **No retry** - the counted error stays authoritative (#721) |

Invariants:
- Replay happens at the run's own SHA by construction (`gh run rerun --failed` re-runs the same revision; marker/G8 bindings make stale-SHA publishes impossible, #723/#722).
- Every replay pass appends to `scripts/ci-weather-waivers.log` (same format as #688; per-release waiver rate stays countable - council run 5).
- One classifier, two consumers: `verify-ci-green.sh` and the release-lane orchestrator share `scripts/lib/weather-replay.sh` (single implementation of the enum).

## Surfaces

- `scripts/lib/weather-replay.sh` - classifier (`weather_classify`, fixture-testable, reads run JSON) + waiver log.
- `scripts/release-replay.sh` - orchestrator CLI: classify -> replay-or-refuse -> log.
- `.github/workflows/release-replay.yml` - `workflow_dispatch(run_id, lane)`, the lane an operator calls instead of a manual `gh run rerun`.
- `scripts/gates/issue725-replay-fixtures.sh` - fixture matrix (audit F1): cancellation-only, mixed failure/cancellation, missing steps field, startup failure, success, plus the negative control (one executed step flips cancellation-only to `none`).

## Out of Scope

- Auto-triggering replays from inside the release workflows themselves (operator dispatches; automation of the *decision*, not the trigger).
- Extending the enum to `startup_failure` (future work if the class shows up; recorded here so extension is a decision, not drift).

## Success Criteria

- [ ] Fixture matrix passes: replayable shapes classify replayable, everything else refuses.
- [ ] Negative control: adding one executed step to a cancelled job flips the verdict to `none`.
- [ ] `verify-ci-green.sh` behavior unchanged (same functions, shared implementation).
- [ ] Orchestrator refuses genuine red with a nonzero exit and no rerun call.
