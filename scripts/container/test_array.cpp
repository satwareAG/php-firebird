#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ibase.h>

#ifndef SQLDA_CURRENT_VERSION
#define SQLDA_CURRENT_VERSION SQLDA_VERSION1
#endif

#ifndef XSQLDA_LENGTH
#define XSQLDA_LENGTH(n) (sizeof(XSQLDA) + (n-1)*sizeof(XSQLVAR))
#endif

void print_hex(const char* label, void* buf, int len) {
    unsigned char* c = (unsigned char*)buf;
    printf("%s: ", label);
    for (int i = 0; i < len; i++) {
        printf("%02x ", c[i]);
    }
    printf("\n");
}

int main() {
    ISC_STATUS status[20];
    isc_db_handle db = 0;
    isc_tr_handle trans = 0;

    // Connect
    char dpb[] = {isc_dpb_version1, isc_dpb_user_name, 6, 'S','Y','S','D','B','A',
                  isc_dpb_password, 9, 'm','a','s','t','e','r','k','e','y',
                  isc_dpb_lc_ctype, 4, 'N','O','N','E'};

    if (isc_attach_database(status, 0, "firebird40:/firebird/repro_cpp.fdb", &db, sizeof(dpb), dpb)) {
        if (status[1] == 335544344) { // DB not found (approx check) or io_error
             if (isc_create_database(status, 0, "firebird40:/firebird/repro_cpp.fdb", &db, sizeof(dpb), dpb, 0)) {
                 isc_print_status(status);
                 return 1;
             }
        } else {
             isc_print_status(status);
             return 1;
        }
    }

    // Recreate table
    if (isc_start_transaction(status, &trans, 1, &db, 0, NULL)) {
        isc_print_status(status); return 1;
    }

    isc_dsql_execute_immediate(status, &db, &trans, 0, "RECREATE TABLE TEST_ARR (ID INT, ARR VARCHAR(10)[2])", 3, NULL);
    isc_commit_transaction(status, &trans);
    trans = 0;

    // Insert
    if (isc_start_transaction(status, &trans, 1, &db, 0, NULL)) {
        isc_print_status(status); return 1;
    }

    ISC_QUAD array_id = {0, 0};
    ISC_ARRAY_DESC desc;

    if (isc_array_lookup_bounds(status, &db, &trans, "TEST_ARR", "ARR", &desc)) {
        isc_print_status(status); return 1;
    }

    printf("Desc Length: %d, DataType: %d\n", desc.array_desc_length, desc.array_desc_dtype);

    // Prepare buffer - PADDED FORMAT
    // E1: [len:2] [pad:2] [data:10] = 14 bytes?
    // Let's try stride 14.

    int stride = desc.array_desc_length + 4; // 10+4=14
    printf("Stride: %d\n", stride);

    char fixed_buf[28]; // 14 * 2
    memset(fixed_buf, 0, 28);

    // E1
    *(short*)fixed_buf = 5;
    // Data at offset 4?
    memcpy(fixed_buf + 4, "test1", 5);

    // E2
    *(short*)(fixed_buf + stride) = 5;
    memcpy(fixed_buf + stride + 4, "test2", 5);

    print_hex("Padded Buffer", fixed_buf, 28);

    ISC_LONG len = 28;
    if (isc_array_put_slice(status, &db, &trans, &array_id, &desc, fixed_buf, &len)) {
        isc_print_status(status); return 1;
    }

    // Insert row
    isc_stmt_handle stmt = 0;
    isc_dsql_allocate_statement(status, &db, &stmt);
    isc_dsql_prepare(status, &trans, &stmt, 0, "INSERT INTO TEST_ARR (ID, ARR) VALUES (1, ?)", 1, NULL);

    XSQLDA *sqlda = (XSQLDA*)malloc(XSQLDA_LENGTH(1));
    sqlda->version = SQLDA_CURRENT_VERSION;
    sqlda->sqln = 1;
    sqlda->sqld = 1;
    sqlda->sqlvar[0].sqldata = (char*)&array_id;
    sqlda->sqlvar[0].sqltype = SQL_ARRAY;
    sqlda->sqlvar[0].sqllen = sizeof(ISC_QUAD);

    isc_dsql_execute(status, &trans, &stmt, 1, sqlda);
    isc_commit_transaction(status, &trans);

    printf("Insert committed. Reading back...\n");
    trans = 0;
    isc_start_transaction(status, &trans, 1, &db, 0, NULL);

    // Select
    isc_dsql_prepare(status, &trans, &stmt, 0, "SELECT ARR FROM TEST_ARR WHERE ID=1", 1, NULL);
    sqlda->sqlvar[0].sqldata = (char*)&array_id;
    isc_dsql_execute(status, &trans, &stmt, 1, NULL); // No input params

    if (isc_dsql_fetch(status, &stmt, 1, sqlda) == 0) {
        // Get slice
        char read_buf[100];
        memset(read_buf, 0, 100);
        ISC_LONG read_len = 100;

        // Re-lookup desc to be sure
        isc_array_lookup_bounds(status, &db, &trans, "TEST_ARR", "ARR", &desc);

        if (isc_array_get_slice(status, &db, &trans, &array_id, &desc, read_buf, &read_len)) {
            isc_print_status(status);
        } else {
            printf("Read Len: %ld\n", read_len);
            print_hex("Read Buffer", read_buf, read_len);

            // Check content
            short l1 = *(short*)read_buf;
            printf("E1 Len: %d\n", l1);
            if (l1 == 5 && strncmp(read_buf+2, "test1", 5) == 0) printf("E1 Content OK\n");
            else printf("E1 Content FAIL\n");

            // Where is E2?
            // If packed, E2 is at 2+5 = 7.
            // If stride 12, E2 is at 12.

            // Heuristic check
            short l2_packed = *(short*)(read_buf + 7);
            short l2_stride = *(short*)(read_buf + 12);

            printf("Packed Offset (7) Len: %d\n", l2_packed);
            printf("Stride Offset (12) Len: %d\n", l2_stride);
        }
    }

    isc_commit_transaction(status, &trans);
    isc_detach_database(status, &db);
    return 0;
}
