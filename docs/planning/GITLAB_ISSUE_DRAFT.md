# Issue: Fix Heap Use-After-Free in fbird_pconnect

**Labels:** bug, security, fuzzing

## Description

The fuzzer detected a Heap Use-After-Free (UAF) in `fbird_pconnect` when reusing persistent connections.
The issue is caused by the connection cache in `EG(regular_list)` holding a dangling pointer after the resource is closed via `fbird_close`.

## Analysis

- **Bug**: Heap Use-After-Free (UAF) in `fbird_pconnect` detected by ASan.
- **Cause**: `zend_register_resource` adds a resource to `EG(regular_list)` (integer key). We also add a pointer to it in `EG(regular_list)` (string key) for caching. When `fbird_close` is called, the resource is removed from the integer list (refcount decremented). If refcount hits 0, it is freed. But the cache entry remains, pointing to freed memory. Next `fbird_pconnect` reuses this dangling pointer.

## Attempted Fix (Reverted)

1. Introduced `le_link_cache` resource type to manage cache entries.
2. Added `hash_key` to `fbird_db_link` to allow `fbird_close` to find and remove the cache entry.
3. Updated `_php_fbird_connect` to use `le_link_cache` and `zend_hash_str_update_ptr`.
4. Updated `fbird_close` to remove from cache.

**Regression**: The attempted fix caused regressions in `tests/fbird_pconnect_001.phpt` and `tests/005.phpt`.
Likely due to `strdup` vs `estrdup` mismatch or buffer overread (MD5 hash is binary, `strdup` stops at null).

## Next Steps

1. **Fix Hash Key Storage**: Use `emalloc` + `memcpy` for `hash_key` to handle binary MD5 hashes correctly (16 bytes).
2. **Refine Cache Destructor**: `_php_fbird_free_link_cache` should use `GC_DELREF(link_res)` instead of `zend_list_delete`.
3. **Debug `fbird_close`**: Ensure `fbird_close` removes from cache (decrementing refcount) AND removes from main list (decrementing refcount).
4. **Verify with ASan**: Run `./scripts/qa.sh --mode full --fail-fast` to confirm UAF is gone.
