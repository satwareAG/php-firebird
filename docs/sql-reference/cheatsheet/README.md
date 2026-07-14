# Firebird 3/4/5 Feature Cheatsheet

Every SQL/PSQL/admin feature introduced in Firebird 3.0, 4.0, or 5.0, mapped to
its availability through the php-firebird driver.

## Categories

| File | Features | Focus |
|------|----------|-------|
| [01-data-types.md](01-data-types.md) | 11 | BOOLEAN, DECFLOAT, INT128, time-zone types, identity columns, SET BIND |
| [02-sql-statements.md](02-sql-statements.md) | 17 | MERGE, UPDATE OR INSERT, RETURNING, SKIP LOCKED, OFFSET/FETCH, batch API |
| [03-window-functions.md](03-window-functions.md) | 10 | ROW_NUMBER, RANK, LAG/LEAD, frames, named windows, FILTER clause |
| [04-psql.md](04-psql.md) | 15 | Packages, subroutines, DDL triggers, autonomous transactions, scrollable cursors |
| [05-security.md](05-security.md) | 12 | SRP, SQL SECURITY, roles, user management, mapping, grants |
| [06-performance.md](06-performance.md) | 15 | Read consistency, timeouts, parallel features, profiler, indices |
| [07-builtin-functions.md](07-builtin-functions.md) | 14 | Statistical, crypto, time-zone, BLOB_APPEND, UNICODE_CHAR/VAL |
| [08-admin-ops.md](08-admin-ops.md) | 21 | gbak, nbackup, monitoring tables, trace, replication, linger |

## Entry format

Each feature entry contains:

1. **Feature name** with version and tracker ticket
2. **Source link** pinned to the relevant Firebird tag
3. **SQL block** showing the minimal syntax
4. **PHP block** showing driver-layer usage (procedural + OOP where applicable)
5. **Driver matrix** (3-row table with Y/P/N + issue references)

## Legend

| Code | Meaning |
|------|---------|
| **Y (SQL)** | Works via SQL passthrough |
| **Y (native)** | Driver has explicit API support |
| **P** | Partial - tracked issue linked |
| **N** | Not supported - tracked issue linked |
| **N/A** | Server-side, no driver surface needed |
