# Cheatsheet: SQL Statements (DML)

DML statement features introduced in Firebird 3.0, 4.0, and 5.0.

**Source inventory**: [feature-inventory.md rows #12-#28](../feature-inventory.md#2-sql-statements-dml)

---

## OFFSET ... FETCH Clauses

- **Introduced**: Firebird 3.0 (CORE-4526)
- **Source**: [offset_fetch @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.offset_fetch.txt)

**SQL**:
```sql
-- SQL:2008 standard pagination (replaces FIRST/SKIP and ROWS/TO)
SELECT * FROM customers ORDER BY name
OFFSET 20 ROWS FETCH NEXT 10 ROWS ONLY;
```

**PHP**:
```php
// Procedural - parameterized pagination
$page = 3; $size = 10;
$res = fbird_query($cxn,
    "SELECT * FROM customers ORDER BY name OFFSET ? ROWS FETCH NEXT ? ROWS ONLY",
    [($page - 1) * $size, $size]
);
```

**Driver matrix**: Y (SQL) on all three layers.

---

## MERGE Statement

- **Introduced**: Firebird 3.0
- **Source**: [merge @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.merge.txt)

**SQL**:
```sql
MERGE INTO target t
USING SOURCE s ON t.id = s.id
WHEN MATCHED THEN UPDATE SET t.name = s.name
WHEN NOT MATCHED THEN INSERT (id, name) VALUES (s.id, s.name);
```

**Driver matrix**: Y (SQL) on all three layers.

---

## MERGE ... WHEN NOT MATCHED BY SOURCE (FB5)

- **Introduced**: Firebird 5.0 (#6681)
- **Source**: [merge @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.merge.txt)

**SQL**:
```sql
MERGE INTO inventory i
USING (SELECT product_id, qty FROM incoming_shipments) s
ON i.product_id = s.product_id
WHEN MATCHED THEN UPDATE SET i.qty = i.qty + s.qty
WHEN NOT MATCHED BY SOURCE AND i.last_updated < CURRENT_DATE - 30 THEN DELETE;
```

**PHP**:
```php
// Complex merge with source-side filtering
fbird_query($cxn, "
    MERGE INTO inventory i
    USING (SELECT product_id, qty FROM incoming_shipments WHERE received_date = ?) s
    ON i.product_id = s.product_id
    WHEN MATCHED THEN UPDATE SET i.qty = i.qty + s.qty
    WHEN NOT MATCHED BY SOURCE AND i.last_updated < ? THEN DELETE
", [date('Y-m-d'), date('Y-m-d', strtotime('-30 days'))]);
```

**Driver matrix**: Y (SQL) on all three layers.

---

## UPDATE OR INSERT Statement

- **Introduced**: Firebird 3.0
- **Source**: [update_or_insert @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.update_or_insert)

**SQL**:
```sql
UPDATE OR INSERT INTO products (id, name, price)
VALUES (100, 'Widget', 9.99)
MATCHING (id);
```

**Driver matrix**: Y (SQL) on all three layers.

---

## RETURNING Clause (incl. RETURNING * since FB4)

- **Introduced**: RETURNING in FB 3.0; `RETURNING *` in FB 4.0 (CORE-3808)
- **Source**: [returning @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.returning)

**SQL**:
```sql
-- FB3: explicit column list
INSERT INTO orders (customer_id, total) VALUES (42, 150.00)
RETURNING id, created_at;

-- FB4+: return all columns
INSERT INTO orders (customer_id, total) VALUES (42, 150.00)
RETURNING *;
```

**PHP**:
```php
// Procedural - fetch RETURNING values
$res = fbird_query($cxn,
    "INSERT INTO orders (customer_id, total) VALUES (?, ?) RETURNING id, created_at",
    [42, 150.00]
);
$row = fbird_fetch_assoc($res);
echo "New order ID: " . $row['ID'];
echo "Created at: " . $row['CREATED_AT'];
```

**Driver matrix**: Y (SQL) on all three layers.

---

## RETURNING with Multiple Rows (FB5)

- **Introduced**: Firebird 5.0 (#6815)
- **Source**: [returning @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.returning)

**SQL**:
```sql
-- FB5: UPDATE/DELETE RETURNING can produce multiple rows
UPDATE products SET price = price * 1.10
WHERE category = 'electronics'
RETURNING id, name, price;
```

**PHP**:
```php
// Fetch multiple RETURNING rows (FB5 only)
$res = fbird_query($cxn,
    "UPDATE products SET price = price * 1.10 WHERE category = ? RETURNING id, name, price",
    ['electronics']
);
while ($row = fbird_fetch_assoc($res)) {
    printf("Product %d (%s): new price = %.2f\n",
        $row['ID'], $row['NAME'], $row['PRICE']);
}
```

**Driver matrix**: Y (SQL) on all three layers.

---

## SELECT ... WITH LOCK

- **Introduced**: Firebird 3.0
- **Source**: [explicit_locks @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.explicit_locks)

**SQL**:
```sql
-- Pessimistic row locking
SELECT * FROM accounts WHERE id = 42 WITH LOCK;
-- Now safe to UPDATE this row in the same transaction
```

**Driver matrix**: Y (SQL) on all three layers.

---

## SKIP LOCKED Clause (FB5)

- **Introduced**: Firebird 5.0 (#7350)
- **Source**: [skip_locked @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.skip_locked.md)

**SQL**:
```sql
-- Queue-style processing: skip rows locked by other workers
SELECT * FROM job_queue WHERE status = 'pending'
ORDER BY priority DESC
ROWS 10
FOR UPDATE WITH LOCK SKIP LOCKED;

-- Also works with UPDATE and DELETE
UPDATE job_queue SET status = 'processing', worker = ?
WHERE id IN (
    SELECT id FROM job_queue WHERE status = 'pending'
    ROWS 10 FOR UPDATE WITH LOCK SKIP LOCKED
)
SKIP LOCKED;
```

**PHP**:
```php
// Worker queue pattern
$res = fbird_query($cxn, "
    SELECT * FROM job_queue WHERE status = 'pending'
    ORDER BY priority DESC
    ROWS 10
    FOR UPDATE WITH LOCK SKIP LOCKED
");
while ($job = fbird_fetch_assoc($res)) {
    processJob($job);
    fbird_query($cxn,
        "UPDATE job_queue SET status = 'done' WHERE id = ?",
        [$job['ID']]
    );
}
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Batch INSERT/UPDATE API (IBatch)

- **Introduced**: Firebird 4.0 (CORE-5951)
- **Source**: [Using_OO_API @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/Using_OO_API.html)
- **Gap tracked**: [#421](https://github.com/satwareAG/php-firebird/issues/421)

**SQL**:
```sql
-- The batch API is a client-side feature, not SQL syntax.
-- It sends multiple parameter sets in a single network round-trip.
INSERT INTO metrics (host, metric, value, ts) VALUES (?, ?, ?, ?);
```

**PHP**:
```php
// Procedural - native batch API (FB4+)
$stmt = fbird_prepare($cxn,
    "INSERT INTO metrics (host, metric, value, ts) VALUES (?, ?, ?, ?)"
);
$batch = fbird_batch_create($stmt);

// Add multiple parameter sets
foreach ($dataPoints as $dp) {
    fbird_batch_add($batch, [$dp['host'], $dp['metric'], $dp['value'], $dp['ts']]);
}

// Execute all in one network round-trip
$result = fbird_batch_execute($batch);

// OOP
$batch = $conn->prepare($sql)->createBatch();
foreach ($dataPoints as $dp) {
    $batch->add([$dp['host'], $dp['metric'], $dp['value'], $dp['ts']]);
}
$batch->execute();
```

**Driver matrix**:

| Layer | Status | Note |
|-------|--------|------|
| `fbird_*` | Y (native) | `fbird_batch_create/add/execute` |
| `Firebird\*` | Y (native) | `BatchHandle` class |
| `pdo_fbird` | N | Not yet exposed. [#421](https://github.com/satwareAG/php-firebird/issues/421) |

---

## DEFAULT Keyword in DML

- **Introduced**: Firebird 4.0 (CORE-5463)
- **Source**: [ddl @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.ddl.txt)

**SQL**:
```sql
-- Use DEFAULT to insert column's default value explicitly
INSERT INTO users (id, name, role, created_at)
VALUES (1, 'admin', DEFAULT, DEFAULT);

-- Also works in UPDATE
UPDATE users SET password_hash = DEFAULT WHERE id = 1;

-- And in MERGE
MERGE INTO users t USING SOURCE s ON t.id = s.id
WHEN MATCHED THEN UPDATE SET t.login_count = DEFAULT;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## RECREATE / CREATE OR ALTER SEQUENCE

- **Introduced**: Firebird 3.0 (CORE-3018)
- **Source**: [sequence_generators @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.sequence_generators)

**SQL**:
```sql
CREATE OR ALTER SEQUENCE seq_orders RESTART WITH 0;
RECREATE SEQUENCE seq_temp;
ALTER SEQUENCE seq_orders RESTART WITH 1000;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Common Table Expressions (CTEs)

- **Introduced**: Firebird 3.0
- **Source**: [common_table_expressions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.common_table_expressions)

**SQL**:
```sql
-- Recursive CTE: organizational hierarchy
WITH RECURSIVE hierarchy AS (
    SELECT id, parent_id, name, 1 AS depth
    FROM org_chart WHERE parent_id IS NULL
    UNION ALL
    SELECT c.id, c.parent_id, c.name, h.depth + 1
    FROM org_chart c
    JOIN hierarchy h ON c.parent_id = h.id
)
SELECT * FROM hierarchy ORDER BY depth;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Derived Tables

- **Introduced**: Firebird 3.0
- **Source**: [derived_tables @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.derived_tables.txt)

**SQL**:
```sql
SELECT t.dept, t.avg_salary
FROM (
    SELECT dept_id AS dept, AVG(salary) AS avg_salary
    FROM employees
    GROUP BY dept_id
) t
WHERE t.avg_salary > 50000;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Parenthesized Query Expressions (FB5)

- **Introduced**: Firebird 5.0 (#6740)
- **Source**: [sql.extensions @ v5.0.4](https://github.com/FirebirdSQL/firebird/tree/v5.0.4/doc/sql.extensions)

**SQL**:
```sql
-- SQL-standard parenthesized query expressions (FB5)
(SELECT name FROM active_customers)
UNION
(SELECT name FROM archived_customers)
ORDER BY name;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## SQL Standard String Literals (U&'...') (FB5)

- **Introduced**: Firebird 5.0 (#5589)
- **Source**: [alternate_string_quoting @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.alternate_string_quoting.txt)

**SQL**:
```sql
-- Unicode string literal (SQL standard syntax)
SELECT U&'caf\00E9' FROM rdb$database;       -- "cafe"
SELECT U&'sum: ' || U&'\03A3' FROM rdb$database;  -- "sum: Sigma"
```

**Driver matrix**: Y (SQL) on all three layers.

---

## SQL Standard Binary String Literals (FB5)

- **Introduced**: Firebird 5.0 (#5588)
- **Source**: [hex_literals @ v5.0.4](https://github.com/FirebirdSQL/firebird/raw/v5.0.4/doc/sql.extensions/README.hex_literals.txt)

**SQL**:
```sql
-- Full SQL standard binary string literal syntax (FB5)
SELECT X'DEADBEEF' FROM rdb$database;
```

**Driver matrix**: Y (SQL) on all three layers.
