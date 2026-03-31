/*
 * pdo_fbird — error handling and SQLSTATE mapping
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_PDO_FBIRD

#include "php.h"
#include "ext/pdo/php_pdo_driver.h"
#include "php_pdo_fbird_int.h"
#include <ibase.h>
#include "../firebird_utils.h"

/* Map common Firebird GDS error codes to SQLSTATE strings */
static const struct {
	long gds_code;
	const char *sqlstate;
} pdo_fbird_sqlstate_map[] = {
	{ 335544321L, "08001" }, /* connect failed */
	{ 335544323L, "42000" }, /* bad SQL */
	{ 335544324L, "42S02" }, /* table unknown */
	{ 335544325L, "42S22" }, /* column unknown */
	{ 335544326L, "23000" }, /* integrity constraint */
	{ 335544327L, "23000" }, /* validation error */
	{ 335544336L, "42000" }, /* expression error */
	{ 335544338L, "40001" }, /* deadlock */
	{ 335544345L, "23000" }, /* unique key violation */
	{ 335544347L, "22012" }, /* division by zero */
	{ 335544349L, "42000" }, /* invalid token */
	{ 335544364L, "08003" }, /* connection lost */
	{ 335544375L, "40001" }, /* lock conflict */
	{ 335544381L, "22001" }, /* string truncation */
	{ 335544382L, "22003" }, /* numeric overflow */
	{ 335544569L, "23000" }, /* foreign key violation */
	{ 0,          NULL    }
};

static const char *pdo_fbird_gds_to_sqlstate(long gds_code)
{
	for (int i = 0; pdo_fbird_sqlstate_map[i].sqlstate; i++) {
		if (pdo_fbird_sqlstate_map[i].gds_code == gds_code) {
			return pdo_fbird_sqlstate_map[i].sqlstate;
		}
	}
	return "HY000";
}

static void _pdo_fbird_set_error_code(pdo_error_type *pdo_err, ISC_STATUS *status)
{
	long gds_code = 0;
	if (status && status[0] == 1 && status[1] > 0) {
		gds_code = status[1];
	}
	const char *sqlstate = pdo_fbird_gds_to_sqlstate(gds_code);
	strncpy(*pdo_err, sqlstate, sizeof(pdo_error_type) - 1);
	(*pdo_err)[sizeof(pdo_error_type) - 1] = '\0';
}

int pdo_fbird_error(pdo_dbh_t *dbh)
{
	pdo_fbird_db_handle *H = (pdo_fbird_db_handle *)dbh->driver_data;
	_pdo_fbird_set_error_code(&dbh->error_code, H->status);
	return 0;
}

int pdo_fbird_stmt_error(pdo_stmt_t *stmt)
{
	pdo_fbird_stmt *S = (pdo_fbird_stmt *)stmt->driver_data;
	_pdo_fbird_set_error_code(&stmt->error_code, S->status);
	return 0;
}

#endif /* HAVE_PDO_FBIRD */
