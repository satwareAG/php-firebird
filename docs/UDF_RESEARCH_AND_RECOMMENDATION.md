# Firebird UDF Research and Recommendation

## Date: 2025-12-21

## Executive Summary

**Recommendation: REMOVE `fbird_udf.c` from the main extension.**

The `fbird_udf.c` file is a **separate Firebird UDF library** (not part of the PHP extension) with significant security implications, legacy architecture, and no current usage within the extension.

---

## What is `fbird_udf.c`?

### Purpose
`fbird_udf.c` implements a User Defined Function (UDF) library designed to be compiled as a **separate shared library** (`php_fbird_udf.so`) that loads **inside the Firebird database server** process.

### What it provides
1. **`exec_php()`** - Execute PHP code stored in BLOB fields directly from SQL
2. **`udf_call_php1()` through `udf_call_php8()`** - Call PHP functions with 1-8 arguments from SQL

### Example SQL usage (if the UDF were deployed)
```sql
-- Call PHP's ucwords() function from SQL
UPDATE employees SET name = CALL_PHP1('ucwords', name);

-- Execute PHP code from a BLOB field
SELECT EXEC_PHP(php_code_blob, result, 0) FROM scripts;
```

---

## Why This is NOT Part of the PHP Extension

| Aspect | PHP Extension (firebird.c) | UDF Library (fbird_udf.c) |
|--------|---------------------------|---------------------------|
| **Runs in** | PHP process | Firebird server process |
| **Loaded by** | PHP interpreter | Firebird server |
| **Purpose** | Connect PHP apps to Firebird | Call PHP FROM Firebird |
| **Build** | phpize/configure/make | Separate gcc compilation |
| **Output** | firebird.so (PHP module) | php_fbird_udf.so (Firebird UDF) |

