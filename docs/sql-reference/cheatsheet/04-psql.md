# Cheatsheet: PSQL (Procedural SQL)

PSQL features introduced in Firebird 3.0, 4.0, and 5.0.

**Source inventory**: [feature-inventory.md rows #39-#53](../feature-inventory.md#4-psql-procedural-sql)

All PSQL features work via **SQL passthrough** on all three driver layers unless
noted otherwise. The driver executes the DDL/DML that creates and invokes PSQL
objects.

---

## PSQL Packages

- **Introduced**: Firebird 3.0 (CORE-2312)
- **Source**: [packages @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.packages.txt)
- **Metadata introspection**: [#423](https://github.com/satwareAG/php-firebird/issues/423)

**SQL**:
```sql
-- Create a package header (specification)
CREATE PACKAGE pricing AS
BEGIN
    FUNCTION calculate_discount(
        customer_id INT,
        order_total DECFLOAT(16)
    ) RETURNS DECFLOAT(16);

    PROCEDURE apply_discounts;
END;

-- Create the package body (implementation)
CREATE PACKAGE BODY pricing AS
BEGIN
    FUNCTION calculate_discount(
        customer_id INT,
        order_total DECFLOAT(16)
    ) RETURNS DECFLOAT(16)
    AS
    BEGIN
        -- Package-private sub-functions (invisible outside the package)
        RETURN order_total * 0.10;  -- 10% discount for all (simplified)
    END

    PROCEDURE apply_discounts
    AS
    BEGIN
        UPDATE orders
        SET total = total - pricing.calculate_discount(customer_id, total)
        WHERE status = 'pending';
    END
END;
```

**PHP**:
```php
// Create the package (DDL passthrough)
fbird_query($cxn, "CREATE PACKAGE pricing AS BEGIN ... END;");

// Call a package function (DML passthrough)
$res = fbird_query($cxn,
    "SELECT pricing.calculate_discount(?, ?) FROM rdb\$database",
    [$customerId, $orderTotal]
);
$row = fbird_fetch_assoc($res);
echo "Discount: " . $row['DISCOUNT'];

// Call a package procedure
fbird_query($cxn, "EXECUTE PROCEDURE pricing.apply_discounts");
```

**Driver matrix**:

| Layer | Status | Note |
|-------|--------|------|
| `fbird_*` | Y (SQL) | Package DDL and calls work via SQL |
| `Firebird\*` | Y (SQL) | Same. Metadata introspection in [#423](https://github.com/satwareAG/php-firebird/issues/423) |
| `pdo_fbird` | Y (SQL) | Same |

---

## Subfunctions / Subprocedures

- **Introduced**: Firebird 3.0 (CORE-3626 / CORE-1288)
- **Source**: [subroutines @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.subroutines.txt)

**SQL**:
```sql
CREATE PROCEDURE process_order(order_id INT)
AS
    -- Declare a sub-function local to this procedure
    DECLARE FUNCTION format_price(amount DECFLOAT(16)) RETURNS VARCHAR(20)
    AS
    BEGIN
        RETURN LPAD(CAST(amount AS VARCHAR(20)), 20, ' ');
    END
BEGIN
    SELECT format_price(total) FROM orders WHERE id = :order_id INTO :formatted;
    SUSPEND;
END;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## User-Defined PSQL Functions

- **Introduced**: Firebird 3.0 (CORE-2047)
- **Source**: [ddl @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.ddl.txt)

**SQL**:
```sql
-- Non-aggregate scalar function (selectable in queries)
CREATE FUNCTION tax_amount(
    gross DECIMAL(18, 2),
    rate DECIMAL(5, 4) DEFAULT 0.19
) RETURNS DECIMAL(18, 2)
AS
BEGIN
    RETURN gross * rate;
END;

-- Usage:
SELECT gross, tax_amount(gross) AS tax, gross - tax_amount(gross) AS net
FROM invoices;
```

**PHP**:
```php
// Create function
fbird_query($cxn, "CREATE FUNCTION tax_amount(gross DECIMAL(18,2), rate DECIMAL(5,4) DEFAULT 0.19) RETURNS DECIMAL(18,2) AS BEGIN RETURN gross * rate; END");

// Use in queries
$res = fbird_query($cxn, "SELECT tax_amount(?, ?) FROM rdb\$database", [1500.00, 0.19]);
```

**Driver matrix**: Y (SQL) on all three layers.

---

## DDL Triggers

- **Introduced**: Firebird 3.0 (CORE-2310)
- **Source**: [ddl_triggers @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.ddl_triggers.txt)

**SQL**:
```sql
-- Audit all DDL changes
CREATE TRIGGER trg_ddl_audit
ON DDL
AS
BEGIN
    INSERT INTO ddl_log (event, user_name, event_time, sql_text)
    VALUES (
        RDB$GET_CONTEXT('DDL_TRIGGER', 'EVENT_NAME'),
        CURRENT_USER,
        CURRENT_TIMESTAMP,
        RDB$GET_CONTEXT('DDL_TRIGGER', 'SQL_TEXT')
    );
END;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Database Triggers (ON CONNECT / DISCONNECT / TRANSACTION)

- **Introduced**: Firebird 3.0
- **Source**: [db_triggers @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.db_triggers.txt)

**SQL**:
```sql
-- Log connections
CREATE TRIGGER trg_connect ON CONNECT POSITION 0
AS
BEGIN
    INSERT INTO connection_log (user_name, ip_address, connect_time)
    VALUES (
        CURRENT_USER,
        RDB$GET_CONTEXT('SYSTEM', 'CLIENT_ADDRESS'),
        CURRENT_TIMESTAMP
    );
END;

-- FB5: ON DISCONNECT triggers fire even during forced shutdown
CREATE TRIGGER trg_disconnect ON DISCONNECT POSITION 0
AS
BEGIN
    -- Cleanup temp data, log session duration, etc.
END;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## CONTINUE Statement

- **Introduced**: Firebird 3.0 (CORE-1209)
- **Source**: [leave_labels @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.leave_labels)

**SQL**:
```sql
EXECUTE BLOCK AS
    DECLARE i INT = 0;
BEGIN
    WHILE (i < 10) DO
    BEGIN
        i = i + 1;
        IF (MOD(i, 2) = 0) THEN
            CONTINUE;  -- Skip even numbers (like continue in PHP/C)
        INSERT INTO odd_numbers VALUES (:i);
    END
END;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Server-Side Scrollable Cursors

- **Introduced**: Firebird 3.0 (CORE-803)
- **Source**: [scrollable_cursors @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.scrollable_cursors.txt)
- **Network scrollable cursors**: [#426](https://github.com/satwareAG/php-firebird/issues/426)

**SQL** (inside PSQL):
```sql
CREATE PROCEDURE read_bi(id INT) RETURNS (val INT)
AS
    DECLARE c CURSOR FOR (
        SELECT value FROM measurements WHERE sensor_id = :id ORDER BY ts
    );
BEGIN
    OPEN c;
    FETCH LAST FROM c;   -- Jump to end
    CLOSE c;
    SUSPEND;
END;
```

**PHP**:
```php
// Procedural - client-side scrolling is NOT available
// (tracked in #426 for FB5's 6-orientation network cursors)
// Workaround: use PHP-side array buffering
$res = fbird_query($cxn, "SELECT * FROM data ORDER BY id");
$rows = [];
while ($row = fbird_fetch_assoc($res)) {
    $rows[] = $row;
}
// Now you can iterate $rows in any direction
```

**Driver matrix**:

| Layer | Status | Note |
|-------|--------|------|
| `fbird_*` | P | Server-side scrollable cursors work in PSQL. Network bi-directional cursors tracked in [#426](https://github.com/satwareAG/php-firebird/issues/426) |
| `Firebird\*` | P | Same. [#426](https://github.com/satwareAG/php-firebird/issues/426) |
| `pdo_fbird` | P | Same. [#426](https://github.com/satwareAG/php-firebird/issues/426) |

---

## EXECUTE BLOCK

- **Introduced**: Firebird 3.0
- **Source**: [execute_block @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.execute_block)

**SQL**:
```sql
-- Anonymous code block (like a temporary stored procedure)
EXECUTE BLOCK (min_price DECIMAL(10,2) = ?)
RETURNS (product_name VARCHAR(100))
AS
BEGIN
    FOR SELECT name FROM products WHERE price >= :min_price INTO :product_name
    DO
        SUSPEND;
END;
```

**PHP**:
```php
// Procedural - execute block with parameters
$res = fbird_query($cxn, "
    EXECUTE BLOCK (min_price DECIMAL(10,2) = ?)
    RETURNS (product_name VARCHAR(100))
    AS
    BEGIN
        FOR SELECT name FROM products WHERE price >= :min_price INTO :product_name
        DO SUSPEND;
    END
", [50.00]);

while ($row = fbird_fetch_assoc($res)) {
    echo $row['PRODUCT_NAME'] . "\n";
}
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Autonomous Transactions in PSQL

- **Introduced**: Firebird 3.0
- **Source**: [autonomous_transactions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.autonomous_transactions.txt)

**SQL**:
```sql
-- Log errors even when the main transaction rolls back
CREATE PROCEDURE risky_operation AS
BEGIN
    BEGIN
        -- Autonomous block: commits independently
        IN AUTONOMOUS TRANSACTION DO
        BEGIN
            INSERT INTO audit_log (event, ts) VALUES ('started', CURRENT_TIMESTAMP);
        END

        -- Main transaction work
        UPDATE accounts SET balance = balance - 100 WHERE id = 1;
        UPDATE accounts SET balance = balance + 100 WHERE id = 2;
    END
END;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Exception Name/Text in WHEN Handler

- **Introduced**: Firebird 4.0 (CORE-2040 / CORE-1132)
- **Source**: [exception_handling @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.exception_handling)

**SQL**:
```sql
CREATE PROCEDURE safe_divide(a INT, b INT) RETURNS (result DOUBLE PRECISION)
AS
BEGIN
    result = a / b;
    SUSPEND;
WHEN ANY DO
BEGIN
    -- FB4: access exception details
    INSERT INTO error_log (msg, exc_name, exc_msg)
    VALUES ('division failed',
        RDB$GET_CONTEXT('EXCEPTION', 'EXCEPTION_NAME'),
        RDB$GET_CONTEXT('EXCEPTION', 'EXCEPTION_MESSAGE'));
END
END;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Management Statements in PSQL

- **Introduced**: Firebird 4.0 (CORE-5887)
- **Source**: [management_statements_psql @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.management_statements_psql.md)

**SQL**:
```sql
-- Create user, grant roles, etc. from within PSQL
CREATE PROCEDURE provision_tenant(tenant_name VARCHAR(63))
AS
BEGIN
    EXECUTE STATEMENT
        'CREATE USER ' || :tenant_name || ' PASSWORD ''' || :tenant_name || '123''';
    EXECUTE STATEMENT
        'GRANT SELECT ON tenants TO ' || :tenant_name;
END;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Subroutine Access to Outer-Scope Variables (FB5)

- **Introduced**: Firebird 5.0 (#4769)
- **Source**: [subroutines @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.subroutines.txt)

**SQL**:
```sql
CREATE PROCEDURE batch_insert(count INT)
AS
    -- FB5: sub-function can read outer variables
    DECLARE FUNCTION next_id RETURNS INT
    AS
    BEGIN
        -- gen_id_seq is visible from the outer scope
        RETURN NEXT VALUE FOR gen_id_seq;
    END
BEGIN
    WHILE (count > 0) DO
    BEGIN
        INSERT INTO log_table (id, msg) VALUES (next_id(), 'auto');
        count = count - 1;
    END
END;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## EXECUTE STATEMENT Rich Form

- **Introduced**: Firebird 4.0 (CORE-694)
- **Source**: [execute_statement2 @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.execute_statement2)
- **Gap tracked**: [#424](https://github.com/satwareAG/php-firebird/issues/424)

**SQL**:
```sql
-- FB4: cross-database, cross-timezone EXECUTE STATEMENT
EXECUTE BLOCK RETURNS (remote_count INT)
AS
BEGIN
    EXECUTE STATEMENT
        'SELECT COUNT(*) FROM users'
        ON EXTERNAL 'localhost:/data/other_db.fdb'
        AS USER 'admin' PASSWORD 'secret'
        WITH AUTONOMOUS TRANSACTION
    INTO :remote_count;
    SUSPEND;
END;
```

**Driver matrix**:

| Layer | Status | Note |
|-------|--------|------|
| `fbird_*` | Y (SQL) | SQL passthrough. Parameter binding options tracked in [#424](https://github.com/satwareAG/php-firebird/issues/424) |
| `Firebird\*` | Y (SQL) | Same |
| `pdo_fbird` | Y (SQL) | Same |

---

## PSQL CALL Statement

- **Introduced**: Firebird 3.0
- **Source**: [call @ master (trunk)](https://github.com/FirebirdSQL/firebird/raw/master/doc/sql.extensions/README.call.md)

**SQL**:
```sql
-- Call a procedure from within another procedure
CREATE PROCEDURE do_work AS
BEGIN
    CALL sub_task(42, 'parameter');
    CALL another_task;
END;
```

**Driver matrix**: Y (SQL) on all three layers.
