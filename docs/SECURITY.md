# Security Guide - php-firebird Extension

> **⚠️ SQL INJECTION WARNING**: SQL injection is one of the most dangerous security vulnerabilities (OWASP Top 10 - A03:2021 Injection). This guide provides comprehensive protection strategies.

## Table of Contents

1. [Primary Defense: Parameterized Queries](#primary-defense-parameterized-queries)
2. [Secondary Defense: Escaping Functions](#secondary-defense-escaping-functions)
3. [Why Parameterized Queries Are Superior](#why-parameterized-queries-are-superior)
4. [Attack Vectors and Prevention](#attack-vectors-and-prevention)
5. [Safe vs Unsafe Code Examples](#safe-vs-unsafe-code-examples)
6. [Driver Comparison](#driver-comparison)
7. [Best Practices Checklist](#best-practices-checklist)
8. [Reporting Security Issues](#reporting-security-issues)

---

## Primary Defense: Parameterized Queries

**Always use parameterized queries as your primary SQL injection defense.**

Parameterized queries separate SQL code from data, ensuring user input is never interpreted as SQL commands.

### Basic Usage

```php
<?php
// ✅ SAFE: Parameterized query with positional placeholders
$db = fbird_connect('localhost:/path/to/database.fdb', 'SYSDBA', 'masterkey');

$username = $_POST['username'];  // Untrusted user input
$active = 1;

// Prepare the statement with ? placeholders
$stmt = fbird_prepare($db, "SELECT id, email FROM users WHERE username = ? AND active = ?");

// Execute with parameters - input is NEVER parsed as SQL
$result = fbird_execute($stmt, $username, $active);

while ($row = fbird_fetch_assoc($result)) {
    echo "User: {$row['ID']}, Email: {$row['EMAIL']}\n";
}

fbird_free_result($result);
fbird_free_query($stmt);
fbird_close($db);
```

### INSERT with Parameters

```php
<?php
$stmt = fbird_prepare($db, "INSERT INTO users (username, email, created_at) VALUES (?, ?, ?)");

// All user input safely bound as literal values
$result = fbird_execute($stmt, 
    $_POST['username'],
    $_POST['email'],
    date('Y-m-d H:i:s')
);

if ($result) {
    echo "User created successfully";
}
```

### UPDATE with Parameters

```php
<?php
$stmt = fbird_prepare($db, "UPDATE users SET email = ?, updated_at = ? WHERE id = ?");
$result = fbird_execute($stmt, $_POST['email'], date('Y-m-d H:i:s'), (int)$_GET['id']);
```

### DELETE with Parameters

```php
<?php
$stmt = fbird_prepare($db, "DELETE FROM sessions WHERE user_id = ? AND token = ?");
$result = fbird_execute($stmt, $userId, $_COOKIE['session_token']);
```

### IN Clauses (Dynamic Parameter Count)

```php
<?php
$ids = [1, 5, 10, 25];  // From user selection

// Build placeholders dynamically
$placeholders = implode(', ', array_fill(0, count($ids), '?'));
$sql = "SELECT * FROM products WHERE id IN ($placeholders)";

$stmt = fbird_prepare($db, $sql);

// Pass array elements as separate arguments
$result = fbird_execute($stmt, ...$ids);
```

---

## Secondary Defense: Escaping Functions

**Use `fbird_escape_string()` only when parameterized queries cannot be used.**

Common scenarios requiring escaping:
- Dynamic table/column names (cannot be parameterized)
- Building dynamic SQL for stored procedures
- Legacy code migration
- Complex query builders

### Basic Escaping

```php
<?php
$userInput = "O'Brien";
$escaped = fbird_escape_string($userInput);
// Result: "O''Brien" (single quote doubled)

// Safe to use in query (but parameterized queries are still preferred)
$sql = "SELECT * FROM users WHERE lastname = '$escaped'";
```

### What fbird_escape_string() Does

| Input | Output | Notes |
|-------|--------|-------|
| `O'Brien` | `O''Brien` | Single quotes doubled |
| `It's "quoted"` | `It''s "quoted"` | Only single quotes escaped |
| `NULL` | `NULL` | String, not SQL NULL |
| `123` | `123` | Numbers unchanged |
| `\path\to` | `\path\to` | Backslash NOT escaped* |

**\* Important**: Unlike MySQL, Firebird does NOT treat backslash as an escape character. Only single quotes need escaping in Firebird SQL.

### When Escaping Is Necessary

```php
<?php
// Dynamic table name - CANNOT use parameters
$tableName = 'users_' . date('Y');
// Validate against whitelist instead of escaping
$allowedTables = ['users_2024', 'users_2025'];
if (!in_array($tableName, $allowedTables, true)) {
    throw new InvalidArgumentException("Invalid table name");
}
$sql = "SELECT * FROM $tableName WHERE id = ?";
$stmt = fbird_prepare($db, $sql);
$result = fbird_execute($stmt, $userId);

// LIKE patterns with user input
$search = fbird_escape_string($_GET['search']);
$stmt = fbird_prepare($db, "SELECT * FROM products WHERE name LIKE ?");
$result = fbird_execute($stmt, "%{$search}%");
```

---

## Why Parameterized Queries Are Superior

### How Parameterized Queries Work

```
┌─────────────────────────────────────────────────────────────────┐
│                    PARAMETERIZED QUERY FLOW                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  PHP Application                                                │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ $stmt = fbird_prepare($db,                              │   │
│  │     "SELECT * FROM users WHERE username = ?");          │   │
│  │ fbird_execute($stmt, "admin' OR '1'='1");               │   │
│  └─────────────────────────────────────────────────────────┘   │
│                           │                                     │
│                           ▼                                     │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ Network Protocol (Wire Protocol)                        │   │
│  │ ┌─────────────────┐  ┌────────────────────────────────┐ │   │
│  │ │ SQL Structure:  │  │ Parameter Data (Separate):     │ │   │
│  │ │ SELECT * FROM   │  │ Value: "admin' OR '1'='1"      │ │   │
│  │ │ users WHERE     │  │ Type: VARCHAR                  │ │   │
│  │ │ username = ?    │  │ Length: 18                     │ │   │
│  │ └─────────────────┘  └────────────────────────────────┘ │   │
│  └─────────────────────────────────────────────────────────┘   │
│                           │                                     │
│                           ▼                                     │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ Firebird Server                                         │   │
│  │ • SQL parsed and compiled FIRST (no user data)          │   │
│  │ • Parameter bound as LITERAL STRING (never parsed)      │   │
│  │ • Searches for user literally named: admin' OR '1'='1   │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  Result: No user found (correct, safe behavior)                 │
└─────────────────────────────────────────────────────────────────┘
```

### How String Concatenation Fails

```
┌─────────────────────────────────────────────────────────────────┐
│                STRING CONCATENATION (UNSAFE)                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  PHP Application                                                │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ $user = "admin' OR '1'='1";                              │   │
│  │ $sql = "SELECT * FROM users WHERE username = '$user'";  │   │
│  │ fbird_query($db, $sql);                                 │   │
│  └─────────────────────────────────────────────────────────┘   │
│                           │                                     │
│                           ▼                                     │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ Network: Single SQL String                              │   │
│  │ "SELECT * FROM users WHERE username = 'admin' OR '1'='1'"│   │
│  └─────────────────────────────────────────────────────────┘   │
│                           │                                     │
│                           ▼                                     │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ Firebird Server                                         │   │
│  │ • Parses entire string as SQL                           │   │
│  │ • Interprets: username = 'admin' OR '1'='1'             │   │
│  │ • Condition '1'='1' is ALWAYS TRUE                      │   │
│  │ • Returns ALL users!                                    │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  Result: FULL DATABASE DUMP - Security Breach!                  │
└─────────────────────────────────────────────────────────────────┘
```

### Key Differences

| Aspect | Parameterized Queries | String Escaping |
|--------|----------------------|-----------------|
| **Security** | Structural separation | Pattern-based filtering |
| **Reliability** | 100% injection-proof | Depends on implementation |
| **Performance** | Query plan cached | Recompiled each time |
| **Maintenance** | Clear intent | Escaping often forgotten |
| **Edge Cases** | Handled by protocol | May miss edge cases |

---

## Attack Vectors and Prevention

### Common SQL Injection Patterns

| Attack Type | Example Payload | Prevention |
|-------------|-----------------|------------|
| **Classic Injection** | `' OR '1'='1` | Parameterized queries |
| **Comment Injection** | `admin'--` | Parameterized queries |
| **Union-based** | `' UNION SELECT password FROM users--` | Parameterized queries |
| **Stacked Queries** | `'; DROP TABLE users;--` | Firebird blocks this* |
| **Second-order** | Stored value exploited later | Parameterize ALL queries |
| **Blind Boolean** | `' AND 1=1--` vs `' AND 1=2--` | Parameterized queries |
| **Blind Time-based** | `' AND (SELECT COUNT(*) ...)` | Parameterized queries |

**\* Firebird Protection**: Firebird does NOT support stacked queries (multiple statements separated by `;`). This provides inherent protection against DROP TABLE attacks. **However, do NOT rely on this as a security measure.**

### Firebird-Specific Considerations

```php
<?php
// Firebird uses double single-quotes for escaping, NOT backslash
$safe = fbird_escape_string("O'Reilly");  // Returns: O''Reilly

// Firebird identifiers use double quotes (case-sensitive)
$sql = 'SELECT * FROM "CaseSensitiveTable" WHERE id = ?';

// Firebird EXECUTE BLOCK (stored procedure syntax)
// Parameters work normally:
$stmt = fbird_prepare($db, "
    EXECUTE BLOCK (p_id INTEGER = ?)
    RETURNS (name VARCHAR(100))
    AS
    BEGIN
        FOR SELECT name FROM users WHERE id = :p_id INTO :name DO
            SUSPEND;
    END
");
```

---

## Safe vs Unsafe Code Examples

### Authentication

```php
<?php
// ❌ UNSAFE - SQL Injection vulnerable
function unsafeLogin($db, $username, $password) {
    $sql = "SELECT * FROM users WHERE username = '$username' AND password = '$password'";
    $result = fbird_query($db, $sql);
    return fbird_fetch_assoc($result);
}
// Attack: username = "admin'--" bypasses password check!

// ✅ SAFE - Parameterized query
function safeLogin($db, $username, $password) {
    $stmt = fbird_prepare($db, 
        "SELECT id, username, password_hash FROM users WHERE username = ?");
    $result = fbird_execute($stmt, $username);
    $user = fbird_fetch_assoc($result);
    
    if ($user && password_verify($password, $user['PASSWORD_HASH'])) {
        return $user;
    }
    return false;
}
```

### Search Functionality

```php
<?php
// ❌ UNSAFE - Injection via search term
function unsafeSearch($db, $term) {
    $sql = "SELECT * FROM products WHERE name LIKE '%$term%'";
    return fbird_query($db, $sql);
}
// Attack: term = "%' UNION SELECT username, password, 1 FROM users--"

// ✅ SAFE - Parameterized LIKE
function safeSearch($db, $term) {
    $stmt = fbird_prepare($db, "SELECT * FROM products WHERE name LIKE ?");
    return fbird_execute($stmt, "%{$term}%");
}

// ✅ SAFER - With escaping for LIKE special characters
function saferSearch($db, $term) {
    // Escape SQL wildcards if needed
    $term = str_replace(['%', '_'], ['\%', '\_'], $term);
    $stmt = fbird_prepare($db, "SELECT * FROM products WHERE name LIKE ? ESCAPE '\\'");
    return fbird_execute($stmt, "%{$term}%");
}
```

### Dynamic Queries

```php
<?php
// ❌ UNSAFE - Dynamic column name
function unsafeSort($db, $column, $direction) {
    $sql = "SELECT * FROM products ORDER BY $column $direction";
    return fbird_query($db, $sql);
}
// Attack: column = "price; DROP TABLE products--"

// ✅ SAFE - Whitelist validation
function safeSort($db, $column, $direction) {
    $allowedColumns = ['name', 'price', 'created_at'];
    $allowedDirections = ['ASC', 'DESC'];
    
    if (!in_array($column, $allowedColumns, true)) {
        $column = 'name';  // Default
    }
    if (!in_array(strtoupper($direction), $allowedDirections, true)) {
        $direction = 'ASC';
    }
    
    $sql = "SELECT * FROM products ORDER BY $column $direction";
    return fbird_query($db, $sql);
}
```

### Bulk Operations

```php
<?php
// ❌ UNSAFE - Building IN clause unsafely
function unsafeDeleteMultiple($db, $ids) {
    $idList = implode(',', $ids);  // User might inject: "1,2); DROP TABLE users;--"
    $sql = "DELETE FROM temp_items WHERE id IN ($idList)";
    return fbird_query($db, $sql);
}

// ✅ SAFE - Parameterized IN clause
function safeDeleteMultiple($db, array $ids) {
    // Validate all IDs are integers
    $ids = array_filter($ids, 'is_numeric');
    $ids = array_map('intval', $ids);
    
    if (empty($ids)) {
        return false;
    }
    
    $placeholders = implode(',', array_fill(0, count($ids), '?'));
    $sql = "DELETE FROM temp_items WHERE id IN ($placeholders)";
    $stmt = fbird_prepare($db, $sql);
    return fbird_execute($stmt, ...$ids);
}
```

---

## Driver Comparison

How other Firebird drivers handle SQL injection prevention:

| Driver | Language | Parameterized Queries | Escape Function | Notes |
|--------|----------|----------------------|-----------------|-------|
| **Jaybird** | Java | ✅ PreparedStatement | ❌ Not exposed | Forces parameterized queries |
| **firebird-driver** | Python | ✅ DB API 2.0 | ❌ Not needed | Standard Python patterns |
| **node-firebird** | JavaScript | ✅ `?` placeholders | ✅ `escape()` | Both mechanisms |
| **.NET Provider** | C# | ✅ FbParameter | ❌ Not exposed | ADO.NET standard |
| **Go firebirdsql** | Go | ✅ database/sql | ❌ Not needed | Standard Go patterns |
| **php-firebird** | PHP | ✅ fbird_execute() | ✅ fbird_escape_string() | This extension |

### Jaybird (Java) - Reference Implementation

Jaybird takes the strictest approach by not exposing any escape functions:

```java
// Jaybird BLOCKS direct SQL execution on PreparedStatement
PreparedStatement stmt = conn.prepareStatement("SELECT * FROM users WHERE id = ?");
stmt.setInt(1, userId);

// This throws SQLNonTransientException:
// stmt.executeQuery("SELECT * FROM users WHERE id = " + userId);
// Error: "This method is only supported on Statement"
```

### node-firebird (JavaScript)

```javascript
// Parameterized (preferred)
db.query('SELECT * FROM users WHERE id = ?', [userId], callback);

// Escape function (when needed)
const safe = Firebird.escape("O'Brien");  // Returns: "'O''Brien'"
```

---

## Best Practices Checklist

### Development Phase

- [ ] **Always use parameterized queries** for user input
- [ ] **Validate input types** before using in queries
- [ ] **Use whitelists** for dynamic table/column names
- [ ] **Escape only when parameters impossible** (dynamic identifiers)
- [ ] **Apply principle of least privilege** - use read-only DB users where possible

### Code Review

- [ ] No string concatenation with user input in SQL
- [ ] All `fbird_query()` calls use only hardcoded SQL
- [ ] `fbird_prepare()` + `fbird_execute()` for dynamic values
- [ ] Input validation before database operations
- [ ] Error messages don't expose SQL or database structure

### Testing

- [ ] Test with common injection payloads:
  - `' OR '1'='1`
  - `'; DROP TABLE users;--`
  - `admin'--`
  - `' UNION SELECT null,null,null--`
- [ ] Test with special characters in legitimate input:
  - Names with apostrophes: `O'Brien`, `McDonald's`
  - International characters: `François`, `北京`
- [ ] Fuzz testing with random input

### Production

- [ ] Database user has minimal required permissions
- [ ] Query logging enabled for security audits
- [ ] Error messages sanitized (no SQL in output)
- [ ] WAF/IDS rules for SQL injection patterns

---

## Reporting Security Issues

If you discover a security vulnerability in php-firebird:

1. **Do NOT** open a public GitHub issue
2. **Email** security concerns to the maintainers privately
3. **Include**:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact assessment
   - Suggested fix (if available)

We follow responsible disclosure practices and will:
- Acknowledge receipt within 48 hours
- Provide an initial assessment within 7 days
- Work with you on a coordinated disclosure timeline

---

## References

- [OWASP SQL Injection Prevention Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/SQL_Injection_Prevention_Cheat_Sheet.html)
- [OWASP Top 10 - A03:2021 Injection](https://owasp.org/Top10/A03_2021-Injection/)
- [Firebird SQL Language Reference](https://firebirdsql.org/file/documentation/html/en/refdocs/fblangref50/firebird-50-language-reference.html)
- [php-firebird Extension Documentation](../README.md)

---

*Last Updated: December 2025*  
*php-firebird Security Guide v1.0*