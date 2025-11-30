# Implementation Plan - Modernization Phase 5: Migration Reliability & API Refactoring

[Overview]
Refactor the PHP Firebird extension to support robust database migrations in modern frameworks (Laravel/Doctrine/Symphony) by solving "object in use" (-607) errors via metadata lock inspection and forced resolution. Simultaneously, modernization of the API surface by replacing ambiguous overloaded functions with distinct, type-safe alternatives.

[Research Conclusions]
- **Object in Use (-607)**: Caused by active metadata locks. Standard solution is ensuring no active transactions reference the object. In CI/Migration contexts, identifying and terminating blocking connections (via `MON$ATTACHMENTS`) is a valid, powerful recovery strategy.
- **Force Drop**: Implemented by querying `MON$DEPENDENCIES` / `MON$STATEMENTS` to identify blockers, then terminating them by `DELETE FROM MON$ATTACHMENTS` (valid in Firebird 2.5+).
- **API Design**: Moving away from overloading (e.g. `fbird_execute` behaving differently based on arg count/types) to distinct functions (`fbird_execute_statement` vs `fbird_execute_query`) is industry best practice for type safety and clarity (reducing "magic").

[Types]
New internal structs for inspection results:
- `fbird_blocker_info`: Struct containing attachment ID, transaction ID, and user info of a blocking process.

[Files]
- `ibase_query.c`: Refactor to implement distinct execution functions.
- `ibase_service.c`: Add maintenance/kill functions.
- `ibase_inspection.c` (New): Core logic for querying `MON$` tables and analyzing blockers.
- `php_ibase_inspection.h` (New): Header for inspection logic.
- `interbase.c`: Registration of new `fbird_*` functions.

[Functions]
1.  **Inspection & Maintenance (Migration Support)**
    - `fbird_list_table_blockers(resource $link, string $table_name): array`: Returns blocking attachment IDs/Transaction IDs.
    - `fbird_kill_attachment(resource $link, int $attachment_id): bool`: Terminates a specific connection (via `DELETE FROM MON$ATTACHMENTS`).
    - `fbird_drop_table_force(resource $link, string $table_name): bool`: High-level helper. Inspects blockers -> Kills them (optional safety check) -> Executes DROP TABLE.

2.  **Distinct Execution API (New Standard)**
    - `fbird_execute_statement(resource $trans, string $sql, array $params = []): int`: For DML/DDL (returns affected rows). STRICTLY requires transaction handle.
    - `fbird_execute_query(resource $trans, string $sql, array $params = []): resource`: For SELECT (returns cursor). STRICTLY requires transaction handle.
    - `fbird_execute_auto(resource $link, string $sql, array $params = []): mixed`: Auto-commit wrapper (implicit transaction start/commit/rollback).
    - `fbird_blob_open_stream(...)` etc. already exist, ensure consistency.

[Classes]
N/A.

[Dependencies]
- Firebird 2.5+ (for `MON$` tables). Fallback for older versions (graceful failure "Force drop not supported").

[Implementation Order]
1.  **Infrastructure**: Create `ibase_inspection.c` and implement `fbird_list_table_blockers` and `fbird_kill_attachment` using `MON$` queries.
2.  **Migration Helper**: Implement `fbird_drop_table_force` utilizing the inspection functions.
3.  **API Refactoring**: Create `fbird_execute_statement` and `fbird_execute_query` in C, exposing them in `interbase.c`.
4.  **Testing**: Create `tests/migration_001.phpt` to simulate a blockage (two connections) and verify `fbird_drop_table_force` succeeds where `fbird_query` fails.
5.  **Documentation**: Update `TRANSACTION_API_IMPROVEMENT_RFC.md` or create `MIGRATION_GUIDE.md`.

task_progress Items:
- [ ] Step 1: Create `ibase_inspection.c` with `fbird_list_table_blockers` and `fbird_kill_attachment`.
- [ ] Step 2: Implement `fbird_drop_table_force` logic combining inspection and kill.
- [ ] Step 3: Implement distinct `fbird_execute_statement`, `fbird_execute_query`, `fbird_execute_auto`.
- [ ] Step 4: Register all new functions in `interbase.c` and header.
- [ ] Step 5: Create reproduction test `tests/migration_001.phpt` (simulate lock, force drop).
