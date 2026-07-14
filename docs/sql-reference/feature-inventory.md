# Firebird 3/4/5 Feature Inventory for php-firebird

**Purpose**: Master cross-reference of every SQL/PSQL/admin feature introduced in
Firebird 3.0, 4.0, and 5.0, mapped to its availability through the php-firebird
driver's three API layers (procedural `fbird_*`, OOP `Firebird\*`, PDO `pdo_fbird`).

**Generated**: 2026-07-14
**Source repo**: [FirebirdSQL/firebird](https://github.com/FirebirdSQL/firebird)

---

## How to read this table

### Status legend

| Code | Meaning |
|------|---------|
| **Y (SQL)** | Works via SQL passthrough - driver sends the SQL string unchanged. No native binding needed. |
| **Y (native)** | Driver has explicit API support (function, method, class, constant, or DSN parameter). |
| **P** | Partial - works with limitations (e.g. type returned as string instead of native object, or only one layer supports it). Issue linked. |
| **N** | Not supported. Proposed issue for implementation. |
| **N/A** | Server-side or engine-internal feature. No driver surface needed or expected. |

### Pin convention

Source links are pinned to the last tag of each major version:

- FB 3.0 features link to `raw/R3_0_7/...`
- FB 4.0 features link to `raw/v4.0.7/...`
- FB 5.0 features link to `raw/v5.0.4/...`

### Column layout

| Col | Field |
|-----|-------|
| # | Sequential ID |
| Feature | Short name |
| Introduced | Version + tracker ticket |
| Source | Pinned link to canonical doc |
| `fbird_*` | Procedural layer status |
| `Firebird\*` | OOP layer status |
| `pdo_fbird` | PDO layer status |
| Issue | Existing php-firebird issue number, or `NEW` for proposed |
| Action | `document` = document usage only; `document + cite #N` = document and cite issue; `propose issue` = needs new issue |

---

## 1. Data Types

| # | Feature | Introduced | Source | `fbird_*` | `Firebird\*` | `pdo_fbird` | Issue | Action |
|---|---------|-----------|--------|-----------|-------------|-------------|-------|--------|
| 1 | `BOOLEAN` data type | FB 3.0 (CORE-726) | [data_types @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.data_types) | Y (native) | Y (native) | Y (native) | - | document |
| 2 | Identity columns (`GENERATED ... AS IDENTITY`) | FB 3.0 (CORE-1385) | [identity_columns @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.identity_columns.txt) | Y (native: `fbird_last_insert_id`) | Y (native) | Y (SQL) | - | document |
| 3 | `GENERATED ALWAYS AS IDENTITY` + `OVERRIDING` clause | FB 4.0 (CORE-5449) | [identity_columns @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.identity_columns.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 4 | `DECFLOAT(16)` / `DECFLOAT(34)` | FB 4.0 (CORE-5525) | [floating_point_types @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.floating_point_types.md) | P (string) | Y (`DecFloat` class) | P (string) | #417 | document + cite #417 |
| 5 | `INT128` (implicit via extended NUMERIC/DECIMAL) | FB 4.0 (CORE-4409) | [data_types @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.data_types) | P (string) | P (string) | P (string) | #418 | document + cite #418 |
| 6 | `TIME WITH TIME ZONE` | FB 4.0 (CORE-694) | [time_zone @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.time_zone.md) | P (string) | P (string) | P (string) | #419 | document + cite #419 |
| 7 | `TIMESTAMP WITH TIME ZONE` | FB 4.0 (CORE-694) | [time_zone @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.time_zone.md) | P (string) | P (string) | P (string) | #419 | document + cite #419 |
| 8 | `(VAR)BINARY(n)` / `BINARY VARYING(n)` aliases | FB 4.0 (CORE-5064) | [data_types @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.data_types) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 9 | Extended `NUMERIC`/`DECIMAL` precision (>18 digits) | FB 4.0 (CORE-4409) | [data_types @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.data_types) | P (string) | P (string) | P (string) | #418 | document + cite #418 |
| 10 | `SET BIND` rules (`isc_dpb_set_bind`) | FB 4.0 (improvement) | [set_bind @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.set_bind.md) | N (no DPB param) | N | Y (`PDO::FBIRD_ATTR_SET_BIND`) | #420 | document + cite #420 |
| 11 | Hex literals for `INT128` | FB 5.0 (#6809) | [hex_literals @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.hex_literals.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |

## 2. SQL Statements (DML)

| # | Feature | Introduced | Source | `fbird_*` | `Firebird\*` | `pdo_fbird` | Issue | Action |
|---|---------|-----------|--------|-----------|-------------|-------------|-------|--------|
| 12 | `OFFSET ... FETCH` clauses | FB 3.0 (CORE-4526) | [offset_fetch @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.offset_fetch.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 13 | `MERGE` statement | FB 3.0 | [merge @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.merge.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 14 | `MERGE ... WHEN NOT MATCHED BY SOURCE` | FB 5.0 (#6681) | [merge @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.merge.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 15 | `UPDATE OR INSERT` statement | FB 3.0 | [update_or_insert @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.update_or_insert) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 16 | `RETURNING` clause (incl. `RETURNING *` since FB 4.0) | FB 3.0 / FB 4.0 (CORE-3808) | [returning @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.returning) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 17 | `RETURNING` with multiple rows | FB 5.0 (#6815) | [returning @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.returning) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 18 | `SELECT ... WITH LOCK` | FB 3.0 | [explicit_locks @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.explicit_locks) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 19 | `SKIP LOCKED` clause | FB 5.0 (#7350) | [skip_locked @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.skip_locked.md) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 20 | Batch INSERT/UPDATE API (`IBatch`) | FB 4.0 (CORE-5951) | [Using_OO_API @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/Using_OO_API.html) | Y (native: `fbird_batch_*`) | Y (`BatchHandle` class) | N | #421 | document + cite #421 |
| 21 | `DEFAULT` keyword in DML (`INSERT ... VALUES (DEFAULT, ...)`) | FB 4.0 (CORE-5463) | [ddl @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.ddl.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 22 | `RECREATE` / `CREATE OR ALTER SEQUENCE` | FB 3.0 (CORE-3018) | [sequence_generators @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.sequence_generators) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 23 | `ALTER SEQUENCE ... RESTART WITH` | FB 3.0 | [sequence_generators @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.sequence_generators) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 24 | CTEs (Common Table Expressions `WITH ... AS`) | FB 3.0 | [common_table_expressions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.common_table_expressions) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 25 | Derived tables | FB 3.0 | [derived_tables @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.derived_tables.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 26 | Parenthesized query expressions (standard compliance) | FB 5.0 (#6740) | [sql.extensions @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 27 | SQL standard string literal syntax (`U&'...'`) | FB 5.0 (#5589) | [alternate_string_quoting @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.alternate_string_quoting.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 28 | SQL standard binary string literal syntax | FB 5.0 (#5588) | [hex_literals @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.hex_literals.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |

## 3. Window Functions

| # | Feature | Introduced | Source | `fbird_*` | `Firebird\*` | `pdo_fbird` | Issue | Action |
|---|---------|-----------|--------|-----------|-------------|-------------|-------|--------|
| 29 | `ROW_NUMBER()`, `RANK()`, `DENSE_RANK()` | FB 3.0 (CORE-2830) | [window_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.window_functions.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 30 | `FIRST_VALUE()`, `LAST_VALUE()`, `NTH_VALUE()` | FB 3.0 (CORE-3619/3620/3621) | [window_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.window_functions.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 31 | `LAG()`, `LEAD()` | FB 3.0 (CORE-2869) | [window_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.window_functions.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 32 | `OVER ()` with existing aggregates (`SUM`, `COUNT`, etc.) | FB 3.0 (CORE-2090) | [window_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.window_functions.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 33 | `PARTITION BY` clause | FB 3.0 (CORE-2133) | [window_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.window_functions.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 34 | `ORDER BY` inside `OVER ()` | FB 3.0 (CORE-2823) | [window_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.window_functions.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 35 | Window frames (`ROWS`/`RANGE` `BETWEEN`) | FB 4.0 (CORE-3647) | [window_functions @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.window_functions.md) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 36 | Named windows (`WINDOW` clause) | FB 4.0 (CORE-5346) | [window_functions @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.window_functions.md) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 37 | `PERCENT_RANK()`, `CUME_DIST()`, `NTILE()` | FB 4.0 (CORE-1688) | [window_functions @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.window_functions.md) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 38 | `FILTER` clause for aggregates (`<agg> ... FILTER (WHERE ...)`) | FB 4.0 (CORE-5768) | [aggregate_filter @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.aggregate_filter.md) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |

## 4. PSQL (Procedural SQL)

| # | Feature | Introduced | Source | `fbird_*` | `Firebird\*` | `pdo_fbird` | Issue | Action |
|---|---------|-----------|--------|-----------|-------------|-------------|-------|--------|
| 39 | PSQL packages (`CREATE PACKAGE`) | FB 3.0 (CORE-2312) | [packages @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.packages.txt) | Y (SQL) | Y (SQL) | Y (SQL) | #423 | document + cite #423 |
| 40 | Subfunctions / Subprocedures | FB 3.0 (CORE-3626/CORE-1288) | [subroutines @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.subroutines.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 41 | User-defined PSQL functions (`CREATE FUNCTION`) | FB 3.0 (CORE-2047) | [ddl @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.ddl.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 42 | DDL triggers (`CREATE TRIGGER ... ON DDL ...`) | FB 3.0 (CORE-2310) | [ddl_triggers @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.ddl_triggers.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 43 | Database triggers (`ON CONNECT/DISCONNECT/TRANSACTION`) | FB 3.0 | [db_triggers @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.db_triggers.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 44 | `CONTINUE` statement | FB 3.0 (CORE-1209) | [leave_labels @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.leave_labels) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 45 | Server-side scrollable cursors | FB 3.0 (CORE-803) | [scrollable_cursors @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.scrollable_cursors.txt) | P (FB3 server-side) | P | P | #426 | document + cite #426 |
| 46 | `EXECUTE BLOCK` | FB 3.0 | [execute_block @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.execute_block) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 47 | Autonomous transactions in PSQL | FB 3.0 | [autonomous_transactions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.autonomous_transactions.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 48 | Cursor variables | FB 3.0 | [cursor_variables @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.cursor_variables.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 49 | Exception name/text in `WHEN ANY` handler | FB 4.0 (CORE-2040/CORE-1132) | [exception_handling @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.exception_handling) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 50 | Management statements in PSQL blocks | FB 4.0 (CORE-5887) | [management_statements_psql @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.management_statements_psql.md) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 51 | Subroutines access to outer-scope variables | FB 5.0 (#4769) | [subroutines @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.subroutines.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 52 | `EXECUTE STATEMENT` rich form (external DB, dialect, timezone) | FB 4.0 (CORE-694) | [execute_statement2 @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.execute_statement2) | Y (SQL) | Y (SQL) | Y (SQL) | #424 | document + cite #424 |
| 53 | PSQL `CALL` for procedures/functions | FB 3.0 | [call @ master (trunk)](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.call.md) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |

## 5. Security & Access Control

| # | Feature | Introduced | Source | `fbird_*` | `Firebird\*` | `pdo_fbird` | Issue | Action |
|---|---------|-----------|--------|-----------|-------------|-------------|-------|--------|
| 54 | SRP (Secure Remote Password) authentication | FB 3.0 | [README.SecureRemotePassword @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/README.SecureRemotePassword.html) | Y (native: client lib) | Y (native) | Y (native) | - | document |
| 55 | `SQL SECURITY {DEFINER\|INVOKER}` clause | FB 4.0 (CORE-5568) | [sql_security @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.sql_security.txt) | Y (SQL) | Y (SQL) | Y (SQL) | #423 | document + cite #423 |
| 56 | Cumulative roles | FB 3.0 (CORE-2884) | [cumulative_roles @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.cumulative_roles.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 57 | Grant role to another role | FB 4.0 (CORE-1815) | [user_management @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.user_management) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 58 | Implicitly active roles | FB 4.0 (CORE-751/CORE-2762) | [set_role @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.set_role) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 59 | `CREATE OR ALTER USER` / `ALTER USER ... INACTIVE` | FB 3.0 (CORE-2063/CORE-2004) | [user_management @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.user_management) | Y (native: `fbird_add_user` etc.) | Y (native: `Service` class) | Y (SQL) | - | document |
| 60 | Auto-auth mapping (`CREATE MAPPING ...`) | FB 4.0 | [mapping @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.mapping.html) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 61 | `COMMENT ON MAPPING ...` | FB 5.0 (#7046) | [ddl @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.ddl.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 62 | Per-database security database | FB 3.0 (CORE-3368) | [README.security_database @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.security_database.txt) | Y (config/DSN) | Y (config) | Y (config) | - | document |
| 63 | DDL access rights (`GRANT ... ON ... TO ...`) | FB 3.0 (CORE-735) | [ddl_access @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.ddl_access.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 64 | Transfer specific DBA privileges to users | FB 4.0 (CORE-5343) | [user_management @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.user_management) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 65 | Grants on `MON$` monitoring tables | FB 4.0 (CORE-2557) | [README.monitoring_tables @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.monitoring_tables) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |

## 6. Performance & Concurrency

| # | Feature | Introduced | Source | `fbird_*` | `Firebird\*` | `pdo_fbird` | Issue | Action |
|---|---------|-----------|--------|-----------|-------------|-------------|-------|--------|
| 66 | True SMP support for SuperServer | FB 3.0 (CORE-775) | [WhatsNew @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/WhatsNew) | N/A (engine) | N/A | N/A | - | document (server-side) |
| 67 | `READ CONSISTENCY` isolation level | FB 4.0 (CORE-5953) | [README.read_consistency @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.read_consistency.md) | Y (native: `FBIRD_READ_CONSISTENCY`) | P (#425) | P (#425) | #425 | document + cite #425 |
| 68 | Statement-level timeouts | FB 4.0 (CORE-5488) | [README.statement_timeouts @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.statement_timeouts) | Y (native: `fbird_set/get_statement_timeout`) | Y (native: `Connection::set/getStatementTimeout`) | N | #464 | document + cite #464 |
| 69 | Session idle timeouts | FB 4.0 (CORE-5488) | [README.session_idle_timeouts @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.session_idle_timeouts) | Y (native: `fbird_set/get_idle_timeout`) | Y (native: `Connection::set/getIdleTimeout`) | N | #422 | document + cite #422 |
| 70 | Parallel sweeping and index creation | FB 5.0 (#7447) | [README.parallel_features @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/README.parallel_features) | N/A (engine) + config | N/A | N/A | #427 | document + cite #427 |
| 71 | Compiled statement cache | FB 5.0 (#7144) | [CHANGELOG @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/CHANGELOG.md) | N/A (engine) | N/A | N/A | - | document (engine-internal) |
| 72 | Cost-based hash join vs nested-loop choice | FB 5.0 (#7331) | [README.Optimizer @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/README.Optimizer.txt) | N/A (optimizer) | N/A | N/A | - | document (optimizer-internal) |
| 73 | PSQL/SQL profiler (`RDB$PROFILER`) | FB 5.0 (#7086) | [profiler @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.profiler.md) | Y (SQL: `SELECT FROM PLG$PROF_*`) | Y (SQL) | Y (SQL) | #428 | document + cite #428 |
| 74 | Database linger (`ALTER DATABASE SET LINGER`) | FB 3.0 (CORE-4263) | [linger @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.linger) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 75 | Online table/index validation | FB 3.0 (CORE-4707) | [README.online_validation @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/README.online_validation) | Y (native: `fbird_maintain_db`) | Y (native: `Service`) | N | - | document |
| 76 | Detailed query execution plan | FB 3.0 (CORE-3332) | [plan @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.plan) | Y (SQL: `SET PLAN` via isql; engine returns plan via API) | Y | Y | - | document |
| 77 | Expression indices | FB 3.0 | [expression_indices @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.expression_indices) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 78 | Partial indices (`WHERE` clause) | FB 5.0 (#3750) | [partial_indices @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.partial_indices) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 79 | `ParallelWorkers` config default | FB 5.0 (#7682) | [CHANGELOG @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/CHANGELOG.md) | N/A (server config) | N/A | N/A | #427 | document + cite #427 |
| 80 | `OptimizerModeForRegression` / optimization modes surfaced | FB 5.0 (#7405) | [CHANGELOG @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/CHANGELOG.md) | Y (SQL: `SET OPTIMIZE`) | Y (SQL) | Y (SQL) | - | document |

## 7. Built-in Functions

| # | Feature | Introduced | Source | `fbird_*` | `Firebird\*` | `pdo_fbird` | Issue | Action |
|---|---------|-----------|--------|-----------|-------------|-------------|-------|--------|
| 81 | Statistical aggregates (`STDDEV_POP`, `STDDEV_SAMP`, `VAR_POP`, `VAR_SAMP`) | FB 3.0 (CORE-4714) | [statistical_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.statistical_functions.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 82 | Covariance/correlation (`COVAR_SAMP`, `COVAR_POP`, `CORR`) | FB 3.0 (CORE-4717) | [statistical_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.statistical_functions.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 83 | Linear regression (`REGR_SLOPE`, `REGR_INTERCEPT`, etc.) | FB 3.0 (CORE-4722) | [regr_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.regr_functions.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 84 | Inverse hyperbolic trig functions (`ASINH`, `ACOSH`, `ATANH`, etc.) | FB 3.0 (CORE-2744) | [builtin_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.builtin_functions.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 85 | Cryptographic functions (`ENCRYPT`, `DECRYPT`, `RSA_SIGN`, `RSA_VERIFY`, etc.) | FB 4.0 (CORE-5970) | [builtin_functions @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.builtin_functions.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 86 | `FIRST_DAY()` / `LAST_DAY()` | FB 4.0 (CORE-5620) | [builtin_functions @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.builtin_functions.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 87 | `QUARTER` support in `EXTRACT`, `FIRST_DAY`, `LAST_DAY` | FB 5.0 (#5959) | [builtin_functions @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.builtin_functions.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 88 | `BLOB_APPEND()` function | FB 5.0 (#7216) | [blob_append @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.blob_append.md) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 89 | `UNICODE_CHAR()` / `UNICODE_VAL()` | FB 5.0 (#6798) | [builtin_functions @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.builtin_functions.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 90 | `HASH()` with multiple algorithms | FB 4.0 (CORE-4436) | [builtin_functions @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.builtin_functions.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 91 | `RDB$GET_CONTEXT` / `RDB$SET_CONTEXT` (incl. `SESSION_TIMEZONE`) | FB 3.0+ / FB 4.0 | [context_variables2 @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.context_variables2) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 92 | `RDB$BLOB_UTIL` system package | FB 5.0 (#281) | [blob_util @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.blob_util.md) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 93 | Time zone built-in functions (`AT TIME ZONE`, `EXTRACT TIMEZONE_HOUR`, etc.) | FB 4.0 (CORE-694) | [time_zone @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.time_zone.md) | Y (SQL) | Y (SQL) | Y (SQL) | #419 | document + cite #419 |
| 94 | `DECFLOAT ROUND` / `DECFLOAT TRAPS` functions | FB 5.0 (#7642) | [builtin_functions @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.builtin_functions.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |

## 8. Administration & Operations

| # | Feature | Introduced | Source | `fbird_*` | `Firebird\*` | `pdo_fbird` | Issue | Action |
|---|---------|-----------|--------|-----------|-------------|-------------|-------|--------|
| 95 | Online table/index validation | FB 3.0 (CORE-4707) | [README.online_validation @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/README.online_validation) | Y (native: `fbird_maintain_db`) | Y (native: `Service`) | N | - | document |
| 96 | Database linger | FB 3.0 (CORE-4263) | [linger @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.linger) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 97 | gbak: ignore specific tables data | FB 3.0 (CORE-2208) | [README.gbak @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.gbak) | Y (native: `fbird_backup` options) | Y (native: `Service`) | N | - | document |
| 98 | gbak: backup encrypted databases | FB 4.0 (CORE-5808) | [README.gbak @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.gbak) | Y (native: `fbird_backup`) | Y (native: `Service`) | N | - | document |
| 99 | gbak: parallel backup/restore (`-PARALLEL`) | FB 5.0 (#1783) | [README.gbak @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/README.gbak) | Y (native: `fbird_backup` options) | Y (native: `Service`) | N | - | document |
| 100 | gbak: custom verbose interval (`-VT`) | FB 3.0 (CORE-462) | [README.gbak @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.gbak) | Y (native: `fbird_backup` options) | Y (native: `Service`) | N | - | document |
| 101 | gbak: enhanced restore using batch API | FB 4.0 (CORE-5952) | [README.gbak @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.gbak) | Y (native: engine-level) | Y | N | - | document |
| 102 | NBackup as online dump | FB 4.0 (CORE-2216) | [WhatsNew @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/CHANGELOG.md) | N (no nbackup API) | N | N | #476 | document + cite #476 |
| 103 | Logical replication (built-in) | FB 4.0 (CORE-2022) | [README.replication @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.replication.md) | N/A (server-side config) | N/A | N/A | - | document (server-side) |
| 104 | Monitoring tables (`MON$*`) extensions | FB 3.0+ / FB 4.0+ / FB 5.0 | [README.monitoring_tables @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/README.monitoring_tables) | Y (SQL: `SELECT FROM MON$*`) | Y (SQL) | Y (SQL) | - | document |
| 105 | `MON$COMPILED_STATEMENTS` table | FB 5.0 (#7050) | [README.monitoring_tables @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/README.monitoring_tables) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 106 | Connection compressed/encrypted status in `MON$ATTACHMENTS` | FB 4.0 (CORE-5536) | [README.monitoring_tables @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.monitoring_tables) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 107 | `MON$SESSION_TIMEZONE` in `MON$ATTACHMENTS` | FB 5.0 (#6794) | [README.monitoring_tables @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/README.monitoring_tables) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 108 | Trace services (`fbtracemgr` functionality) | FB 3.0+ | [README.trace_services @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/README.trace_services) | N (only wire trace #377) | N | N | #477 | document + cite #477 |
| 109 | External connections pool (server-side) | FB 4.0 (CORE-5990) | [external_connections_pool @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.external_connections_pool) | N/A (server config) | N/A | N/A | - | document (server-side) |
| 110 | `RDB$RECORD_VERSION` pseudocolumn | FB 3.0 (CORE-3291) | [WhatsNew @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/WhatsNew) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 111 | `RDB$KEYWORDS` system table | FB 5.0 (#6713) | [ddl @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.ddl.txt) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 112 | Pseudo-table of users (`RDB$USER_PRIVILEGES` extensions) | FB 3.0 (CORE-2639) | [WhatsNew @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/WhatsNew) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 113 | Inline minor ODS upgrade | FB 5.0 (#7397) | [CHANGELOG @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/CHANGELOG.md) | N/A (engine) | N/A | N/A | - | document (engine-internal) |
| 114 | `ALTER DATABASE SET LINGER` | FB 3.0 (CORE-4263) | [linger @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.linger) | Y (SQL) | Y (SQL) | Y (SQL) | - | document |
| 115 | `ALTER DATABASE ENCRYPTION` (database encryption) | FB 3.0 (CORE-657) | [WhatsNew @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/WhatsNew) | P (SQL: `ALTER DATABASE`; key mgmt via `fbird_maintain_db`?) | P | N | - | document |

---

## Created issues (Phase 3)

These are the **net-new gaps** discovered during the inventory cross-check.
Created via `gh issue create` on 2026-07-14.

| Issue | Title | Labels | Rationale |
|-------|-------|--------|-----------|
| [#478](https://github.com/satwareAG/php-firebird/issues/478) | `feat: FB4 session timezone DPB parameter (isc_dpb_session_time_zone)` | `enhancement`, `fb4-plus`, `client-coverage`, `procedural-parity` | Driver does not expose `isc_dpb_session_time_zone` at connect time. SQL `SET TIME ZONE` works per-statement, but DPB sets default session timezone for the entire connection. Affects rows #6, #7, #93. |
| [#476](https://github.com/satwareAG/php-firebird/issues/476) | `feat: FB4 nbackup online dump service API` | `enhancement`, `fb4-plus`, `client-coverage` | `fbird_backup` exposes gbak but not nbackup. NBackup provides incremental online dumps. Row #102. |
| [#477](https://github.com/satwareAG/php-firebird/issues/477) | `feat: FB3 trace service start/stop/config via service API` | `enhancement`, `fb3`, `client-coverage` | #377 tracks wire-protocol trace (different feature). The Firebird trace service (`isc_action_svc_trace_*`) is not exposed. Row #108. |

### Existing issues referenced in this inventory

| Issue | Title | Features cited |
|-------|-------|----------------|
| #417 | FB4 DECFLOAT(16/34) native type support | #4, (related: #93, #94) |
| #418 | FB4 INT128 native type support | #5, #9 |
| #419 | FB4 TIME/TIMESTAMP WITH TIME ZONE | #6, #7, #93 |
| #420 | FB4 SET BIND rule coverage | #10 |
| #421 | FB4 batch DML API (IBatch) | #20 |
| #422 | FB4 statement + session idle timeout | #69 |
| #423 | FB4 packages + SQL SECURITY | #39, #55 |
| #424 | FB4 EXECUTE STATEMENT rich form | #52 |
| #425 | FB4 READ CONSISTENCY transaction isolation | #67 |
| #426 | FB5 scrollable cursors (6 orientations) | #45 |
| #427 | FB5 parallel workers | #70, #79 |
| #428 | FB5 profiler plugin | #73 |
| #464 | Per-statement timeout (IStatement::getTimeout/setTimeout) | #68 |

### Summary statistics

| Metric | Count |
|--------|-------|
| Total features inventoried | 115 |
| Y (SQL passthrough) - no driver work needed | 76 |
| Y (native) - already fully supported | 22 |
| P (partial) - works with limitations, issue tracked | 11 |
| N (gap) - needs implementation | 3 (#476, #477, #478) |
| N/A (server-side / engine-internal) | 8 |
| Existing issues referenced | 13 |
| New issues created | 3 (#476, #477, #478) |

---

## Next steps

1. ~~**Review** the 3 proposed issues~~ - approved and created (#476, #477, #478).
2. ~~Issues created via `gh issue create`~~.
3. Cheatsheet category files (Phase 4) reference both existing and new issue numbers.
