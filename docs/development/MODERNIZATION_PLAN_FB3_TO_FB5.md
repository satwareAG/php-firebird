# Modernization Plan: PHP-Firebird Extension for Firebird 3.0 - 5.0

## Executive Summary

This document outlines a comprehensive plan for modernizing the PHP-Firebird extension to fully support Firebird client libraries from version 3.0 through 5.0 using the modern C++ Object-Oriented API.

**Current State**: Mixed C/C++ codebase using legacy isc_* functional API with partial C++ OO API integration  
**Target State**: Full C++ OO API implementation with backward compatibility layer for Firebird 3.0+  
**Timeline**: Estimated 8-12 weeks for full implementation

---

## 1. Background and Rationale

### 1.1 Why Move to C++ OO API?

The DeepWiki research confirms critical architectural changes in Firebird:

1. **Version 3.0 Introduction**: Complete new interface-based OO API replacing traditional function-based API
2. **Plugin Architecture**: New extensible plugin system
3. **Provider System**: Database access through provider plugins
4. **Backward Compatibility**: Old API implemented as thin layer over OO API (performance impact minimal)

**Benefits of C++ OO API migration:**

| Aspect | Legacy API (isc_*) | OO API (C++ Interfaces) |
|--------|-------------------|------------------------|
| Type Safety | Manual casting, void pointers | Strong typing, templates |
| Error Handling | ISC_STATUS arrays | Exception-based + status wrappers |
| Resource Management | Manual malloc/free | RAII patterns |
| Versioning | None | Built-in interface versioning |
| New Features | Limited | Full access to FB 4.0/5.0 features |
| Maintainability | Complex | Cleaner, self-documenting |

### 1.2 Firebird Version Feature Matrix

Based on DeepWiki research (FirebirdSQL/firebird):

| Feature | FB 3.0 | FB 4.0 | FB 5.0 |
|---------|--------|--------|--------|
| OO Interface API | ✅ | ✅ | ✅ |
| Plugin Architecture | ✅ | ✅ | ✅ |
| Timeout Support (attachment/statement) | ❌ | ✅ | ✅ |
| Batch API | ❌ | ✅ | ✅ |
| Replication API | ❌ | ✅ | ✅ |
| DECFLOAT Data Type | ❌ | ✅ | ✅ |
| INT128 Data Type | ❌ | ✅ | ✅ |
| Time Zone Support | ❌ | ✅ | ✅ |
| BINARY/VARBINARY Types | ❌ | ✅ | ✅ |
| Blob Caching (client-side) | ❌ | ❌ | ✅ |
| Service Cancellation | ❌ | ✅ | ✅ |
| Parallel Execution | ❌ | ❌ | ✅ |
| SKIP LOCKED | ❌ | ❌ | ✅ |
| Compiled Statement Cache | ❌ | ❌ | ✅ |
| Profiler | ❌ | ❌ | ✅ |

---

## 2. Current Codebase Analysis

### 2.1 File Structure

```
php-firebird/
├── firebird.c           # Main extension entry point (1500+ lines C)
├── fbird_blobs.c        # Blob handling (C)
├── fbird_events.c       # Event handling (C)
├── fbird_inspection.c   # Metadata inspection (C)
├── fbird_metadata.c     # Field/param metadata (C)
├── fbird_query_*.c      # Query execution (6 files, C)
├── fbird_result.c       # Result handling (C)
├── fbird_service.c      # Service API (C)
├── fbird_udf.c          # UDF support (C)
├── firebird_utils.cpp   # C++ OO API wrappers (already exists)
├── firebird_utils.h     # C/C++ interface header
└── firebird_utils_internal.h  # Internal C++ utilities
```

### 2.2 Existing C++ Integration

The codebase already has partial C++ integration in `firebird_utils.cpp`:

**Already Implemented in C++:**
- `fbu_get_client_version()` - Uses IMaster interface
- `fbu_encode_time()` / `fbu_encode_date()` - IUtil interface
- `fbu_decode_time_tz()` / `fbu_decode_timestamp_tz()` - FB4.0+ timezone handling
- `fbu_encode_time_tz()` / `fbu_encode_timestamp_tz()` - FB4.0+ timezone encoding
- `fbu_insert_field_info()` - IStatement metadata
- `fbu_insert_aliases()` - IStatement alias extraction

