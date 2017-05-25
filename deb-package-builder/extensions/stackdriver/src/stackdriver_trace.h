/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
