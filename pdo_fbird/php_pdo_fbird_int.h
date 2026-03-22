/*
 * pdo_fbird — internal structs and declarations
 */

#ifndef PHP_PDO_FBIRD_INT_H
#define PHP_PDO_FBIRD_INT_H

#include "php.h"
#include "ext/pdo/php_pdo_driver.h"
#include <ibase.h>
#include "../firebird_utils.h"

/* Per-connection driver data stored in pdo_dbh_t->driver_data */
typedef struct {
	void        *fbc_conn;      /* fb::Connection* via fbc_connect() */
	void        *fbt_trans;     /* fb::Transaction* via fbt_start()  */
	int          dialect;       /* SQL dialect (1 or 3)              */
	char        *charset;       /* connection charset                */
	char        *role;          /* SQL role                          */
	int          autocommit;    /* 1 = autocommit mode               */
	ISC_STATUS_ARRAY status;    /* status vector for error reporting */
} pdo_fbird_db_handle;

/* Per-statement driver data stored in pdo_stmt_t->driver_data */
typedef struct {
	pdo_fbird_db_handle *H;         /* back-pointer to connection    */
	void                *fbs_stmt;  /* fb::Statement* via fbs_*()    */
	void                *out_meta;  /* IMessageMetadata* for output  */
	void                *in_meta;   /* IMessageMetadata* for input   */
	unsigned char       *out_buf;   /* output message buffer         */
	unsigned char       *in_buf;    /* input message buffer          */
	unsigned int         out_count; /* number of output columns      */
	unsigned int         in_count;  /* number of input parameters    */
	int                  has_rows;  /* cursor open and has data      */
	ISC_STATUS_ARRAY     status;    /* status vector                 */
} pdo_fbird_stmt;

/* Forward declarations */
extern const pdo_driver_t pdo_fbird_driver;
extern const struct pdo_dbh_methods pdo_fbird_dbh_methods;
extern const struct pdo_stmt_methods pdo_fbird_stmt_methods;

int pdo_fbird_error(pdo_dbh_t *dbh);
int pdo_fbird_stmt_error(pdo_stmt_t *stmt);

#endif /* PHP_PDO_FBIRD_INT_H */