**Modern C++ Features Already Used:**
- `std::optional` for nullable returns
- `std::string_view` for efficient string handling
- RAII wrappers (FirebirdMasterWrapper, FirebirdStatusManager, FirebirdMetadataWrapper)
- `extern "C"` linkage for C interop
- `noexcept` specifications
- Structured bindings (C++17)

### 2.3 Legacy API Usage (To Be Migrated)

Functions still using legacy isc_* API:

```c
// Connection (firebird.c)
isc_attach_database()    → IProvider::attachDatabase()
isc_detach_database()    → IAttachment::detach()
isc_drop_database()      → IAttachment::dropDatabase()

// Transactions (firebird.c)
isc_start_transaction()  → IAttachment::startTransaction()
isc_start_multiple()     → IProvider::startTransaction() (multi-db)
isc_commit_transaction() → ITransaction::commit()
isc_rollback_transaction() → ITransaction::rollback()
isc_commit_retaining()   → ITransaction::commitRetaining()
isc_rollback_retaining() → ITransaction::rollbackRetaining()

// Statements (fbird_query_*.c)
isc_dsql_allocate_statement() → IAttachment::prepare() / creates IStatement
isc_dsql_prepare()       → IAttachment::prepare()
isc_dsql_describe()      → IStatement::getInputMetadata/getOutputMetadata()
isc_dsql_describe_bind() → IStatement::getInputMetadata()
isc_dsql_execute()       → IStatement::execute()
isc_dsql_execute2()      → IStatement::execute()
isc_dsql_fetch()         → IResultSet::fetchNext()
isc_dsql_free_statement() → IStatement::free() / release()
isc_dsql_exec_immed2()   → IAttachment::execute()

// Blobs (fbird_blobs.c)
isc_create_blob2()       → IAttachment::createBlob()
isc_open_blob2()         → IAttachment::openBlob()
isc_put_segment()        → IBlob::putSegment()
isc_get_segment()        → IBlob::getSegment()
isc_close_blob()         → IBlob::close()
isc_cancel_blob()        → IBlob::cancel()
isc_blob_info()          → IBlob::getInfo()

// Events (fbird_events.c)
isc_que_events()         → IAttachment::queEvents()
isc_cancel_events()      → IEvents::cancel()
isc_wait_for_event()     → (use queEvents + sync handling)

// Services (fbird_service.c)
isc_service_attach()     → IProvider::attachServiceManager()
isc_service_detach()     → IService::detach()
isc_service_query()      → IService::query()
isc_service_start()      → IService::start()

// Information (various)
isc_database_info()      → IAttachment::getInfo()
isc_transaction_info()   → ITransaction::getInfo()
isc_dsql_sql_info()      → IStatement::getInfo()
```

---

## 3. Architecture Design

### 3.1 Proposed New Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     PHP Extension Layer                         │
│   (firebird.c - PHP function declarations, arginfo, module)     │
└────────────────────────────┬────────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────────┐
│                   C++ Implementation Layer                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ fb_connect.cpp│  │ fb_query.cpp │  │ fb_blob.cpp  │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ fb_trans.cpp │  │ fb_event.cpp │  │ fb_service.cpp│          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
└────────────────────────────┬────────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────────┐
│               C++ RAII Wrapper Layer (fb_core.hpp)               │
│  ┌────────────────┐  ┌──────────────────┐  ┌─────────────────┐ │
│  │ AttachmentPtr  │  │ TransactionPtr   │  │ StatementPtr    │ │
│  │ (unique_ptr)   │  │ (unique_ptr)     │  │ (unique_ptr)    │ │
│  └────────────────┘  └──────────────────┘  └─────────────────┘ │
│  ┌────────────────┐  ┌──────────────────┐  ┌─────────────────┐ │
│  │ BlobPtr        │  │ ResultSetPtr     │  │ ServicePtr      │ │
│  └────────────────┘  └──────────────────┘  └─────────────────┘ │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │ StatusWrapper, MetadataWrapper, MasterWrapper              │ │
│  └────────────────────────────────────────────────────────────┘ │
└────────────────────────────┬────────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────────┐
│                 Firebird OO API (Interface.h)                    │
│  IMaster, IProvider, IAttachment, ITransaction, IStatement...   │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 Core RAII Wrappers Design

