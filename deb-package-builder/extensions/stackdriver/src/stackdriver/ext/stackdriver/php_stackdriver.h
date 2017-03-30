#ifndef PHP_STACKDRIVER_H
#define PHP_STACKDRIVER_H 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "stackdriver_trace.h"
#include "stackdriver_debugger.h"


#define PHP_STACKDRIVER_VERSION "0.1"
#define PHP_STACKDRIVER_EXTNAME "stackdriver"

PHP_FUNCTION(stackdriver_version);

extern zend_module_entry stackdriver_module_entry;
#define phpext_stackdriver_ptr &stackdriver_module_entry

PHP_MINIT_FUNCTION(stackdriver);
PHP_MSHUTDOWN_FUNCTION(stackdriver);
PHP_RINIT_FUNCTION(stackdriver);

ZEND_BEGIN_MODULE_GLOBALS(stackdriver)
    /* map of functions we're tracing to callbacks */
    HashTable *traced_functions;

    /* old zend execute functions */
    void (*_zend_execute_ex) (zend_execute_data *execute_data TSRMLS_DC);
    void (*_zend_execute_internal) (zend_execute_data *data, struct _zend_fcall_info *fci, int ret TSRMLS_DC);

    // Trace context
    stackdriver_trace_span_t *current_span;
    stackdriver_trace_span_t **spans;
    int span_count;
ZEND_END_MODULE_GLOBALS(stackdriver)

extern ZEND_DECLARE_MODULE_GLOBALS(stackdriver)

#ifdef ZTS
#define        STACKDRIVER_G(v)        TSRMG(stackdriver_globals_id, zend_stackdriver_globals *, v)
#else
#define        STACKDRIVER_G(v)        (stackdriver_globals.v)
#endif


#endif /* PHP_STACKDRIVER_H */