The file is currently in the repository but:
- ✗ Not included in `config.m4` (not built with the extension)
- ✗ Not linked into the extension
- ✗ cppcheck reports all functions as "unused" (correct - they aren't linked)

---

## Security Analysis

### Critical Security Risks

| Risk | Severity | Description |
|------|----------|-------------|
| **Arbitrary Code Execution** | CRITICAL | PHP code runs with Firebird server privileges |
| **Remote Exploitation Surface** | HIGH | Any SQL client can trigger code execution |
| **Server Stability** | HIGH | PHP crashes can crash the database server |
| **Resource Exhaustion** | MEDIUM | Long-running PHP can DOS the database |
| **Privilege Escalation** | CRITICAL | PHP has access to filesystem, network, etc. |

### Why PHP-inside-DB is Particularly Dangerous

1. **Full PHP runtime** - Not a sandboxed subset, but the entire PHP interpreter
2. **PHP extensions loaded** - Filesystem, network, sockets all available
3. **Server process context** - Runs as the Firebird service user
4. **No input sanitization** - SQL parameters passed directly to PHP
5. **Audit bypass** - Code execution not visible in DB logs/triggers

### OWASP Alignment

This architecture violates several OWASP principles:
- **A03:2021 Injection** - SQL-to-PHP code injection surface
- **A04:2021 Insecure Design** - Embedding interpreters in DB servers
- **A05:2021 Security Misconfiguration** - Overly privileged DB components

---

## Technical Issues

### Legacy API Usage
The UDF code uses deprecated Firebird API:
```c
/* Legacy API - Comments in code acknowledge this */
isc_decode_sql_date((ISC_DATE*)argv[i]->dsc_address, &t);
isc_decode_sql_time((ISC_TIME*)argv[i]->dsc_address, &t);
isc_decode_timestamp((ISC_TIMESTAMP*)argv[i]->dsc_address, &t);
```

The php-firebird extension itself uses the modern Firebird 3.0+ OO API.

### Build Complexity
Requires separate compilation with PHP embedded library:
```bash
# Manual build required (not part of extension build)
gcc -shared `php-config --includes` `php-config --ldflags` \
    `php-config --libs` -o php_fbird_udf.so fbird_udf.c \
    /usr/lib/libphp8.a  # Requires static PHP library!
```

### Thread Safety Concerns
Uses pthread mutexes for ZTS (Zend Thread Safety), but:
- Complex locking around Zend operations
- Potential for deadlocks
- Not extensively tested with modern PHP

---

## Modern Alternatives

### Recommended Architecture: Application-Side Logic

```
┌─────────────────────────────────────────────────────────────┐
│                     Modern Pattern                          │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────┐    ┌─────────────┐    ┌───────────────────┐   │
│  │ Firebird │◄──│ PHP App     │◄──│ Business Logic    │   │
│  │ Database │   │ (Extension) │   │ (PHP functions)   │   │
│  └─────────┘    └─────────────┘    └───────────────────┘   │
│       ▲                ▲                    ▲               │
│       │                │                    │               │
│    Data only      Queries &           All transforms       │
│                   Results             happen here           │
└─────────────────────────────────────────────────────────────┘
```

### Instead of UDF SQL:
```sql
-- DON'T: Execute PHP inside database
UPDATE employees SET name = CALL_PHP1('ucwords', name);
```

### Do this in PHP:
```php
// DO: Execute PHP in application layer
$stmt = $conn->prepare('SELECT id, name FROM employees');
$stmt->execute();
while ($row = $stmt->fetch()) {
    $update = $conn->prepare('UPDATE employees SET name = ? WHERE id = ?');
    $update->execute([ucwords($row['name']), $row['id']]);
}
```

### For complex transformations, use stored procedures:
```sql
-- Firebird stored procedure for deterministic logic
CREATE PROCEDURE uppercase_name (name VARCHAR(100))
RETURNS (result VARCHAR(100))
AS
BEGIN
    result = UPPER(name);
    SUSPEND;
END
```

---

## Recommendation

### Primary Recommendation: Remove from Repository

**Action:** Delete `fbird_udf.c` from the repository.

**Rationale:**
1. ✗ Not part of the PHP extension build
2. ✗ Critical security vulnerabilities
3. ✗ Uses deprecated Firebird API
4. ✗ Modern best practice is application-side logic
5. ✗ Requires separate, complex build process
6. ✗ Creates false impression it's part of the extension

### Alternative: Move to Archive

If historical preservation is desired:

1. Move to `contrib/legacy/fbird_udf.c`
2. Add `SECURITY_WARNING.md` in same directory
3. Add prominent warnings in file header
4. Document that it's unsupported and dangerous

### Migration Path for Legacy Users

If any users depend on this functionality:

1. **Identify usage** - Survey if anyone uses php_fbird_udf.so
2. **Provide migration guide** - Document application-side alternatives
3. **Deprecate with timeline** - Mark as deprecated in one release, remove in next
4. **Recommend alternatives**:
   - Move logic to PHP application layer
   - Use Firebird stored procedures for data-only transforms
   - Implement job queues for async PHP processing triggered by DB

---

## Implementation Plan

### If Removing (Recommended)

```bash
git rm fbird_udf.c
```

Update CHANGELOG.md:
```markdown
### Removed
- Removed `fbird_udf.c` UDF library (server-side PHP execution) due to security
  concerns and modern best practices. Use application-side PHP logic instead.
```

### If Archiving

```bash
mkdir -p contrib/legacy
git mv fbird_udf.c contrib/legacy/
# Create warning documentation
```

---

## References

1. Firebird UDF Documentation: https://firebirdsql.org/manual/udf.html
2. OWASP Injection Prevention: https://owasp.org/Top10/A03_2021-Injection/
3. PHP Embedded SAPI: https://www.php.net/manual/en/internals2.sapis.php
4. Firebird External Functions: https://firebirdsql.org/file/documentation/html/en/refdocs/fblangref40/firebird-40-language-reference.html#fblangref40-ddl-extfunc

---

## Appendix: cppcheck Output

```
fbird_udf.c:128:0: style: The function 'exec_php' is never used. [unusedFunction]
fbird_udf.c:350:0: style: The function 'udf_call_php1' is never used. [unusedFunction]
fbird_udf.c:356:0: style: The function 'udf_call_php2' is never used. [unusedFunction]
fbird_udf.c:362:0: style: The function 'udf_call_php3' is never used. [unusedFunction]
fbird_udf.c:368:0: style: The function 'udf_call_php4' is never used. [unusedFunction]
fbird_udf.c:375:0: style: The function 'udf_call_php5' is never used. [unusedFunction]
fbird_udf.c:382:0: style: The function 'udf_call_php6' is never used. [unusedFunction]
fbird_udf.c:389:0: style: The function 'udf_call_php7' is never used. [unusedFunction]
fbird_udf.c:396:0: style: The function 'udf_call_php8' is never used. [unusedFunction]
```

These warnings are **correct** - the functions are never linked into the extension.