```cpp
// fb_core.hpp - Core RAII types

namespace fb {

// Forward declarations
struct AttachmentDeleter;
struct TransactionDeleter;
struct StatementDeleter;
struct BlobDeleter;
struct ResultSetDeleter;
struct ServiceDeleter;

// Smart pointer types with custom deleters
using AttachmentPtr = std::unique_ptr<Firebird::IAttachment, AttachmentDeleter>;
using TransactionPtr = std::unique_ptr<Firebird::ITransaction, TransactionDeleter>;
using StatementPtr = std::unique_ptr<Firebird::IStatement, StatementDeleter>;
using BlobPtr = std::unique_ptr<Firebird::IBlob, BlobDeleter>;
using ResultSetPtr = std::unique_ptr<Firebird::IResultSet, ResultSetDeleter>;
using ServicePtr = std::unique_ptr<Firebird::IService, ServiceDeleter>;

// Version-aware feature detection
class VersionInfo {
public:
    static constexpr unsigned FB30 = 0x0300;
    static constexpr unsigned FB40 = 0x0400;
    static constexpr unsigned FB50 = 0x0500;
    
    [[nodiscard]] bool hasTimeouts() const noexcept { return version_ >= FB40; }
    [[nodiscard]] bool hasBatchAPI() const noexcept { return version_ >= FB40; }
    [[nodiscard]] bool hasTimezones() const noexcept { return version_ >= FB40; }
    [[nodiscard]] bool hasBlobCaching() const noexcept { return version_ >= FB50; }
    [[nodiscard]] bool hasProfiler() const noexcept { return version_ >= FB50; }
    [[nodiscard]] bool hasParallelExec() const noexcept { return version_ >= FB50; }
    
private:
    unsigned version_;
};

// Connection class wrapping IAttachment
class Connection {
public:
    // Factory method - returns optional for error handling
    [[nodiscard]] static std::optional<Connection> create(
        IMaster* master,
        std::string_view database,
        std::string_view user = {},
        std::string_view password = {},
        std::string_view charset = {},
        std::string_view role = {},
        unsigned dialect = SQL_DIALECT_CURRENT
    ) noexcept;
    
    // Transaction management
    [[nodiscard]] std::optional<Transaction> startTransaction(
        const TransactionParams& params = {}
    ) noexcept;
    
    // Statement preparation
    [[nodiscard]] std::optional<Statement> prepare(
        Transaction& trans,
        std::string_view sql
    ) noexcept;
    
    // Direct execution
    [[nodiscard]] bool execute(
        Transaction& trans,
        std::string_view sql
    ) noexcept;
    
    // FB 4.0+ Timeout support
    void setStatementTimeout(unsigned milliseconds) noexcept;
    void setIdleTimeout(unsigned seconds) noexcept;
    
    // FB 5.0+ Blob caching
    void setMaxBlobCacheSize(unsigned size) noexcept;
    void setMaxInlineBlobSize(unsigned size) noexcept;
    
private:
    AttachmentPtr attachment_;
    VersionInfo version_;
};

} // namespace fb
```

### 3.3 PHP Resource Structure Update

```cpp
// Updated internal structures for OO API

struct fbird_db_link_v2 {
    fb::Connection connection;           // RAII connection wrapper
    unsigned short dialect;
    std::vector<fbird_transaction_v2*> transactions;  // Transaction list
    std::vector<fbird_event*> events;    // Event handlers
    
    // Version-specific feature flags
    bool supports_timeout;
    bool supports_batch;
    bool supports_timezone;
};

struct fbird_transaction_v2 {
    fb::Transaction transaction;         // RAII transaction wrapper
    std::vector<fbird_db_link_v2*> links; // Multi-database support
    zend_long affected_rows;
};

struct fbird_query_v2 {
    fb::Statement statement;             // RAII statement wrapper
    fb::ResultSet result;                // RAII result set (if query)
    fbird_db_link_v2* link;
    fbird_transaction_v2* trans;
    
    // Metadata (cached from IMessageMetadata)
    std::vector<FieldInfo> out_fields;
    std::vector<FieldInfo> in_params;
};
```

---

## 4. Implementation Phases

### Phase 1: Core Infrastructure (Weeks 1-2)

**Goals:**
- Complete RAII wrapper framework
- Version detection and feature flags
- Error handling standardization

