#!/bin/bash
# check_db_integrity.sh

echo "Connecting to Firebird to verify table content..."

# Use ISQL to select the data inserted by the PHP test
# The table is 'test_varchar10' created by tests/007_iso_varchar10.phpt
# Note: The test uses RECREATE TABLE, so if we run this AFTER the test, the table exists.

# ISQL Command
# user: SYSDBA, pass: masterkey
# db: /firebird/data/test.fdb (standard inside docker)

isql -u SYSDBA -p masterkey /firebird/data/test.fdb <<EOF
SET LIST ON;
SELECT ID, V_VARCHAR FROM test_varchar10;
COMMIT;
EXIT;
EOF
