# Cheatsheet: Security & Access Control

Security features introduced in Firebird 3.0, 4.0, and 5.0.

**Source inventory**: [feature-inventory.md rows #54-#65](../feature-inventory.md#5-security--access-control)

---

## SRP (Secure Remote Password) Authentication

- **Introduced**: Firebird 3.0
- **Source**: [README.SecureRemotePassword @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/README.SecureRemotePassword.html)

**SQL**:
```sql
-- Create user with SRP (default in FB3+)
CREATE USER app_user PASSWORD 'secure_password';

-- Legacy auth (for old clients) explicitly:
CREATE USER legacy_user PASSWORD 'pass' USING PLUGIN Legacy_UserManager;
```

**PHP**:
```php
// The driver uses the Firebird client library, which handles SRP automatically.
// No special code needed - just connect normally:
$conn = fbird_connect('localhost:employee', 'app_user', 'secure_password');

// Force wire encryption:
$conn = fbird_connect('localhost/3050:employee', 'app_user', 'pass', [
    'wire_crypt' => true,  // FB3+ default
]);
```

**Driver matrix**: Y (native) on all three layers. Handled by fbclient library.

---

## SQL SECURITY {DEFINER | INVOKER}

- **Introduced**: Firebird 4.0 (CORE-5568)
- **Source**: [sql_security @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.sql_security.txt)
- **Related**: [#423](https://github.com/satwareAG/php-firebird/issues/423)

**SQL**:
```sql
-- DEFINER (default): runs with the privileges of the procedure owner
CREATE PROCEDURE admin_task
SQL SECURITY DEFINER
AS
BEGIN
    -- Even non-admin callers can execute privileged operations
    DELETE FROM audit_log WHERE log_date < CURRENT_DATE - 365;
END;

-- INVOKER: runs with the caller's privileges
CREATE PROCEDURE user_query
SQL SECURITY INVOKER
AS
BEGIN
    -- Fails if caller lacks SELECT on sensitive_table
    SELECT COUNT(*) FROM sensitive_table;
END;
```

**PHP**:
```php
fbird_query($cxn, "
    CREATE PROCEDURE admin_task
    SQL SECURITY DEFINER
    AS
    BEGIN
        DELETE FROM audit_log WHERE log_date < CURRENT_DATE - 365;
    END
");

// Any user can now call it - it runs as the owner
fbird_query($cxn, "EXECUTE PROCEDURE admin_task");
```

**Driver matrix**: Y (SQL) on all three layers. Metadata introspection in [#423](https://github.com/satwareAG/php-firebird/issues/423).

---

## Cumulative Roles

- **Introduced**: Firebird 3.0 (CORE-2884)
- **Source**: [cumulative_roles @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.cumulative_roles.txt)

**SQL**:
```sql
-- Roles can include other roles (role hierarchy)
CREATE ROLE reader;
CREATE ROLE editor;
CREATE ROLE admin;

GRANT reader TO editor;
GRANT editor TO admin;  -- admin now has all reader + editor + admin privileges

-- User gets admin role and inherits all sub-roles
GRANT admin TO user_john;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Grant Role to Another Role

- **Introduced**: Firebird 4.0 (CORE-1815)
- **Source**: [user_management @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.user_management)

**SQL**:
```sql
-- FB4: roles can be granted to other roles (not just to users)
GRANT role_dept_a TO role_manager;  -- role_manager inherits role_dept_a
GRANT SELECT ON TABLE invoices TO role_manager;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Implicitly Active Roles

- **Introduced**: Firebird 4.0 (CORE-751 / CORE-2762)
- **Source**: [set_role @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.set_role)

**SQL**:
```sql
-- FB4: roles are active by default (no need for SET ROLE at connect time)
-- Check which roles are active:
SELECT RDB$ROLE_IN_USE('ROLE_MANAGER') FROM rdb$database;

-- Explicitly switch (still supported):
SET ROLE role_editor;

-- FB4 builtin: check if a role is implicitly active
SELECT RDB$IS_ROLE_IN_USE('reader') AS has_reader FROM rdb$database;
```

**PHP**:
```php
// FB4: all granted roles are active by default
$res = fbird_query($cxn,
    "SELECT RDB\$ROLE_IN_USE(?) FROM rdb\$database",
    ['editor']
);
$row = fbird_fetch_assoc($res);
echo $row['RDB$ROLE_IN_USE'] ? 'has editor' : 'no editor';
```

**Driver matrix**: Y (SQL) on all three layers.

---

## CREATE OR ALTER USER / ALTER USER INACTIVE

- **Introduced**: Firebird 3.0 (CORE-2063 / CORE-2004)
- **Source**: [user_management @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.user_management)

**SQL**:
```sql
CREATE OR ALTER USER app_user PASSWORD 'new_pass' FIRSTNAME 'App' LASTNAME 'User';
ALTER USER temp_user INACTIVE;  -- Disable login without deleting
ALTER USER temp_user ACTIVE;     -- Re-enable
```

**PHP**:
```php
// Procedural - via service manager API
$svc = fbird_service_attach('localhost', 'sysdba', 'masterkey');

// Create or update user
fbird_modify_user($svc, [
    'user_name'    => 'app_user',
    'password'     => 'new_pass',
    'first_name'   => 'App',
    'last_name'    => 'User',
]);

// OOP
$svc = new Firebird\Service('localhost', 'sysdba', 'masterkey');
$svc->modifyUser('app_user', ['password' => 'new_pass']);

// Disable a user (SQL passthrough)
fbird_query($cxn, "ALTER USER temp_user INACTIVE");
```

**Driver matrix**:

| Layer | Status | Note |
|-------|--------|------|
| `fbird_*` | Y (native) | `fbird_add_user`, `fbird_modify_user`, `fbird_delete_user` |
| `Firebird\*` | Y (native) | `Service` class methods |
| `pdo_fbird` | Y (SQL) | SQL passthrough for DDL |

---

## Auto-Auth Mapping

- **Introduced**: Firebird 4.0
- **Source**: [mapping @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.mapping.html)

**SQL**:
```sql
-- Map OS authentication to Firebird users
CREATE MAPPING TRUSTED_AUTH
USING PLUGIN Win_SSPI
FROM ANY USER
TO USER;

-- Map a specific OS group to a Firebird role
CREATE MAPPING ADMIN_GROUP
USING PLUGIN Win_SSPI
FROM GROUP "DOMAIN\Admins"
TO ROLE admin;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## COMMENT ON MAPPING (FB5)

- **Introduced**: Firebird 5.0 (#7046)
- **Source**: [ddl @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.ddl.txt)

**SQL**:
```sql
COMMENT ON MAPPING trusted_auth IS 'Maps Windows SSPI users to Firebird accounts';
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Per-Database Security Database

- **Introduced**: Firebird 3.0 (CORE-3368)
- **Source**: [README.security_database @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.security_database.txt)

**Configuration** (`databases.conf`):
```text
# Use a custom security database for this database
/mydb = /data/mydb.fdb
{
    SecurityDatabase = mydb_security
}
```

**PHP**:
```php
// The security database is configured server-side.
// Connect normally - the server routes auth to the correct security DB:
$conn = fbird_connect('localhost:/data/mydb.fdb', 'user', 'pass');
```

**Driver matrix**: Y (config) on all three layers. Server-side configuration.

---

## DDL Access Rights

- **Introduced**: Firebird 3.0 (CORE-735)
- **Source**: [ddl_access @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.ddl_access.txt)

**SQL**:
```sql
-- Grant DDL privileges (FB3+)
GRANT CREATE TABLE TO role_developer;
GRANT ALTER ANY TABLE TO role_dba;
GRANT DROP ANY PROCEDURE TO role_admin;

-- Check privileges:
SELECT * FROM rdb$user_privileges WHERE rdb$relation_name = 'MY_TABLE';
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Transfer Specific DBA Privileges

- **Introduced**: Firebird 4.0 (CORE-5343)
- **Source**: [user_management @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.user_management)

**SQL**:
```sql
-- FB4: grant specific DBA-like privileges without full DBA
GRANT CREATE USER TO role_user_manager;
GRANT CREATE DATABASE TO role_provisioner;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Grants on MON$ Monitoring Tables

- **Introduced**: Firebird 4.0 (CORE-2557)
- **Source**: [README.monitoring_tables @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/README.monitoring_tables)

**SQL**:
```sql
-- FB4: regular users can be granted SELECT on monitoring tables
GRANT SELECT ON MON$ATTACHMENTS TO role_monitor;
GRANT SELECT ON MON$STATEMENTS TO role_monitor;

-- Application queries its own attachments:
SELECT * FROM MON$ATTACHMENTS WHERE MON$USER = CURRENT_USER;
```

**Driver matrix**: Y (SQL) on all three layers.
