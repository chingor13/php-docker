#ifndef PHP_STACKDRIVER_TRACE_H
#define PHP_STACKDRIVER_TRACE_H 1

#include <sys/time.h>

// Trace functions
PHP_FUNCTION(stackdriver_trace_function);
PHP_FUNCTION(stackdriver_trace_method);
PHP_FUNCTION(stackdriver_trace_list);
PHP_FUNCTION(stackdriver_trace_begin);
PHP_FUNCTION(stackdriver_trace_finish);
PHP_FUNCTION(stackdriver_trace_clear);
PHP_FUNCTION(stackdriver_trace_set_context);
PHP_FUNCTION(stackdriver_trace_context);

void stackdriver_trace_execute_internal(zend_execute_data *execute_data,
                                                      struct _zend_fcall_info *fci, int ret TSRMLS_DC);
void stackdriver_trace_execute_ex (zend_execute_data *execute_data TSRMLS_DC);

// TraceSpan struct
typedef struct stackdriver_trace_span_t {
    zend_string *name;
    uint32_t span_id;
    double start;
    double stop;
    struct stackdriver_trace_span_t *parent;

    // zend_string* => zend_string*
    HashTable *labels;
} stackdriver_trace_span_t;

void stackdriver_trace_init();
void stackdriver_trace_teardown();

#endif /* PHP_STACKDRIVER_TRACE_H */