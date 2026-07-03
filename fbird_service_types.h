/* fbird_service_types.h - Unified service struct for procedural + OOP paths.
 * Included by php_fbird_includes.h (procedural) and fbird_class_internal.h (OOP).
 * Eliminates the dual-struct cast pattern (OC-11). */
#ifndef FBIRD_SERVICE_TYPES_H
#define FBIRD_SERVICE_TYPES_H

#include "php.h"
#include "zend.h"

typedef struct fbird_service_s {
	char          *hostname;
	char          *username;
	zend_resource *res;       /* resource handle (procedural path) or reference (OOP wrap) */
	void          *fbsvc;     /* OO API ServiceWrapper* (fbsvc_attach) */
	zend_object    std;       /* must be last for zend_object_alloc (OOP path only) */
} fbird_service;

static inline fbird_service *fbird_service_from_obj(zend_object *obj) {
	return (fbird_service *)((char *)obj - XtOffsetOf(fbird_service, std));
}
#define Z_FBIRD_SERVICE_P(zv) fbird_service_from_obj(Z_OBJ_P(zv))

#endif /* FBIRD_SERVICE_TYPES_H */
