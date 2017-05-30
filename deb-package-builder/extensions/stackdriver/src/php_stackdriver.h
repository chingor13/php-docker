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
PHP_RSHUTDOWN_FUNCTION(stackdriver);

ZEND_BEGIN_MODULE_GLOBALS(stackdriver)
    /* map of functions we're tracing to callbacks */
    HashTable *traced_functions;

    // Trace context
    stackdriver_trace_span_t *current_span;
    stackdriver_trace_span_t **spans;

    int span_count;
    zend_string *trace_id;
    long trace_parent_span_id;
ZEND_END_MODULE_GLOBALS(stackdriver)

extern ZEND_DECLARE_MODULE_GLOBALS(stackdriver)

#ifdef ZTS
#define        STACKDRIVER_G(v)        TSRMG(stackdriver_globals_id, zend_stackdriver_globals *, v)
#else
#define        STACKDRIVER_G(v)        (stackdriver_globals.v)
#endif

#endif /* PHP_STACKDRIVER_H */
