# Stubs Sync Rule — php-firebird

**Purpose**: Prevent stub drift that breaks PHPStan analysis in `doctrine-firebird-driver`
and other consumers of `satwareag/php-firebird-stubs`.

## Rule: PHP_FE → Stub Mandatory Update

**Whenever `firebird.c` is modified with a new `PHP_FE(fbird_*, ...)` entry:**

1. **Add stub to `stubs/firebird-stubs.php`** — the Packagist-distributed package
2. **Add stub to `phpstan/fbird.stub.php`** — the internal PHPStan stub
3. **Run the sync check** to verify: `bash scripts/check-stubs-sync.sh`

Failure to update stubs causes silent PHPStan failures in downstream consumers.

## Source of Truth

```bash
# Functions registered in the C extension (callable from PHP):
grep "PHP_FE(fbird_" firebird.c | grep -v "ibase_" | sed 's/.*PHP_FE(\(fbird_[^,]*\).*/\1/'

# Functions in stubs:
grep "^function fbird_" stubs/firebird-stubs.php | sed 's/function \(fbird_[^(]*\).*/\1/'
```

These two sets MUST be identical. Run `bash scripts/check-stubs-sync.sh` to verify.

## Stub Addition Checklist

When adding a new `fbird_*` function, the stub entry MUST include:

- [ ] Correct function signature matching `ZEND_BEGIN_ARG_INFO_EX` macro in `firebird.c`
- [ ] All required parameters with correct PHP types
- [ ] Optional parameters with `?type` or default value
- [ ] Return type (use `mixed` when return can be `resource`)
- [ ] PHPDoc `@param` for each parameter with type and description
- [ ] PHPDoc `@return` with type and meaning
- [ ] PHPDoc `@since 7.0.0` (or current version)
- [ ] Entry added to BOTH `stubs/firebird-stubs.php` AND `phpstan/fbird.stub.php`
- [ ] `bash scripts/check-stubs-sync.sh` exits 0

## Type Mapping: C arginfo → PHP Stub

| C arginfo macro | PHP type |
|-----------------|----------|
| `ZEND_ARG_INFO(0, name)` | `mixed $name` |
| `ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)` | `string $name` |
| `ZEND_ARG_TYPE_INFO(0, name, IS_LONG, 0)` | `int $name` |
| `ZEND_ARG_TYPE_INFO(0, name, IS_ARRAY, 0)` | `array $name` |
| `ZEND_ARG_TYPE_INFO(0, name, _IS_BOOL, 0)` | `bool $name` |
| `ZEND_ARG_TYPE_INFO(0, name, IS_ARRAY, 1)` | `?array $name` |
| `ZEND_ARG_OBJ_INFO(0, name, ClassName, 0)` | `ClassName $name` |
| Resource return | `mixed` (PHP has no `resource` type hint) |
| `RETURN_LONG` / `RETVAL_LONG` | `int` |
| `RETURN_BOOL` or `RETURN_TRUE/FALSE` | `bool` |
| `RETURN_STRING` | `string` |
| `RETURN_FALSE` on error | `|false` union |

## Phantom Stubs (Forbidden)

Do NOT add stubs for functions that are NOT registered via `PHP_FE`:

- `fbird_timefmt` — declared in header but intentionally not exported (removed)
- `fbird_batch_get_blob_alignment` — was a stub-only placeholder (removed)

A "phantom stub" misleads doctrine-firebird-driver into thinking a function is callable
at runtime when it is not, hiding catastrophic runtime errors from static analysis.

## Split-Stubs Auto-Sync

`stubs/firebird-stubs.php` is automatically synced to the `satwareAG/php-firebird-stubs`
repository on every push to `satware-main` via `.github/workflows/split-stubs.yml`.

The stubs package is used by `doctrine/orm-firebird-driver` as a Composer dev dependency
for PHPStan analysis. Any missing stubs will surface as PHPStan errors in that project.

## Anti-Patterns

- ❌ Adding PHP_FE in C without updating stubs
- ❌ Adding stubs for unimplemented/unexported functions (phantom stubs)
- ❌ Using ibase_* prefix for new stubs (fbird_* only)
- ❌ Skipping `bash scripts/check-stubs-sync.sh` before commit
