# OO API Only Policy

## Overview

**The php-firebird extension exclusively supports the Firebird 3.0+ Object-Oriented (OO) API.**

This document explains our architectural decision to use only the OO API and how this affects compatibility with different Firebird server versions.

---

## Key Architecture: Client Library vs Server Version

### The Critical Distinction

```
┌─────────────────────────────────────────────────────────────────────┐
│                        PHP Application                               │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    php-firebird Extension                            │
│                    (Uses OO API Exclusively)                         │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│              Firebird CLIENT Library (3.0+ Required)                 │
│                                                                      │
│   • Provides OO API interfaces (IAttachment, ITransaction, etc.)    │
│   • Handles wire protocol negotiation with server                    │
│   • Maintains backward compatibility with older servers              │
└─────────────────────────────────────────────────────────────────────┘
                                │
                         (Network Protocol)
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                   Firebird SERVER (Any Version)                      │
│                                                                      │
│   Supported: 2.5, 3.0, 4.0, 5.0                                     │
│   Wire protocol auto-negotiated by client library                    │
└─────────────────────────────────────────────────────────────────────┘
```

### Version Requirements

| Component | Minimum Version | Recommended | Notes |
|-----------|-----------------|-------------|-------|
| **Firebird Client Library** | 3.0 | 5.0+ | **REQUIRED** for OO API |
| **Firebird Server** | 2.5 | 4.0+ | Any version works |
| **PHP** | 8.1 | 8.4+ | Extension requirement |

### Compile-Time Enforcement

The extension enforces FB 3.0+ requirement at compile time in `php_firebird.h`:

```c
#if FB_API_VER < 30
  #error "FATAL: This extension requires Firebird 3.0+ OO API (FB_API_VER >= 30)"
#endif
```

This fails the build immediately with a clear error message if attempted with an older Firebird client library.

---

## Why OO API Only?

### 1. Simplified Codebase Maintenance

**Before (Dual API Support):**
```c
/* Every operation required dual paths */
if (using_legacy_api) {
    isc_dsql_execute(status, &tr_handle, &stmt_handle, ...);
} else {
    fbs_execute(master, statement, transaction, ...);
}
```

**After (OO API Only):**
```c
/* Single, clean code path */
fbs_execute(master, statement, transaction, in_buffer, in_meta, ...);
```

### 2. Modern Resource Management

The OO API provides:
- Opaque pointer handles (type safety)
- Built-in reference counting
- Automatic cleanup via release() methods
- Clear ownership semantics

### 3. Consistent Behavior

- Same API behavior across all server versions
- No version-specific code branches
- Predictable error handling via IStatus

### 4. Future-Proof

- Legacy `isc_*` API deprecated by Firebird project
- OO API is the forward path for Firebird development
- New Firebird features only exposed via OO API

---

## Backward Compatibility: How It Works

### Wire Protocol Negotiation

When a Firebird 4.0 client connects to a Firebird 2.5 server:

1. **Client initiates connection** with its highest supported protocol version
2. **Server responds** with the highest mutually supported version
3. **Protocol downgrade** happens automatically if needed
4. **OO API abstracts** the wire protocol differences

### Example: Docker Test Infrastructure

```yaml
# docker-compose.yml
services:
  php84-dev:
    image: php:8.4-cli
    # Firebird 4.0.5 client library installed
    # Provides OO API interfaces
    
  firebird25:
    image: jacobalberty/firebird:2.5-ss
    # Firebird 2.5 server
    # Accessed via FB 4.0.5 client's OO API
```

**Verification:**
```bash
# This works because FB 4.0 client negotiates with FB 2.5 server
./scripts/host/test_matrix.sh php84-dev firebird25
```

---

## Removed Legacy API Functions

The following `isc_*` functions are no longer used directly in execution paths:

### Connection Management
| Legacy Function | OO API Replacement |
|-----------------|-------------------|
| `isc_attach_database()` | `fbc_connect()` via IProvider::attachDatabase |
| `isc_detach_database()` | `fbc_close()` via IAttachment::detach |
| `isc_create_database()` | `fbc_create_database()` via IProvider::createDatabase |
| `isc_drop_database()` | `fbc_drop_database()` via IAttachment::dropDatabase |

### Transaction Management
| Legacy Function | OO API Replacement |
|-----------------|-------------------|
| `isc_start_transaction()` | `fbt_start()` via IAttachment::startTransaction |
| `isc_commit_transaction()` | `fbt_commit()` via ITransaction::commit |
| `isc_rollback_transaction()` | `fbt_rollback()` via ITransaction::rollback |
| `isc_commit_retaining()` | `fbt_commit_retaining()` via ITransaction::commitRetaining |
| `isc_rollback_retaining()` | `fbt_rollback_retaining()` via ITransaction::rollbackRetaining |