**Tasks:**
1. Create `src/fb_core.hpp` - Core RAII types and utilities
2. Create `src/fb_status.hpp` - Status/exception handling
3. Create `src/fb_version.hpp` - Version detection and feature flags
4. Update `firebird_utils_internal.h` - Integrate new wrappers
5. Add comprehensive unit tests for RAII wrappers

**Files to Create:**
```
src/cpp/
├── fb_core.hpp           # Core types, smart pointers
├── fb_status.hpp         # Status handling, exceptions
├── fb_version.hpp        # Version detection
├── fb_dpb_builder.hpp    # DPB construction (modern)
├── fb_tpb_builder.hpp    # TPB construction (modern)
└── fb_metadata.hpp       # Metadata handling
```

### Phase 2: Connection Layer (Weeks 3-4)

**Goals:**
- Migrate connection handling to OO API
- Implement connection pooling foundation
- Add FB 4.0/5.0 timeout support

**Tasks:**
1. Create `src/cpp/fb_connection.cpp` - OO API connection implementation
2. Migrate `_php_fbird_attach_db()` to use IProvider::attachDatabase()
3. Migrate `_php_fbird_close_link()` to use IAttachment::detach()
4. Add `fbird_set_idle_timeout()` function (FB 4.0+)
5. Add `fbird_set_statement_timeout()` function (FB 4.0+)
6. Update DPB building to use modern OO approach

**New PHP Functions:**
```php
// FB 4.0+ features
fbird_set_idle_timeout(resource $connection, int $seconds): bool
fbird_set_statement_timeout(resource $connection, int $milliseconds): bool
fbird_get_idle_timeout(resource $connection): int
fbird_get_statement_timeout(resource $connection): int

// FB 5.0+ features  
fbird_set_blob_cache_size(resource $connection, int $size): bool
fbird_set_inline_blob_size(resource $connection, int $size): bool
```

### Phase 3: Transaction Layer (Weeks 5-6)

**Goals:**
- Migrate transaction handling to OO API
- Improve TPB building
- Add transaction info queries

**Tasks:**
1. Create `src/cpp/fb_transaction.cpp` - OO API transaction implementation
2. Migrate `fbird_trans()` to use IAttachment::startTransaction()
3. Migrate commit/rollback functions
4. Improve `_php_fbird_populate_trans()` with modern TPB builder
5. Add `fbird_trans_info()` enhancements using ITransaction::getInfo()

### Phase 4: Query Execution (Weeks 7-8)

**Goals:**
- Migrate statement handling to OO API
- Implement IResultSet-based fetching
- Add batch API support (FB 4.0+)

**Tasks:**
1. Create `src/cpp/fb_statement.cpp` - OO API statement implementation
2. Create `src/cpp/fb_resultset.cpp` - OO API result handling
3. Migrate `_php_fbird_alloc_query()` to use IAttachment::prepare()
4. Migrate fetch operations to use IResultSet::fetchNext()
5. Add batch execution support (FB 4.0+)

**New PHP Functions:**
```php
// FB 4.0+ Batch API
fbird_batch_create(resource $connection, string $sql, array $options = []): resource
fbird_batch_add(resource $batch, array $parameters): bool
fbird_batch_execute(resource $batch): array // Returns affected rows per statement
fbird_batch_cancel(resource $batch): bool
```

### Phase 5: Blob Handling (Week 9)

**Goals:**
- Migrate blob handling to OO API
- Add FB 5.0 blob caching support

**Tasks:**
1. Create `src/cpp/fb_blob.cpp` - OO API blob implementation
2. Migrate create/open/put/get operations to IBlob interface
3. Add client-side blob caching support (FB 5.0+)
4. Improve stream wrapper efficiency

### Phase 6: Events and Services (Week 10)

**Goals:**
- Migrate event handling to OO API
- Migrate service API to OO API
- Add service cancellation (FB 4.0+)

**Tasks:**
1. Create `src/cpp/fb_events.cpp` - OO API event implementation
2. Create `src/cpp/fb_service.cpp` - OO API service implementation
3. Migrate event queue/cancel operations
4. Add `fbird_service_cancel()` function (FB 4.0+)

**New PHP Functions:**
```php
// FB 4.0+ Service features
fbird_service_cancel(resource $service): bool
```

### Phase 7: Testing and Documentation (Weeks 11-12)

**Goals:**
- Comprehensive test coverage
- Documentation updates
- Performance benchmarking

