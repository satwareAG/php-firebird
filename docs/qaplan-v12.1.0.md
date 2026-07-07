# QA Plan v12.1.0

Prioritized gap list from the M2 Procedural API Parity milestone.
Each gap has a tracking issue and suggested target version.

## P0 - Critical (blocks Doctrine/migration use cases)

| Issue | Gap | Target | Rationale |
|-------|-----|--------|-----------|
| #359 | `fbird_fetch_array` (BOTH mode) | v13.0.0 | Regression vs interbase; most-felt gap for migration |
| #369 | Per-connection error context | v13.0.0 | Global single-slot loses concurrent errors; architectural |

## P1 - High (Doctrine/amicron-platform needs)

| Issue | Gap | Target | Rationale |
|-------|-----|--------|-----------|
| #360 | `fbird_ping` (procedural) | v12.1.x | Asymmetry with OO; trivial wrapper |
| #361 | `fbird_server_version` at conn level | v12.1.x | Doctrine DBAL `getDatabasePlatformVersion()` |
| #373 | `fbird_meta_data` / `list_tables` | v13.0.0 | Doctrine `SchemaManager::listTableColumns` |

## P2 - Medium (PHP dev ergonomics)

| Issue | Gap | Target | Rationale |
|-------|-----|--------|-----------|
| #362 | `fbird_data_seek` (scrollable) | v13.0.0 | PDO has scrollable; procedural doesn't expose |
| #363 | `fbird_fetch_all` | v13.0.0 | Trivial convenience |
| #364 | `fbird_fetch_column` | v13.0.0 | Trivial convenience |
| #365 | `fbird_fetch_object(class, args)` | v13.0.0 | ORM hydration |
| #366 | `fbird_set_charset` / `get_charset` | v13.0.0 | Runtime charset change |
| #367 | `fbird_bind_param` (named, by ref) | v13.0.0 | Doctrine DBAL `bindParam` |
| #368 | `fbird_bind_result` | v13.0.0 | oci8-style output binding |
| #370 | `fbird_error_list` | v13.0.0 | Multi-error from status vector |
| #371 | Structured diagnostic fields | v13.0.0 | pgsql-style `error_field` |
| #372 | `fbird_multi_query` | v13.0.0 | PDO has it; procedural doesn't |

## P3 - Low (nice to have)

| Issue | Gap | Target | Rationale |
|-------|-----|--------|-----------|
| #374 | `fbird_escape_literal` / `identifier` | v13.0.0 | pgsql has both |
| #375 | `fbird_blob_truncate` / `erase` / `flush` | v13.0.0 | oci8 has full LOB suite |
| #376 | `fbird_blob_export` (to file) | v13.0.0 | pgsql/oci8 have it |
| #377 | `fbird_debug` / `trace` | v13.0.0 | Wire-protocol trace |
| #378 | `fbird_stmt_attr_get/set` | v13.0.0 | Per-stmt prefetch |
| #379 | `fbird_result_metadata` (pre-exec) | v13.0.0 | mysqli has it |
| #380 | `fbird_send_long_data` | v13.0.0 | Stream BLOB to param |
| #381 | `fbird_stmt_reset` | v13.0.0 | Re-execute without re-prepare |

## Legitimate Non-Gaps (documented, not implemented)

| Issue | Why N/A |
|-------|---------|
| #382 | `fbird_warning_count` - Firebird has no warning stream |
| #383 | `fbird_select_db` - DB bound to attachment |
| #384 | `fbird_thread_id` - FB protocol is connection-based |

## Implementation Branch Tracking

Each gap implementation spawns a separate `feat/*` branch off `satware-main`
after v12.1.0 ships. See M9 backlog issues (#436-#441) for tracking.
