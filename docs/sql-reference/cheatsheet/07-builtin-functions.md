# Cheatsheet: Built-in Functions

Built-in SQL functions introduced in Firebird 3.0, 4.0, and 5.0.

**Source inventory**: [feature-inventory.md rows #81-#94](../feature-inventory.md#7-built-in-functions)

All built-in functions work via **SQL passthrough** on all three driver layers.

---

## Statistical Aggregates

- **Introduced**: Firebird 3.0 (CORE-4714)
- **Source**: [statistical_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.statistical_functions.txt)

**SQL**:
```sql
SELECT
    department,
    COUNT(*)                     AS headcount,
    AVG(salary)                  AS mean_salary,
    STDDEV_POP(salary)           AS pop_stddev,
    STDDEV_SAMP(salary)          AS sample_stddev,
    VAR_POP(salary)              AS population_variance,
    VAR_SAMP(salary)             AS sample_variance
FROM employees
GROUP BY department;
```

**PHP**:
```php
$res = fbird_query($cxn, "
    SELECT department,
           AVG(salary) AS mean,
           STDDEV_SAMP(salary) AS stddev
    FROM employees
    GROUP BY department
    ORDER BY mean DESC
");
while ($row = fbird_fetch_assoc($res)) {
    printf("%-20s mean=%.2f stddev=%.2f\n",
        $row['DEPARTMENT'], $row['MEAN'], $row['STDDEV']);
}
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Covariance / Correlation

- **Introduced**: Firebird 3.0 (CORE-4717)
- **Source**: [statistical_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.statistical_functions.txt)

**SQL**:
```sql
-- Correlation between experience and salary
SELECT
    CORR(years_experience, salary)      AS correlation,
    COVAR_SAMP(years_experience, salary) AS sample_covariance,
    COVAR_POP(years_experience, salary)  AS population_covariance
FROM employees;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Linear Regression Functions

- **Introduced**: Firebird 3.0 (CORE-4722)
- **Source**: [regr_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.regr_functions.txt)

**SQL**:
```sql
-- Linear regression: predict y from x
SELECT
    REGR_SLOPE(salary, years_experience)     AS slope,
    REGR_INTERCEPT(salary, years_experience)  AS intercept,
    REGR_R2(salary, years_experience)         AS r_squared
FROM employees;

-- Full set: REGR_SLOPE, REGR_INTERCEPT, REGR_R2, REGR_AVGX, REGR_AVGY,
-- REGR_SXX, REGR_SYY, REGR_SXY, REGR_COUNT
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Inverse Hyperbolic Trigonometric Functions

- **Introduced**: Firebird 3.0 (CORE-2744)
- **Source**: [builtin_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.builtin_functions.txt)

**SQL**:
```sql
SELECT
    ASINH(1.0) AS inverse_hyperbolic_sine,
    ACOSH(2.0) AS inverse_hyperbolic_cosine,
    ATANH(0.5) AS inverse_hyperbolic_tangent
FROM rdb$database;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Cryptographic Functions

- **Introduced**: Firebird 4.0 (CORE-5970)
- **Source**: [builtin_functions @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.builtin_functions.txt)

**SQL**:
```sql
-- Symmetric encryption/decryption
SELECT ENCRYPT('secret data' USING AES MODE CBC KEY 'mykey1234567890ab' IV '0123456789012345');
SELECT DECRYPT(encrypted_col USING AES MODE CBC KEY 'mykey1234567890ab' IV '0123456789012345');

-- RSA signing/verification
SELECT RSA_SIGN(data_col PRIVATE KEY PEM '-----BEGIN RSA PRIVATE KEY-----...');
SELECT RSA_VERIFY(data_col, signature_col PUBLIC KEY PEM '-----BEGIN PUBLIC KEY-----...');

-- Hashing (FB4+ supports multiple algorithms)
SELECT HASH(password, SHA256);
SELECT HASH(password, SHA512);

-- Base64 / Hex encoding
SELECT BASE64_ENCODE(blob_col);
SELECT HEX_ENCODE(binary_col);
```

**PHP**:
```php
// Encrypt sensitive data at insert time
fbird_query($cxn, "
    INSERT INTO secrets (id, data)
    VALUES (?, ENCRYPT(? USING AES MODE CBC KEY ? IV ?))
", [
    $id,
    $plaintext,
    $key,  // exactly 16/24/32 bytes for AES-128/192/256
    $iv,   // exactly 16 bytes
]);

// Verify a signature
$res = fbird_query($cxn, "
    SELECT RSA_VERIFY(?, ? PUBLIC KEY PEM ?) AS verified
    FROM rdb\$database
", [$data, $signature, $publicKeyPem]);
```

**Driver matrix**: Y (SQL) on all three layers.

---

## FIRST_DAY() / LAST_DAY()

- **Introduced**: Firebird 4.0 (CORE-5620)
- **Source**: [builtin_functions @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.builtin_functions.txt)

**SQL**:
```sql
-- First/last day of the month/quarter/year/week
SELECT
    FIRST_DAY(CURRENT_DATE, MONTH)   AS first_of_month,
    LAST_DAY(CURRENT_DATE, MONTH)    AS last_of_month,
    FIRST_DAY(CURRENT_DATE, QUARTER) AS first_of_quarter,  -- FB5
    LAST_DAY(CURRENT_DATE, YEAR)     AS last_of_year;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## QUARTER Support (FB5)

- **Introduced**: Firebird 5.0 (#5959)
- **Source**: [builtin_functions @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.builtin_functions.txt)

**SQL**:
```sql
-- FB5: QUARTER in EXTRACT, FIRST_DAY, LAST_DAY
SELECT
    EXTRACT(QUARTER FROM order_date)  AS q,
    FIRST_DAY(order_date, QUARTER)    AS q_start,
    LAST_DAY(order_date, QUARTER)     AS q_end
FROM orders
WHERE order_date >= '2026-01-01';
```

**Driver matrix**: Y (SQL) on all three layers.

---

## BLOB_APPEND()

- **Introduced**: Firebird 5.0 (#7216)
- **Source**: [blob_append @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.blob_append.md)

**SQL**:
```sql
-- Efficiently concatenate blobs and strings without round-trips
SELECT BLOB_APPEND(blob_col, ' -- appended text -- ', other_blob_col)
FROM documents WHERE id = 42;

-- Build a log blob:
SELECT BLOB_APPEND(
    BLOB_APPEND(
        BLOB_APPEND(logo, 'Header text'),
        body
    ),
    'Footer text'
) AS full_document
FROM templates WHERE name = 'invoice';
```

**PHP**:
```php
// Build blob content server-side (more efficient than PHP-side concatenation)
$res = fbird_query($cxn, "
    SELECT BLOB_APPEND(header_blob, body_blob, footer_blob) AS full_doc
    FROM document_parts WHERE doc_id = ?
", [$docId]);
$row = fbird_fetch_assoc($res);

// Read the combined blob
$blob = fbird_blob_open($cxn, $row['FULL_DOC']);
$content = '';
while ($chunk = fbird_blob_get($blob, 4096)) {
    $content .= $chunk;
}
fbird_blob_close($blob);
```

**Driver matrix**: Y (SQL) on all three layers.

---

## UNICODE_CHAR() / UNICODE_VAL()

- **Introduced**: Firebird 5.0 (#6798)
- **Source**: [builtin_functions @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.builtin_functions.txt)

**SQL**:
```sql
-- Convert between Unicode code points and characters
SELECT UNICODE_CHAR(8364) FROM rdb$database;  -- Euro sign: EUR
SELECT UNICODE_VAL('A') FROM rdb$database;     -- 65

-- Generate characters from code points
SELECT UNICODE_CHAR(0x1F600) FROM rdb$database;  -- Emoji: grinning face
```

**Driver matrix**: Y (SQL) on all three layers.

---

## HASH() with Multiple Algorithms

- **Introduced**: Firebird 4.0 (CORE-4436)
- **Source**: [builtin_functions @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.builtin_functions.txt)

**SQL**:
```sql
-- FB3: only CRC32 (legacy) or no algorithm specified
SELECT HASH('test');           -- CRC32 (legacy default)

-- FB4+: explicit algorithm
SELECT HASH('test', SHA256);   -- SHA-256
SELECT HASH('test', SHA512);   -- SHA-512
SELECT HASH('test', MD5);      -- MD5 (not recommended for security)
```

**PHP**:
```php
// Store password hashes using SHA-256 (or better)
fbird_query($cxn, "
    UPDATE users
    SET password_hash = HASH(?, SHA256)
    WHERE id = ?
", [$newPassword, $userId]);

// Verify:
$res = fbird_query($cxn, "
    SELECT CASE WHEN password_hash = HASH(?, SHA256) THEN 1 ELSE 0 END AS match
    FROM users WHERE username = ?
", [$inputPassword, $username]);
```

**Driver matrix**: Y (SQL) on all three layers.

---

## RDB$GET_CONTEXT / RDB$SET_CONTEXT

- **Introduced**: Context variables in FB 2.0+; SESSION_TIMEZONE context in FB 4.0
- **Source**: [context_variables2 @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.context_variables2)

**SQL**:
```sql
-- User-defined context (persists for session)
SELECT RDB$SET_CONTEXT('USER_SESSION', 'tenant_id', 42) FROM rdb$database;
SELECT RDB$GET_CONTEXT('USER_SESSION', 'tenant_id') FROM rdb$database;  -- 42

-- System context (FB4+ includes SESSION_TIMEZONE)
SELECT RDB$GET_CONTEXT('SYSTEM', 'SESSION_TIMEZONE') FROM rdb$database;
SELECT RDB$GET_CONTEXT('SYSTEM', 'CLIENT_ADDRESS') FROM rdb$database;
SELECT RDB$GET_CONTEXT('SYSTEM', 'CURRENT_ROLE') FROM rdb$database;
```

**PHP**:
```php
// Set tenant context at login
fbird_query($cxn, "
    SELECT RDB\$SET_CONTEXT('USER_SESSION', 'tenant_id', ?) FROM rdb\$database
", [$tenantId]);

// All subsequent queries can use the context (e.g., in views/triggers):
$res = fbird_query($cxn, "
    SELECT * FROM tenant_data
    WHERE tenant_id = RDB\$GET_CONTEXT('USER_SESSION', 'tenant_id')
");
```

**Driver matrix**: Y (SQL) on all three layers.

---

## RDB$BLOB_UTIL System Package (FB5)

- **Introduced**: Firebird 5.0 (#281)
- **Source**: [blob_util @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.blob_util.md)

**SQL**:
```sql
-- FB5: server-side blob manipulation functions
SELECT RDB$BLOB_UTIL.GET_SIZE(blob_col) FROM documents WHERE id = 1;
SELECT RDB$BLOB_UTIL.SUB_BLOB(blob_col, 0, 1024) FROM documents;  -- First 1KB
SELECT RDB$BLOB_UTIL.CAT(blob1, blob2, blob3);  -- Concatenate blobs
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Time Zone Functions

- **Introduced**: Firebird 4.0 (CORE-694)
- **Source**: [time_zone @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.time_zone.md)
- **Type binding**: [#419](https://github.com/satwareAG/php-firebird/issues/419)

**SQL**:
```sql
-- Convert between timezones
SELECT CURRENT_TIMESTAMP AT TIME ZONE 'America/New_York';
SELECT TIMESTAMP '2026-07-14 12:00:00' AT TIME ZONE 'UTC' AT TIME ZONE 'Europe/Berlin';

-- Extract timezone info
SELECT EXTRACT(TIMEZONE_HOUR FROM CURRENT_TIMESTAMP) AS tz_hour;
SELECT EXTRACT(TIMEZONE_MINUTE FROM CURRENT_TIMESTAMP) AS tz_min;

-- RDB$TIME_ZONE_UTIL package (FB4+):
SELECT * FROM RDB$TIME_ZONE_UTIL.OFFSET('Europe/Berlin', TIMESTAMP '2026-07-14 12:00:00');
```

**Driver matrix**: Y (SQL) on all three layers. Native TZ type binding in [#419](https://github.com/satwareAG/php-firebird/issues/419).

---

## DECFLOAT ROUND / TRAPS Settings (FB5)

- **Introduced**: Firebird 5.0 (#7642)
- **Source**: [builtin_functions @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.builtin_functions.txt)

**SQL**:
```sql
-- FB5: control DECFLOAT rounding mode and exception traps
SET DECFLOAT ROUND TO HALF_UP;
SET DECFLOAT TRAPS TO OVERFLOW, DIVIDE_BY_ZERO;

-- Query current settings:
SELECT RDB$GET_CONTEXT('SYSTEM', 'DECFLOAT_ROUND') FROM rdb$database;
SELECT RDB$GET_CONTEXT('SYSTEM', 'DECFLOAT_TRAPS') FROM rdb$database;
```

**Driver matrix**: Y (SQL) on all three layers.
