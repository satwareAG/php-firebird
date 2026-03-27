# Firebird 5.0 Client API Research

## Overview
This document outlines the research findings for Firebird 3.0, 4.0, and 5.0 client API changes relevant to `php-firebird` v10 release preparation.

## Firebird API Version Gates (FB_API_VER)

| Version | FB_API_VER | Key Changes |
|---------|------------|-------------|
| Firebird 3.0 | 30 | `isc_dsql_sql_info` new codes, wire protocol compression, enhanced security |
| Firebird 4.0 | 40 | Timezone support (`ISC_TIME_TZ`, `ISC_TIMESTAMP_TZ`), `isc_blob_set_data`, `isc_blob_get_data` |
| Firebird 5.0 | 50 | `INT128` native type, Protocol v16, parallel backup/restore support |

## Firebird 5.0 Specific Findings

### 1. INT128 Support
Firebird 5.0 introduces a native 128-bit integer type.
- **SQLType Codes**:
  - `INT128`: 600 (nullable: 601)
  - `INT128` (scaled/Numeric): 610/611, 620/621
- **Struct**: `FB_INT128` (16 bytes).
- **Impact**: `XSQLVAR` processing must handle these new codes to prevent "unknown data type" errors.

### 2. Protocol and Connection
- **Protocol Version**: 16 (upgraded from 15 in 4.0).
- **Wire Compression**: Enabled by default in 5.0.
- **FB_API_VER**: Recommended to set `FB_API_VER=50` for full feature access.

### 3. Handle Types
All handle types remain `void*` pointers in the C API, which supports our "void star elimination" strategy for v10.
- `isc_db_handle`
- `isc_tr_handle`
- `isc_blob_handle`
- `isc_req_handle`
- `isc_svc_handle`

## Firebird 4.0 Legacy (Still Relevant)

### 1. Timezone-Aware Types
- `ISC_TIME_TZ` and `ISC_TIMESTAMP_TZ` structs.
- API Functions: `isc_decode_timestamp_tz`, `isc_encode_timestamp_tz`.

### 2. Streaming Blob API
- `isc_blob_set_data` / `isc_blob_get_data` available since 4.0.
- Optimization: Can replace segment-based `isc_get_segment` loops for improved performance.

## GitHub Issues (satwareAG/php-firebird)

Current blockers identified:
1. **#1 & #4: PHP 8.4 Support**: `zend_object` structure changes and removal of `stdobj`.
2. **#2: Firebird 5.0.2 Build Failure**: Related to `FB_API_VER` changes and headers.
3. **#3: Firebird 5.0 Test Failures**: `INT128` and Timezone type mismatches in existing tests.

## Conclusion for v10 Prep
- INT128 support is mandatory for Firebird 5.0 compatibility.
- PHP 8.4 compatibility requires refactoring object initialization.
- The legacy wrapper approach for handles remains valid as they are all pointers.
