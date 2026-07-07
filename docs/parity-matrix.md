# Cross-Driver Capability Matrix

Comparison of php-firebird's `fbird_*` procedural API against the big-5 PHP
database extensions. Legend: **Y** = supported, **N** = not supported,
**P** = partial / different API, **-** = N/A for that DB engine.

## Connection Management

| Capability | mysqli | pgsql | sqlite3 | oci8 | interbase | **php-firebird** |
|---|---|---|---|---|---|---|
| connect | Y | Y | Y | Y | Y | **Y** |
| pconnect | Y | Y | - | Y | Y | **Y** |
| close | Y | Y | Y | Y | Y | **Y** |
| ping | Y | Y | - | - | N | **N** (#360) |
| create_database | N | N | Y | N | N | **Y** |
| drop_db | N | N | N | N | Y | **Y** |

## Query Execution

| Capability | mysqli | pgsql | sqlite3 | oci8 | interbase | **php-firebird** |
|---|---|---|---|---|---|---|
| query | Y | Y | Y | Y | Y | **Y** |
| query_params (one-shot) | Y | Y | Y | Y | N | **Y** |
| multi_query | Y | P | Y | N | N | **N** (#372) |
| unbuffered_query | Y | Y | - | Y | Y | **Y** |

## Prepared Statements

| Capability | mysqli | pgsql | sqlite3 | oci8 | interbase | **php-firebird** |
|---|---|---|---|---|---|---|
| prepare | Y | Y | Y | Y | Y | **Y** |
| bind_param (named, by ref) | Y | N | Y | Y | N | **N** (#367) |
| bind_result (define output) | Y | N | N | Y | N | **N** (#368) |
| execute | Y | Y | Y | Y | Y | **Y** |
| stmt_reset | Y | N | Y | N | N | **N** (#381) |

## Result Fetching

| Capability | mysqli | pgsql | sqlite3 | oci8 | interbase | **php-firebird** |
|---|---|---|---|---|---|---|
| fetch_row / assoc / object | Y | Y | Y | Y | Y | **Y** |
| fetch_array (BOTH mode) | Y | Y | Y | Y | Y | **N** (#359 regression) |
| fetch_all | Y | Y | Y | Y | N | **N** (#363) |
| fetch_column | Y | P | N | P | N | **N** (#364) |
| fetch_object(class, args) | Y | Y | Y | N | N | **N** (#365) |
| data_seek | Y | Y | P | N | N | **N** (#362) |

## Transactions

| Capability | mysqli | pgsql | sqlite3 | oci8 | interbase | **php-firebird** |
|---|---|---|---|---|---|---|
| begin / commit / rollback | Y | Y (SQL) | Y (SQL) | Y | Y | **Y** |
| commit_ret / rollback_ret | N | N | N | N | Y | **Y** (unique) |
| savepoint | Y | N (SQL) | N (SQL) | N (SQL) | N | **Y** (unique) |
| multi-DB 2PC | N | N | N | N | Y | **Y** (unique) |
| TPB isolation flags | P | N | N | N | Y | **Y** (most granular) |
| table reservation locks | N | N | N | N | N | **Y** (unique) |

## Error Handling

| Capability | mysqli | pgsql | sqlite3 | oci8 | interbase | **php-firebird** |
|---|---|---|---|---|---|---|
| errmsg / errcode | Y | Y | Y | Y | Y | **Y** |
| sqlstate | Y | Y | N | Y | N | **Y** (global only, #369) |
| error_list (array) | Y | P | N | P | N | **N** (#370) |
| structured diag fields | N | Y | N | P | N | **N** (#371) |
| per-conn error context | Y | Y | N | Y | N | **N** (#369 architectural) |

## BLOB / LOB

| Capability | mysqli | pgsql | sqlite3 | oci8 | interbase | **php-firebird** |
|---|---|---|---|---|---|---|
| create / write | P | Y | Y | Y | Y | **Y** (richest) |
| read / seek / stream | N | Y | Y | Y | P | **Y** |
| truncate / erase | N | Y | N | Y | N | **N** (#375) |
| export (to file) | N | Y | N | Y | N | **N** (#376) |
| native array fields | N | P | N | Y | N | **Y** (unique) |

## Special Features

| Capability | mysqli | pgsql | sqlite3 | oci8 | interbase | **php-firebird** |
|---|---|---|---|---|---|---|
| DB-side events | N | Y | N | N | Y | **Y** (richest) |
| async query | Y | Y | N | N | N | **N** |
| backup / restore | N | N | Y | N | Y | **Y** (unique, driver-level) |
| user management | N | Y | N | Y | Y | **Y** (full CRUD) |
| batch DML | N | N | N | N | N | **Y** (unique, FB4+ IBatch) |
| scrollable cursors | P | N (SQL) | P | P | P | **Y** (PDO, FB5+) |
| escape_literal / identifier | N | Y | N | N | N | **N** (#374) |
| debug / trace | Y | Y | N | N | N | **N** (#377) |

## php-firebird Unique Strengths

1. **Driver-level backup/restore** — no big-5 has this
2. **Most granular transaction control** — full TPB, table reservations, multi-DB 2PC
3. **Native event manager** — richer than pgsql LISTEN/NOTIFY
4. **Limbo/2PC transaction recovery**
5. **Batch DML API** (FB 4.0+ IBatch)
6. **Native SQL_ARRAY field support** (full read/write)
7. **Service manager** (server_info, db_info, maintain_db, user management)
8. **Scrollable cursors** (in PDO layer, FB5+)
