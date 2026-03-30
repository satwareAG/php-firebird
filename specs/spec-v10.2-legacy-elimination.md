# Spec: v10.2.0 - Full Legacy Elimination

**Issues**: #141 (events), #142 (bridge removal), #143 (coverage)
**Milestone**: v10.2.0
**Status**: Active
**Depends on**: v10.1.0 merged

## Goal

Remove all remaining uses of `fbc_get_legacy_handle_ptr()` and the underlying
`legacy_handle_` field. After this milestone, zero legacy isc_db_handle bridge code
remains in the extension.

## Implementation: Issue #141 (fbird_events.c - 4 sites)

### Site 1: fbird_wait_event - init wait (lines ~201-213)

Before:
```c
ISC_STATUS init_status[20];
ISC_ULONG init_counts[15];
void *db_handle_ptr = fbc_get_legacy_handle_ptr(ib_link->fbc_connection);
if (fbe_wait_for_event(init_status, db_handle_ptr, buffer_size, event_buffer, result_buffer)) {
```

After:
```c
ISC_STATUS init_status[20];
ISC_ULONG init_counts[15];
void *attachment_ptr = fbc_get_attachment(ib_link->fbc_connection);
if (fbe_wait_for_event_oo(init_status, attachment_ptr, buffer_size, event_buffer, result_buffer)) {
```

### Site 2: fbird_wait_event - actual wait (lines ~213-220)

Before:
```c
void *db_handle_ptr = fbc_get_legacy_handle_ptr(ib_link->fbc_connection);
if (fbe_wait_for_event(IB_STATUS, db_handle_ptr, buffer_size, event_buffer, result_buffer)) {
```

After:
```c
void *attachment_ptr = fbc_get_attachment(ib_link->fbc_connection);
if (fbe_wait_for_event_oo(IB_STATUS, attachment_ptr, buffer_size, event_buffer, result_buffer)) {
```

### Site 3: fbird_poll_event - baseline init (lines ~406)

Before:
```c
void *db_handle_ptr = fbc_get_legacy_handle_ptr(event->link->fbc_connection);
if (fbe_wait_for_event(init_status, db_handle_ptr,
        event->buffer_size, event->event_buffer, event->result_buffer)) {
```

After:
```c
void *attachment_ptr = fbc_get_attachment(event->link->fbc_connection);
if (fbe_wait_for_event_oo(init_status, attachment_ptr,
        event->buffer_size, event->event_buffer, event->result_buffer)) {
```

### Site 4: fbird_poll_event - actual wait (lines ~469)

Before:
```c
wait_result = fbe_wait_for_event(IB_STATUS, fbc_get_legacy_handle_ptr(event->link->fbc_connection),
        event->buffer_size, event->event_buffer, event->result_buffer);
```

After:
```c
wait_result = fbe_wait_for_event_oo(IB_STATUS, fbc_get_attachment(event->link->fbc_connection),
        event->buffer_size, event->event_buffer, event->result_buffer);
```

## Implementation: Issue #142 (Remove bridge - 4 files)

After #141 is verified: `fbc_get_legacy_handle_ptr()` has zero callers. Remove:

### firebird_utils.h

Remove the declaration:
```c
void* fbc_get_legacy_handle_ptr(void* connection);
```

### firebird_utils.cpp

Remove the implementation (lines 709-714):
```cpp
void* fbc_get_legacy_handle_ptr(void* connection) {
    if (!connection) return nullptr;
    auto* conn = static_cast<fb::Connection*>(connection);
    return conn->getLegacyHandlePtr();
}
```

### src/cpp/fb_connection.hpp

Remove `getLegacyHandle()` and `getLegacyHandlePtr()` methods:
```cpp
[[nodiscard]] isc_db_handle getLegacyHandle() const noexcept {
    return legacy_handle_;
}

[[nodiscard]] isc_db_handle* getLegacyHandlePtr() noexcept {
    return &legacy_handle_;
}
```

Remove `legacy_handle_` field from private section:
```cpp
isc_db_handle legacy_handle_ = 0;       ///< Legacy handle (for compatibility)
```

Remove `legacy_handle_` from constructor and move operations.

## Implementation: Issue #143 (Coverage gate)

### New test: tests/fbird_events_error_001.phpt

Test event error handling path (connection invalid during wait). This covers the
`event->state = DEAD` and `RETURN_FALSE` paths in `fbird_poll_event`.

### Coverage workflow change

In `.github/workflows/coverage.yml`:
```yaml
# Before:
COVERAGE_THRESHOLD: 54.0

# After:
COVERAGE_THRESHOLD: 65.0
```

## Success Criteria

- All 12 CI jobs pass
- Zero references to `fbc_get_legacy_handle_ptr` in source after #142
- Zero references to `legacy_handle_` in source after #142
- Coverage >= 65.0% on CI coverage job
- Event tests pass: `tests/event_poller_wrapper.phpt`
