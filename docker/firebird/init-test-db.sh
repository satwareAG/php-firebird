#!/bin/bash
# docker/firebird/init-test-db.sh
# Initialization script for Firebird 5.x container
# Creates test.fdb database if it doesn't exist
#
# This script is mounted to /docker-entrypoint-initdb.d/ and executed
# automatically on container first start by the firebirdsql/firebird:5 image.
#
# Issue: https://github.com/satwareAG/php-firebird/issues/44

set -e

DB_PATH="/firebird/data/test.fdb"
DB_USER="${FIREBIRD_USER:-SYSDBA}"
DB_PASSWORD="${ISC_PASSWORD:-masterkey}"

echo "=== Firebird 5.x Database Initialization ==="
echo "Database path: $DB_PATH"

# Check if database already exists
if [ -f "$DB_PATH" ]; then
    echo "Database $DB_PATH already exists, skipping creation."
    exit 0
fi

echo "Creating test database: $DB_PATH"

# Create database using isql
# jane: image tag drift - firebirdsql/firebird:5 renamed isql-fb to isql
# (current image: /opt/firebird/bin/isql). Feature-detect, don't hardcode.
ISQL_BIN="$(command -v isql || command -v isql-fb || true)"
if [ -z "$ISQL_BIN" ]; then
    echo "✗ No isql binary found (tried: isql, isql-fb)"
    exit 1
fi
"$ISQL_BIN" -user "$DB_USER" -password "$DB_PASSWORD" <<EOF
CREATE DATABASE '$DB_PATH'
    USER '$DB_USER' PASSWORD '$DB_PASSWORD'
    PAGE_SIZE 16384
    DEFAULT CHARACTER SET UTF8;
COMMIT;
EOF

# Verify creation
if [ -f "$DB_PATH" ]; then
    echo "✓ Database created successfully: $DB_PATH"
    ls -la "$DB_PATH"
else
    echo "✗ Database creation failed!"
    exit 1
fi

echo "=== Database initialization complete ==="