**Tasks:**
1. Create version-specific test suites (FB 3.0, 4.0, 5.0)
2. Update Docker test matrix for all versions
3. Performance benchmarks comparing legacy vs OO API
4. Update README and API documentation
5. Create migration guide for users

---

## 5. Backward Compatibility Strategy

### 5.1 Runtime Version Detection

```cpp
// At module initialization
PHP_MINIT_FUNCTION(fbird) {
    // Detect client library version
    unsigned client_version = fbu_get_client_version(master_instance);
    
    // Set feature flags
    IBG(supports_timeout) = (client_version >= 0x0400);
    IBG(supports_batch) = (client_version >= 0x0400);
    IBG(supports_timezone) = (client_version >= 0x0400);
    IBG(supports_blob_cache) = (client_version >= 0x0500);
    IBG(supports_profiler) = (client_version >= 0x0500);
    
    // Register version-specific constants
    REGISTER_LONG_CONSTANT("FBIRD_CLIENT_VERSION", client_version, CONST_PERSISTENT);
    
    // Register feature availability constants
    if (IBG(supports_timeout)) {
        REGISTER_LONG_CONSTANT("FBIRD_HAS_TIMEOUT", 1, CONST_PERSISTENT);
    }
    // ... etc
}
```

### 5.2 Graceful Degradation

```cpp
// Example: Timeout support with graceful degradation
PHP_FUNCTION(fbird_set_statement_timeout) {
    zval *link = NULL;
    zend_long timeout;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &link, &timeout) == FAILURE) {
        return;
    }
    
    fbird_db_link *ib_link = get_link(link);
    
    if (!IBG(supports_timeout)) {
        php_error_docref(NULL, E_WARNING, 
            "Statement timeout requires Firebird client 4.0 or later (current: %d.%d)",
            IBG(client_major_version), IBG(client_minor_version));
        RETURN_FALSE;
    }
    
    // Use OO API
    try {
        ib_link->connection->setStatementTimeout(static_cast<unsigned>(timeout));
        RETURN_TRUE;
    } catch (const fb::Exception& e) {
        _php_fbird_error_from_exception(e);
        RETURN_FALSE;
    }
}
```

### 5.3 API Compatibility

All existing functions remain unchanged in signature:
- `fbird_connect()`, `fbird_pconnect()`, `fbird_close()`
- `fbird_query()`, `fbird_prepare()`, `fbird_execute()`
- `fbird_fetch_*()`, `fbird_num_fields()`, etc.

New functionality exposed through:
- New functions (`fbird_set_statement_timeout()`, etc.)
- New constants (`FBIRD_HAS_TIMEOUT`, etc.)
- Enhanced `fbird_trans_info()` return values

---

## 6. Build System Updates

### 6.1 config.m4 Modifications

```m4
dnl Check for C++ compiler
AC_PROG_CXX
AC_LANG_PUSH([C++])
AX_CXX_COMPILE_STDCXX([17], [noext], [mandatory])
AC_LANG_POP([C++])

dnl Firebird client detection
PHP_ARG_WITH(firebird, for Firebird support,
[  --with-firebird[=DIR]    Include Firebird support])

if test "$PHP_FIREBIRD" != "no"; then
  dnl ... existing detection ...
  
  dnl Check for OO API headers
  AC_CHECK_HEADER([firebird/Interface.h], [], [
    AC_MSG_ERROR([Firebird OO API headers not found. Firebird 3.0+ required.])
  ])
  
  dnl Check minimum version
  AC_MSG_CHECKING([for Firebird client version >= 3.0])
  dnl ... version check ...
  
  PHP_REQUIRE_CXX()
  PHP_ADD_LIBRARY(stdc++, 1, FIREBIRD_SHARED_LIBADD)
  
  PHP_NEW_EXTENSION(firebird, 
    firebird.c fbird_blobs.c fbird_events.c fbird_inspection.c \
    fbird_metadata.c fbird_query.c fbird_query_array.c fbird_query_bind.c \
    fbird_query_exec.c fbird_query_prepare.c fbird_result.c fbird_service.c \
    fbird_udf.c \
    firebird_utils.cpp \
    src/cpp/fb_connection.cpp src/cpp/fb_transaction.cpp \
    src/cpp/fb_statement.cpp src/cpp/fb_blob.cpp \
    src/cpp/fb_events.cpp src/cpp/fb_service.cpp,
    $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
```

