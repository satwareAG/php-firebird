#include <stdio.h>
#include <ibase.h>

int main() {
    printf("isc_db_handle: %zu\n", sizeof(isc_db_handle));
    printf("isc_tr_handle: %zu\n", sizeof(isc_tr_handle));
    printf("isc_stmt_handle: %zu\n", sizeof(isc_stmt_handle));
    printf("isc_blob_handle: %zu\n", sizeof(isc_blob_handle));
    printf("void*: %zu\n", sizeof(void*));
    return 0;
}
