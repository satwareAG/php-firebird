#include <stdio.h>
#include <ibase.h>

int main() {
    printf("sizeof(isc_db_handle) = %zu\n", sizeof(isc_db_handle));
    printf("sizeof(void*) = %zu\n", sizeof(void*));
    return 0;
}
