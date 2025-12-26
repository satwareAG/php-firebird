# UAF Fix Strategy: Persistent Connection Cache

## Current Status
- **Bug**: Heap Use-After-Free (UAF) in `fbird_pconnect` detected by ASan.
- **Cause**: `zend_register_resource` adds a resource to `EG(regular_list)` (integer key). We also add a pointer to it in `EG(regular_list)` (string key) for caching. When `fbird_close` is called, the resource is removed from the integer list (refcount decremented). If refcount hits 0, it is freed. But the cache entry remains, pointing to freed memory. Next `fbird_pconnect` reuses this dangling pointer.
- **Attempted Fix**:
  1. Introduced `le_link_cache` resource type to manage cache entries.
  2. Added `hash_key` to `fbird_db_link` to allow `fbird_close` to find and remove the cache entry.
  3. Updated `_php_fbird_connect` to use `le_link_cache` and `zend_hash_str_update_ptr`.
  4. Updated `fbird_close` to remove from cache.

## Regression
The attempted fix caused regressions in `tests/fbird_pconnect_001.phpt` and `tests/005.phpt`.
Possible reasons:
- `strdup` vs `estrdup` mismatch or buffer overread (MD5 hash is binary, `strdup` stops at null).
- `zend_list_delete` in cache destructor might be forcing connection closure prematurely.
- `fbird_close` logic might be interfering with persistent connection reuse.

## Next Steps

### 1. Fix Hash Key Storage
- Use `emalloc` + `memcpy` for `hash_key` to handle binary MD5 hashes correctly (16 bytes).
- Do not use `strdup`/`estrdup`.

### 2. Refine Cache Destructor
- `_php_fbird_free_link_cache` should use `GC_DELREF(link_res)` instead of `zend_list_delete`.
- This ensures we decrement the reference count without forcing removal from the main resource list if other references exist.

### 3. Debug `fbird_close`
- Ensure `fbird_close` removes from cache (decrementing refcount) AND removes from main list (decrementing refcount).
- Verify that persistent connections (`le_plink`) are not destroyed if `EG(persistent_list)` still holds a reference.

### 4. Verify with ASan
- Run `./scripts/qa.sh --mode full --fail-fast` to confirm UAF is gone.
- Run `tests/fbird_pconnect_001.phpt` to confirm persistence works.

## Implementation Plan
1. Re-apply `le_link_cache` registration.
2. Implement binary-safe `hash_key` storage in `fbird_db_link`.
3. Implement `_php_fbird_free_link_cache` using `GC_DELREF`.
4. Update `fbird_close` to remove from cache using `hash_key`.
