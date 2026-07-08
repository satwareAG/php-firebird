---
description: >-
  v12.1.0 Firebird Client Coverage: verify every IAttachment/ITransaction/
  IStatement/IResultSet/IBlob/IEvents/IService/IMessageMetadata/IMetadataBuilder/
  IUtil/IXpbBuilder/IDtc/IRequest/IConfigManager method is reachable through the
  PHP driver. FB3 baseline only; FB4+/5+/6+ methods skip gracefully.
tags: [client-coverage, fb3, testing, v12.1]
priority: 1
---

# Spec: v12.1.0 Firebird Client Coverage

## Issue
#325 (this spec) - index for #385-#403 (19 test issues)

## Branch
`test/integration-conformance`

## Date
2026-07-07

## Status
Approved - execution in progress

---

## Intent

Verify that every method of every Firebird OO API interface (as defined in
`~/external/firebird/src/include/firebird/FirebirdInterface.idl`) is reachable
through the PHP driver's procedural `fbird_*` and OOP `Firebird\*` APIs.

This milestone produces **aggregation smoke tests** - each test calls each
method of one interface once with minimal assertions. Edge cases are already
covered by the existing `tests/fbird_*` and `tests/oop_*` test suites. M4
provides explicit per-method reachability verification.

---

## Interface Coverage Table (FB3 baseline)

| Interface | Issue | FB3 Methods | FB4+ (skip) | FB5+ (skip) | FB6+ (skip) |
|-----------|-------|-------------|-------------|-------------|-------------|
| IAttachment | #385 | attach/detach/ping/getInfo/startTransaction/prepare/execute/openCursor/createBlob/openBlob/queEvents/cancelOperation | getIdleTimeout/setIdleTimeout/getStatementTimeout/setStatementTimeout/createBatch/createReplicator | - | getMaxBlobCacheSize/setMaxBlobCacheSize/getMaxInlineBlobSize/setMaxInlineBlobSize |
| ITransaction | #386 | commit/rollback/commitRetaining/rollbackRetaining/getInfo/prepare(2PC)/join/validate/deprecated* | - | - | - |
| IStatement | #387 | getInfo/getType/getPlan/getAffectedRecords/getInputMetadata/getOutputMetadata/execute/openCursor/setCursorName/getFlags/free | getTimeout/setTimeout/createBatch | - | getMaxInlineBlobSize/setMaxInlineBlobSize |
| IResultSet | #388 | fetchNext/getMetadata/close/isEof/isBof/setDelayedOutputFormat | - | fetchPrior/fetchFirst/fetchLast/fetchAbsolute/fetchRelative/getInfo | - |
| IBlob | #389 | getInfo/getSegment/putSegment/seek/cancel/close/deprecatedCancel/deprecatedClose | - | - | - |
| IEvents | #390 | que/cancel/eventCallbackFunction | - | - | - |
| IService | #391 | query/start/detach/deprecatedDetach | - | cancel | - |
| IMessageMetadata | #392 | getCount/getField/getRelation/getOwner/getAlias/getType/isNullable/getSubType/getLength/getScale/getCharSet/getOffset/getNullOffset/getBuilder/getMessageLength | getAlignment/getAlignedLength | - | getSchema |
| IMetadataBuilder | #393 | setType/setSubType/setLength/setCharSet/setScale/truncate/moveNameToIndex/remove/addField/getMetadata | setField/setRelation/setOwner/setAlias | - | setSchema |
| IUtil | #394 | getFbVersion/loadBlob/dumpBlob/getPerfCounters/executeCreateDatabase/decodeDate/decodeTime/encodeDate/encodeTime/formatStatus/getClientVersion/getXpbBuilder/setOffsets | getDecFloat16/34/getInt128/decodeTimeTz(Ex)/encodeTimeTz(Ex) | - | executeCreateDatabase2/convert |
| IXpbBuilder | #395 | DPB/TPB/SPB_ATTACH/SPB_START/BPB/SPB_SEND/SPB_RECEIVE/SPB_RESPONSE/INFO_SEND/INFO_RESPONSE kinds | BATCH kind | - | - |
| IDtc | #396 | join/startBuilder/addAttachment/addWithTpb/start | - | - | - |
| IRequest | #397 | compile/receive/send/getInfo/start/startAndSend/unwind/free | - | - | - |
| IConfigManager | #398 | getDirectory/getFirebirdConf/getDatabaseConf/getPluginConfig/getInstallDirectory/getRootDirectory | getDefaultSecurityDb | - | - |

---

## FB3 SQL Features (#399)

| Feature | BLR opcode | Status |
|---------|------------|--------|
| BOOLEAN type | dtype_boolean=21 | Test |
| Sub-routines | blr_subproc/subfunc | Test |
| Window functions | blr_window | Test |
| Continue loop | blr_continue_loop | Test |
| Coalesce/Decode | blr_coalesce/blr_decode | Test |
| NEXT VALUE FOR | blr_gen_id2 | Test |
| SIMILAR TO substring | blr_substring_similar | Test |
| Identity columns | - | Test |
| fb_cancel_operation | - | Test |
| Savepoints | - | Test (existing coverage) |
| CREATE OR ALTER | - | Test |
| RECREATE | - | Test |
| EXECUTE BLOCK | - | Test |
| ROWS clause | blr_skip | Test |
| RETURNING (4 forms) | - | Test |

---

## FB3 Protocol/Auth (#400-#403)

| Issue | Topic |
|-------|-------|
| #400 | Wire protocol 13 negotiation (FB5 client -> FB3 server) |
| #401 | ODS 12.0 verification |
| #402 | Legacy auth (Legacy_Auth plugin, FIREBIRD_USE_LEGACY_AUTH=Enabled) |
| #403 | SQL dialect 1 vs 3 (identifier quoting, date types) |

---

## Out of Scope

- FB 4.0+ features (M8 stretch, v13.0.0)
- FB 5.0+ features (M8 stretch)
- FB 6.0 features (M8 stretch)
- Per-method edge case testing (existing tests/ suite covers this)

---

## Issue Index

| Issue | Title |
|-------|-------|
| #385 | test: IAttachment full lifecycle (FB3 baseline) |
| #386 | test: ITransaction all commit/rollback variants (FB3 baseline) |
| #387 | test: IStatement prepare flags + execute + openCursor (FB3 baseline) |
| #388 | test: IResultSet fetchNext only (FB3 baseline) |
| #389 | test: IBlob full lifecycle (FB3 baseline) |
| #390 | test: IEvents que/cancel (FB3 baseline) |
| #391 | test: IService query/start/detach (FB3 baseline) |
| #392 | test: IMessageMetadata all getters (FB3 baseline) |
| #393 | test: IMetadataBuilder (FB3 baseline) |
| #394 | test: IUtil kitchen sink (FB3 baseline) |
| #395 | test: IXpbBuilder all kinds (FB3 baseline) |
| #396 | test: IDtc / IDtcStart distributed transactions (FB3 baseline) |
| #397 | test: IRequest BLR (FB3 baseline) |
| #398 | test: IConfigManager / IFirebirdConf (FB3 baseline) |
| #399 | test: FB3 SQL features smoke (BOOLEAN, sub-routines, window functions, etc.) |
| #400 | test: FB3 wire protocol 13 negotiation |
| #401 | test: FB3 ODS 12.0 specific behaviors |
| #402 | test: FB3 legacy auth (Legacy_Auth plugin) |
| #403 | test: FB3 SQL dialect 1 vs 3 |
