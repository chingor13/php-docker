#ifndef PHP_STACKDRIVER_TRACE_H
#define PHP_STACKDRIVER_TRACE_H 1

#include <sys/time.h>

// Trace functions
PHP_FUNCTION(stackdriver_trace_function);
PHP_FUNCTION(stackdriver_trace_method);
PHP_FUNCTION(stackdriver_trace_list);
PHP_FUNCTION(stackdriver_trace_begin);
PHP_FUNCTION(stackdriver_trace_finish);

void stackdriver_trace_execute_internal(zend_execute_data *execute_data,
                                                      struct _zend_fcall_info *fci, int ret TSRMLS_DC);
void stackdriver_trace_execute_ex (zend_execute_data *execute_data TSRMLS_DC);

// TraceSpan struct
typedef struct stackdriver_trace_span_t {
    char *name;
    struct timeval start;
    struct timeval stop;
    struct stackdriver_trace_span_t *parent;
} stackdriver_trace_span_t;

void stackdriver_trace_setup_automatic_tracing();

#endif /* PHP_STACKDRIVER_TRACE_H */
