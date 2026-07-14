# Cheatsheet: Window Functions

Analytic/window function features introduced in Firebird 3.0 and 4.0.

**Source inventory**: [feature-inventory.md rows #29-#38](../feature-inventory.md#3-window-functions)

All window functions work via **SQL passthrough** on all three driver layers.
The driver sends the SQL unchanged; no native binding is needed.

---

## ROW_NUMBER(), RANK(), DENSE_RANK()

- **Introduced**: Firebird 3.0 (CORE-2830)
- **Source**: [window_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.window_functions.txt)

**SQL**:
```sql
-- Assign sequential ranks to rows within partitions
SELECT
    employee_id,
    department,
    salary,
    ROW_NUMBER() OVER (PARTITION BY department ORDER BY salary DESC) AS rn,
    RANK()       OVER (PARTITION BY department ORDER BY salary DESC) AS rnk,
    DENSE_RANK() OVER (PARTITION BY department ORDER BY salary DESC) AS dense_rnk
FROM employees;
```

**PHP**:
```php
$res = fbird_query($cxn, "
    SELECT employee_id, department, salary,
           ROW_NUMBER() OVER (PARTITION BY department ORDER BY salary DESC) AS rn
    FROM employees
");
while ($row = fbird_fetch_assoc($res)) {
    if ($row['RN'] <= 3) {  // Top 3 per department
        printf("#%d %s: %s earns %.2f\n",
            $row['RN'], $row['DEPARTMENT'], $row['EMPLOYEE_ID'], $row['SALARY']);
    }
}
```

**Driver matrix**: Y (SQL) on all three layers.

---

## FIRST_VALUE(), LAST_VALUE(), NTH_VALUE()

- **Introduced**: Firebird 3.0 (CORE-3619/3620/3621)
- **Source**: [window_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.window_functions.txt)

**SQL**:
```sql
SELECT
    day,
    revenue,
    FIRST_VALUE(revenue) OVER (ORDER BY day) AS first_day_rev,
    LAST_VALUE(revenue)  OVER (ORDER BY day ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING) AS last_day_rev,
    NTH_VALUE(revenue, 3) OVER (ORDER BY day ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING) AS third_day_rev
FROM daily_sales;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## LAG(), LEAD()

- **Introduced**: Firebird 3.0 (CORE-2869)
- **Source**: [window_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.window_functions.txt)

**SQL**:
```sql
-- Day-over-day comparison
SELECT
    day,
    revenue,
    revenue - LAG(revenue, 1) OVER (ORDER BY day) AS daily_change,
    revenue - LAG(revenue, 7) OVER (ORDER BY day) AS week_over_week
FROM daily_sales;
```

**PHP**:
```php
$res = fbird_query($cxn, "
    SELECT day, revenue,
           revenue - LAG(revenue, 1) OVER (ORDER BY day) AS daily_change
    FROM daily_sales
    ORDER BY day
");
while ($row = fbird_fetch_assoc($res)) {
    $change = $row['DAILY_CHANGE'];
    $arrow = ($change !== null && $change > 0) ? '+' : '';
    printf("%s: %.2f (%s%.2f)\n",
        $row['DAY'], $row['REVENUE'], $arrow, $change ?? 0);
}
```

**Driver matrix**: Y (SQL) on all three layers.

---

## OVER () with Existing Aggregates

- **Introduced**: Firebird 3.0 (CORE-2090)
- **Source**: [window_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.window_functions.txt)

**SQL**:
```sql
-- Grand total alongside each row (no GROUP BY collapse)
SELECT
    product,
    sales,
    SUM(sales) OVER () AS total_sales,
    ROUND(sales * 100.0 / SUM(sales) OVER (), 2) AS pct_of_total
FROM monthly_sales
WHERE month = '2026-07';
```

**Driver matrix**: Y (SQL) on all three layers.

---

## PARTITION BY Clause

- **Introduced**: Firebird 3.0 (CORE-2133)
- **Source**: [window_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.window_functions.txt)

**SQL**:
```sql
-- Running total per customer
SELECT
    customer_id,
    order_date,
    amount,
    SUM(amount) OVER (PARTITION BY customer_id ORDER BY order_date) AS running_total
FROM orders;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## ORDER BY Inside OVER ()

- **Introduced**: Firebird 3.0 (CORE-2823)
- **Source**: [window_functions @ R3_0_7](https://github.com/FirebirdSQL/firebird/raw/R3_0_7/doc/sql.extensions/README.window_functions.txt)

**SQL**:
```sql
-- Moving average with ORDER BY in OVER
SELECT
    date,
    price,
    AVG(price) OVER (ORDER BY date ROWS BETWEEN 2 PRECEDING AND CURRENT ROW) AS ma_3day
FROM stock_prices
WHERE symbol = 'AAPL';
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Window Frames (ROWS / RANGE BETWEEN)

- **Introduced**: Firebird 4.0 (CORE-3647)
- **Source**: [window_functions @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.window_functions.md)

**SQL**:
```sql
-- Explicit frame specification
SELECT
    date,
    sales,
    -- Running sum from partition start to current row
    SUM(sales) OVER (ORDER BY date ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS cumulative,
    -- Sum of current row + 2 preceding + 2 following
    SUM(sales) OVER (ORDER BY date ROWS BETWEEN 2 PRECEDING AND 2 FOLLOWING) AS ma_5,
    -- Average within a range of +/- 100 in sales value
    AVG(sales) OVER (ORDER BY sales RANGE BETWEEN 100 PRECEDING AND 100 FOLLOWING) AS range_avg
FROM daily_sales;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## Named Windows (WINDOW Clause)

- **Introduced**: Firebird 4.0 (CORE-5346)
- **Source**: [window_functions @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.window_functions.md)

**SQL**:
```sql
-- Define named windows to avoid repeating the same OVER clause
SELECT
    dept,
    employee,
    salary,
    RANK()       OVER w AS dept_rank,
    DENSE_RANK() OVER w AS dept_dense_rank,
    ROW_NUMBER() OVER w AS dept_row_num
FROM employees
WINDOW w AS (PARTITION BY dept ORDER BY salary DESC);
```

**Driver matrix**: Y (SQL) on all three layers.

---

## PERCENT_RANK(), CUME_DIST(), NTILE()

- **Introduced**: Firebird 4.0 (CORE-1688)
- **Source**: [window_functions @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.window_functions.md)

**SQL**:
```sql
-- Statistical ranking functions
SELECT
    student,
    score,
    PERCENT_RANK() OVER (ORDER BY score) AS pct_rank,
    CUME_DIST()    OVER (ORDER BY score) AS cume_dist,
    NTILE(4)       OVER (ORDER BY score DESC) AS quartile   -- 4 equal groups
FROM exam_results;
```

**Driver matrix**: Y (SQL) on all three layers.

---

## FILTER Clause for Aggregates

- **Introduced**: Firebird 4.0 (CORE-5768)
- **Source**: [aggregate_filter @ v4.0.7](https://github.com/FirebirdSQL/firebird/raw/v4.0.7/doc/sql.extensions/README.aggregate_filter.md)

**SQL**:
```sql
-- SQL-standard FILTER clause (cleaner than CASE WHEN)
SELECT
    COUNT(*) AS total,
    COUNT(*) FILTER (WHERE status = 'active')    AS active_count,
    COUNT(*) FILTER (WHERE status = 'inactive')  AS inactive_count,
    SUM(amount) FILTER (WHERE status = 'active') AS active_revenue
FROM orders
WHERE order_date >= '2026-01-01';

-- Also works with window functions:
SELECT
    day,
    region,
    COUNT(*) OVER w                                    AS total_orders,
    COUNT(*) FILTER (WHERE status = 'shipped') OVER w  AS shipped_orders
FROM orders
WINDOW w AS (PARTITION BY region ORDER BY day);
```

**PHP**:
```php
$res = fbird_query($cxn, "
    SELECT
        COUNT(*) FILTER (WHERE status = 'active') AS active_count,
        COUNT(*) FILTER (WHERE status = 'inactive') AS inactive_count
    FROM users
");
$stats = fbird_fetch_assoc($res);
printf("Active: %d, Inactive: %d\n", $stats['ACTIVE_COUNT'], $stats['INACTIVE_COUNT']);
```

**Driver matrix**: Y (SQL) on all three layers.
