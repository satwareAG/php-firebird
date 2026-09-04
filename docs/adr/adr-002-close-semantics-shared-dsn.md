# ADR-002: close() semantics for DSN-deduplicated non-persistent connections

- **Status**: Accepted (option a - document current behavior)
- **Date**: 2026-09-04
- **Issue**: #579
- **Related**: #577 / #576 (ownership rework), #202 (plink default_link dance), #578 (lock-lifetime research)

## Context

`fbird_connect()` deduplicates non-persistent connections per DSN via the
`EG(regular_list)` hash cache (`_php_fbird_connect_link`, reuse path). Two
`Firebird\Connection` objects created for the same DSN therefore wrap **one**
resource entry with **one** physical server link:

```php
$a = fbird_connect($dsn);
$b = fbird_connect($dsn);   // cache reuse: same underlying entry
$a->close();                // fires the link dtor -> server link CLOSED
$b->isConnected();          // false - silently disconnected
```

Legacy php-interbase behaved the same way (non-plink branch always
`zend_list_close`), so nothing regressed in the #576/#577 ownership rework -
the quirk only became visible there. Options considered:

| Option | Effect | Trade-off |
|--------|--------|-----------|
| a) Keep + document | `close()` closes the shared server link | none |
| b) Per-wrapper logical handles (refcounted close: last wrapper closes) | PDO-like independence | large change; delayed lock release |
| c) Disable non-persistent DSN dedup | each connect = own physical link | aligns with other exts; changes perf footprint |

## Decision

**Option (a): keep the current behavior and document it.**

`close()` on a wrapper sharing a DSN-cached entry closes the shared server
link for all wrappers of that entry. This is the documented contract.

Rationale:

1. `close()` that actually closes is the least surprising primitive for a
   procedural-rooted extension; it is also what every existing consumer
   (including the Doctrine driver's connection handling and our own test
   suite) assumes today.
2. Option (b) delays physical disconnects - the exact lock-retention class
   that made #578 expensive to triage. Refcounted "last one out closes"
   semantics would keep server-side locks alive longer by default.
3. Option (c) is the cleanest long-term alignment (PDO and most PHP DB
   extensions do not deduplicate non-persistent links) and remains open as a
   future major-version proposal. It changes connection-count behavior for
   existing applications and therefore does not belong in a patch release.

## Consequences

- The behavior is contractual: closing one wrapper of a shared entry
  disconnects its siblings. Applications needing independent handles must use
  `FBIRD_CONNECT_FORCE_NEW` (or persistent links, whose `rc > 1` close is a
  no-op since #202).
- Memory safety of the shared-entry path is covered by regression tests
  (`tests/issue580_shared_entry_shutdown.phpt`, `tests/issue580_force_new_shutdown.phpt`).
- Reopen this ADR if a major version opts for option (c).
