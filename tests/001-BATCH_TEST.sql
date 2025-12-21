-- Test table for IBatch API comprehensive type testing
-- Covers all SQL data types supported by Firebird 4.0+ IBatch operations
-- Used by: fbird_batch_multitype_001.phpt

RECREATE TABLE BATCH_TEST (
    -- Primary key - INTEGER (most common for batch inserts)
    ID INTEGER NOT NULL PRIMARY KEY,

    -- Integer types
    INT_COL INTEGER,
    BIGINT_COL BIGINT,
    SMALLINT_COL SMALLINT,

    -- Floating point types
    FLOAT_COL FLOAT,
    DOUBLE_COL DOUBLE PRECISION,

    -- Fixed-point numeric types
    NUMERIC_COL NUMERIC(18, 4),
    DECIMAL_COL DECIMAL(10, 2),

    -- String types
    CHAR_COL CHAR(20),
    VARCHAR_COL VARCHAR(100),

    -- Date/Time types
    DATE_COL DATE,
    TIME_COL TIME,
    TIMESTAMP_COL TIMESTAMP,

    -- BLOB types
    BLOB_TEXT_COL BLOB SUB_TYPE TEXT,
    BLOB_BIN_COL BLOB SUB_TYPE BINARY,

    -- Boolean type (Firebird 3.0+)
    BOOLEAN_COL BOOLEAN
);