### 6.2 CMake Support (Optional)

Add `CMakeLists.txt` for IDE integration and modern build tooling.

---

## 7. Testing Strategy

### 7.1 Docker Test Matrix

```yaml
# docker-compose.test.yml
services:
  firebird30:
    image: jacobalberty/firebird:3.0
    environment:
      FIREBIRD_USER: test
      FIREBIRD_PASSWORD: test
      
  firebird40:
    image: jacobalberty/firebird:4.0
    environment:
      FIREBIRD_USER: test
      FIREBIRD_PASSWORD: test
      
  firebird50:
    image: jacobalberty/firebird:v5.0
    environment:
      FIREBIRD_USER: test
      FIREBIRD_PASSWORD: test
      
  php-test:
    build: ./docker/php
    depends_on:
      - firebird30
      - firebird40
      - firebird50
    environment:
      FB30_HOST: firebird30
      FB40_HOST: firebird40
      FB50_HOST: firebird50
```

### 7.2 Version-Specific Tests

```
tests/
├── common/              # Tests for all versions
│   ├── connect.phpt
│   ├── transaction.phpt
│   ├── query.phpt
│   └── blob.phpt
├── fb40/                # FB 4.0+ specific tests
│   ├── timeout.phpt
│   ├── timezone.phpt
│   ├── batch.phpt
│   └── decfloat.phpt
├── fb50/                # FB 5.0+ specific tests
│   ├── blob_cache.phpt
│   ├── skip_locked.phpt
│   └── profiler.phpt
└── skipif/
    ├── skipif_fb30.inc
    ├── skipif_fb40.inc
    └── skipif_fb50.inc
```

---

## 8. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Breaking existing behavior | Medium | High | Comprehensive test suite, phased rollout |
| Performance regression | Low | Medium | Benchmarks, profiling, optional legacy fallback |
| Build complexity increase | Medium | Medium | Clear documentation, CI/CD validation |
| Memory leaks in new code | Low | High | Valgrind testing, RAII patterns, code review |
| Incomplete FB 5.0 support | Low | Low | Feature flags, graceful degradation |

---

## 9. Success Criteria

1. **All existing tests pass** - Zero regressions in current functionality
2. **Performance parity** - No more than 5% performance degradation
3. **Full FB 4.0 feature support** - Timeouts, Batch API, Timezones, DECFLOAT
4. **Full FB 5.0 feature support** - Blob caching, SKIP LOCKED, Profiler access
5. **Clean static analysis** - Zero clang-tidy warnings
6. **Documentation complete** - API docs, migration guide, examples

---

## 10. Recommendation: Proceed with C++ Migration

**Conclusion**: Based on the analysis, migrating to the C++ OO API is strongly recommended:

1. **Required for modern features** - FB 4.0/5.0 features only accessible via OO API
2. **Already partially implemented** - `firebird_utils.cpp` proves the approach works
3. **Better maintainability** - RAII, strong typing, cleaner error handling
4. **Future-proof** - OO API is the primary Firebird development path
5. **Backward compatible** - Old API is thin wrapper over OO, migration transparent

The existing C++ integration in `firebird_utils.cpp` demonstrates the pattern works well with the PHP extension architecture. The plan builds on this foundation systematically.

---

## Appendix A: Interface Mapping Reference

| Legacy Function | OO Interface | Method |
|-----------------|--------------|--------|
| `fb_get_master_interface()` | IMaster | Factory function |
| `isc_attach_database()` | IProvider | `attachDatabase()` |
| `isc_detach_database()` | IAttachment | `detach()` |
| `isc_start_transaction()` | IAttachment | `startTransaction()` |
| `isc_commit_transaction()` | ITransaction | `commit()` |
| `isc_rollback_transaction()` | ITransaction | `rollback()` |
| `isc_dsql_prepare()` | IAttachment | `prepare()` |
| `isc_dsql_execute()` | IStatement | `execute()` |
| `isc_dsql_fetch()` | IResultSet | `fetchNext()` |
| `isc_create_blob2()` | IAttachment | `createBlob()` |
| `isc_open_blob2()` | IAttachment | `openBlob()` |
| `isc_que_events()` | IAttachment | `queEvents()` |
| `isc_service_attach()` | IProvider | `attachServiceManager()` |

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-12-12 | Jane Alesi | Initial plan based on DeepWiki research |
