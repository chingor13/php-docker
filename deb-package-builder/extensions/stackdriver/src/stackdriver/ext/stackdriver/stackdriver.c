#include "php.h"
#include "php_stackdriver.h"
#include "stackdriver_trace.h"
#include "stackdriver_debugger.h"
#include "zend_extensions.h"

void (*_zend_execute_ex) (zend_execute_data *execute_data TSRMLS_DC);
void (*_zend_execute_internal) (zend_execute_data *data,
                      struct _zend_fcall_info *fci, int ret TSRMLS_DC);

ZEND_DECLARE_MODULE_GLOBALS(stackdriver)

// list of custom PHP functions provided by this extension
// set {NULL, NULL, NULL} as the last record to mark the end of list
static zend_function_entry stackdriver_functions[] = {
    PHP_FE(stackdriver_version, NULL)
    PHP_FE(stackdriver_trace_function, NULL)
    PHP_FE(stackdriver_trace_method, NULL)
    PHP_FE(stackdriver_trace_list, NULL)
    PHP_FE(stackdriver_trace_begin, NULL)
    PHP_FE(stackdriver_trace_finish, NULL)
    PHP_FE(stackdriver_debugger, NULL)
    {NULL, NULL, NULL}
};

// the following code creates an entry for the module and registers it with Zend.
zend_module_entry stackdriver_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_STACKDRIVER_EXTNAME,
    stackdriver_functions,
    PHP_MINIT(stackdriver),
    PHP_MSHUTDOWN(stackdriver),
    PHP_RINIT(stackdriver),
    NULL, // name of the RSHUTDOWN function or NULL if not applicable
    NULL, // name of the MINFO function or NULL if not applicable
#if ZEND_MODULE_API_NO >= 20010901
    PHP_STACKDRIVER_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

ZEND_GET_MODULE(stackdriver)

// returns the version of the stackdriver extension
PHP_FUNCTION(stackdriver_version)
{
    RETURN_STRING(PHP_STACKDRIVER_VERSION);
}

static void php_stackdriver_globals_ctor(void *pDest TSRMLS_DC)
{
    zend_stackdriver_globals *stackdriver_global = (zend_stackdriver_globals *) pDest;
}

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(stackdriver)
{
    // php_printf("stackdriver MINIT...\n");

#ifdef ZTS
    ts_allocate_id(&stackdriver_globals_id, sizeof(zend_stackdriver_globals), php_stackdriver_globals_ctor, NULL);
#else
    php_stackdriver_globals_ctor(&php_stackdriver_globals_ctor);
#endif

    // Save original zend execute functions and use our own to instrument function calls
    STACKDRIVER_G(_zend_execute_ex) = zend_execute_ex;
    zend_execute_ex = stackdriver_trace_execute_ex;

    // STACKDRIVER_G(_zend_execute_internal) = zend_execute_internal;
    // zend_execute_internal = stackdriver_trace_execute_internal;

    // php_printf("... done MINIT\n");
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(stackdriver)
{
    return SUCCESS;
}
/* }}} */

PHP_RINIT_FUNCTION(stackdriver)
{
    stackdriver_trace_init();
    return SUCCESS;
}

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(stackdriver)
{
    stackdriver_trace_teardown();
    return SUCCESS;
}
/* }}} */