### Statement Execution
| Legacy Function | OO API Replacement |
|-----------------|-------------------|
| `isc_dsql_prepare()` | `fbs_prepare()` via IAttachment::prepare |
| `isc_dsql_execute()` | `fbs_execute()` via IStatement::execute |
| `isc_dsql_execute2()` | `fbs_execute()` with output buffer |
| `isc_dsql_fetch()` | `fbs_fetch_next()` via IResultSet::fetchNext |
| `isc_dsql_free_statement()` | `fbs_free()` via IStatement::free |

### BLOB Operations
| Legacy Function | OO API Replacement |
|-----------------|-------------------|
| `isc_create_blob2()` | `fbb_create()` via IAttachment::createBlob |
| `isc_open_blob2()` | `fbb_open()` via IAttachment::openBlob |
| `isc_put_segment()` | `fbb_put_segment()` via IBlob::putSegment |
| `isc_get_segment()` | `fbb_get_segment()` via IBlob::getSegment |
| `isc_close_blob()` | `fbb_close()` via IBlob::close |

### Exceptions (Kept Temporarily)
Some legacy functions remain for features not yet migrated:
- `isc_array_get_slice()` / `isc_array_put_slice()` - Array handling
- Event functions are being migrated to `fbe_*` wrappers

---

## OO API Message Buffer Architecture

### The Core Difference

**Legacy XSQLDA:**
```c
/* Self-describing structure with embedded data pointers */
typedef struct {
    short   version;
    short   sqln;      /* Allocated count */
    short   sqld;      /* Used count */
    XSQLVAR sqlvar[1]; /* Variable-length array */
} XSQLDA;

/* Each variable has its own buffer */
typedef struct {
    short   sqltype;
    short   sqllen;
    char*   sqldata;   /* Pointer to actual data */
    short*  sqlind;    /* Null indicator */
    /* ... */
} XSQLVAR;
```

**OO API Message Buffers:**
```c
/* Metadata describes the layout */
IMessageMetadata* metadata = statement->getInputMetadata();

/* Flat buffer with data at computed offsets */
unsigned char* buffer = malloc(metadata->getMessageLength());

/* Access via metadata offsets */
for (int i = 0; i < metadata->getCount(); i++) {
    unsigned offset = metadata->getOffset(i);
    unsigned length = metadata->getLength(i);
    /* Data lives at buffer + offset */
}
```

### Parameter Binding Flow

```
PHP Value → _php_fbird_bind() → XSQLDA → [TRANSFER] → Message Buffer → OO API Execute
                                              ↑
                                     THIS STEP WAS MISSING!
```

The fix requires transferring bound XSQLDA values to the OO API message buffer format before execution.

---

## Testing with Different Server Versions

### Local Docker Testing

```bash
# Test against Firebird 2.5 server
./scripts/host/test_matrix.sh php84-dev firebird25

# Test against Firebird 3.0 server
./scripts/host/test_matrix.sh php84-dev firebird30

# Test against Firebird 4.0 server
./scripts/host/test_matrix.sh php84-dev firebird40

# All use the same OO API client library!
```

### CI/CD Matrix

The test matrix validates OO API client against multiple server versions:

| Client Version | Server Versions Tested |
|---------------|------------------------|
| FB 4.0.5 | 2.5, 3.0, 4.0, 5.0 |

---

## Migration Notes

### For Extension Developers

When adding new features:
1. **Always use OO API wrappers** (`fb*_` prefix functions)
2. **Never add new `isc_*` calls** to execution paths
3. **Use message buffers** for parameter passing (not XSQLDA direct)
4. **Test against FB 2.5** to ensure backward compatibility

### For Users

**Requirements:**
- Install Firebird 3.0+ **client library** (not the server)
- Server can be any version from 2.5 to 5.0

**Example (Debian/Ubuntu):**
```bash
# Install Firebird 4.0 client library
apt-get install firebird-dev

# Connect to any server version
$conn = fbird_connect('fb25-server:/path/to/db.fdb', 'user', 'pass');  # Works!
$conn = fbird_connect('fb40-server:/path/to/db.fdb', 'user', 'pass');  # Works!
```

---

## References

- [Firebird 3.0 Release Notes - OO API](https://firebirdsql.org/file/documentation/release_notes/html/en/3_0/rlsnotes30.html#rnfb30-apiods-api-oo)
- [Firebird OO API Guide](https://firebirdsql.org/file/documentation/html/en/firebirddocs/firebird-interfaces/firebird-interfaces.html)
- `docs/development/FIREBIRD_OO_API_REFERENCE.md` - Our internal API reference
- `firebird_utils.h` / `firebird_utils.cpp` - OO API wrapper implementations

---

## Document History

| Date | Version | Changes |
|------|---------|---------|
| 2025-12-13 | 1.0 | Initial policy document |
