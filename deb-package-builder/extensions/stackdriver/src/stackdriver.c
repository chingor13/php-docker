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

#include "php.h"
#include "php_stackdriver.h"
#include "stackdriver_trace.h"
#include "stackdriver_debugger.h"
#include "zend_extensions.h"

ZEND_DECLARE_MODULE_GLOBALS(stackdriver)

// List of functions provided by this extension
static zend_function_entry stackdriver_functions[] = {
    PHP_FE(stackdriver_version, NULL)
    PHP_FE(stackdriver_trace_function, NULL)
    PHP_FE(stackdriver_trace_method, NULL)
    PHP_FE(stackdriver_trace_list, NULL)
    PHP_FE(stackdriver_trace_begin, NULL)
    PHP_FE(stackdriver_trace_finish, NULL)
    PHP_FE(stackdriver_trace_clear, NULL)
    PHP_FE(stackdriver_trace_set_context, NULL)
    PHP_FE(stackdriver_trace_context, NULL)
    PHP_FE(stackdriver_debugger, NULL)
    PHP_FE(stackdriver_debugger_add_snapshot, NULL)
    PHP_FE_END
};

// Registers the lifecycle hooks for this extension
zend_module_entry stackdriver_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_STACKDRIVER_EXTNAME,
    stackdriver_functions,
    PHP_MINIT(stackdriver),
    PHP_MSHUTDOWN(stackdriver),
    PHP_RINIT(stackdriver),
    PHP_RSHUTDOWN(stackdriver),
    NULL, // name of the MINFO function or NULL if not applicable
    PHP_STACKDRIVER_VERSION,
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
    // allocate global request variables
#ifdef ZTS
    ts_allocate_id(&stackdriver_globals_id, sizeof(zend_stackdriver_globals), php_stackdriver_globals_ctor, NULL);
#else
    php_stackdriver_globals_ctor(&php_stackdriver_globals_ctor);
#endif

    stackdriver_trace_minit(INIT_FUNC_ARGS_PASSTHRU);
    stackdriver_debugger_minit(INIT_FUNC_ARGS_PASSTHRU);

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
    stackdriver_trace_rinit(TSRMLS_C);
    stackdriver_debugger_rinit(TSRMLS_C);
    return SUCCESS;
}

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(stackdriver)
{
    stackdriver_trace_rshutdown(TSRMLS_C);
    stackdriver_debugger_rshutdown(TSRMLS_C);
    return SUCCESS;
}
/* }}} */
