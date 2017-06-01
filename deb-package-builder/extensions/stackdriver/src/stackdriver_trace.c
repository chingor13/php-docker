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

#include "php_stackdriver.h"
#include <sys/time.h>
#include "stackdriver_trace.h"
#include "stackdriver_trace_span.h"
#include "stackdriver_trace_context.h"
#include "Zend/zend_compile.h"
#include "Zend/zend_closures.h"

// True global for storing the original zend_execute_ex function pointer
void (*original_zend_execute_ex) (zend_execute_data *execute_data TSRMLS_DC);

// Type for a trace callback handler
typedef void (*stackdriver_trace_callback)(stackdriver_trace_span_t *span, zend_execute_data *data TSRMLS_DC);

// Returns the callback handler for the specified function name.
// This is always a zval* and should be either an array or a pointer to a callback function.
static zval *stackdriver_trace_find_callback(zend_string *function_name)
{
    return zend_hash_find(STACKDRIVER_G(traced_functions), function_name);
}

static int stackdriver_trace_add_label(stackdriver_trace_span_t *span, zend_string *k, zend_string *v)
{
    zval zv;

    // instantiate labels if not already created
    if (span->labels == NULL) {
        span->labels = emalloc(sizeof(HashTable));
        zend_hash_init(span->labels, 4, NULL, ZVAL_PTR_DTOR, 0);
        // zend_hash_init(span->labels, 4, NULL, NULL, 0);
    }

    // put the string value into a zval and save it in the HashTable
    ZVAL_STRING(&zv, ZSTR_VAL(v));
    dump_zval(&zv);

    if (zend_hash_update(span->labels, zend_string_copy(k), &zv) == NULL) {
        php_printf("failed to update label hash table\n");
        return FAILURE;
    } else {
        return SUCCESS;
    }
}

static int stackdriver_trace_add_label_str(stackdriver_trace_span_t *span, char *k, zend_string *v)
{
    return stackdriver_trace_add_label(span, zend_string_init(k, strlen(k), 0), v);
}

static int stackdriver_trace_add_labels_merge(stackdriver_trace_span_t *span, zval *label_array)
{
    ulong idx;
    zend_string *k;
    zval *v;
    HashTable *ht = Z_ARRVAL_P(label_array);
    // instantiate labels if not already created
    if (span->labels == NULL) {
        span->labels = emalloc(sizeof(HashTable));
        zend_hash_init(span->labels, 4, NULL, ZVAL_PTR_DTOR, 0);
    }

    zend_hash_merge(span->labels, ht, zval_add_ref, 0);
    return SUCCESS;
}

static int stackdriver_trace_add_labels(stackdriver_trace_span_t *span, zval *label_array)
{
    ulong idx;
    zend_string *k;
    zval *v;
    HashTable *ht = Z_ARRVAL_P(label_array);

    ZEND_HASH_FOREACH_KEY_VAL(ht, idx, k, v) {
        if (stackdriver_trace_add_label(span, k, Z_STR_P(v)) != SUCCESS) {
            php_printf("failed to add label\n");
            return FAILURE;
        }
    } ZEND_HASH_FOREACH_END();
    return SUCCESS;
}


static double stackdriver_trace_now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (double) (tv.tv_sec + tv.tv_usec / 1000000.00);
}

// Update the provided span with the provided zval (array) of span options
static void stackdriver_trace_modify_span_with_array(stackdriver_trace_span_t *span, zval *span_options)
{
    HashTable *ht;
    ulong idx;
    zend_string *k;
    zval *v;
    ht = Z_ARR_P(span_options);

    ZEND_HASH_FOREACH_KEY_VAL(ht, idx, k, v) {
        if (strcmp(ZSTR_VAL(k), "labels") == 0) {
            stackdriver_trace_add_labels_merge(span, v);
        } else if (strcmp(ZSTR_VAL(k), "startTime") == 0) {
            span->start = Z_DVAL_P(v);
        } else if (strcmp(ZSTR_VAL(k), "name") == 0) {
            span->name = zend_string_copy(Z_STR_P(v));
        }
    } ZEND_HASH_FOREACH_END();
}

static int stackdriver_trace_zend_fcall_closure(zend_execute_data *execute_data, stackdriver_trace_span_t *span, zval *closure TSRMLS_DC)
{
    int i, num_args = ZEND_CALL_NUM_ARGS(execute_data);
    zend_fcall_info fci;
    zend_fcall_info_cache fcc;
    zval closure_result, *params[num_args];

    for (i = 0; i < num_args; i++) {
        params[i] = ZEND_CALL_VAR_NUM(execute_data, i);
    }

    if (zend_fcall_info_init(
            closure,
            0,
            &fci,
            &fcc,
            NULL,
            NULL
            TSRMLS_CC
        ) != SUCCESS) {
        php_printf("failed to initialize fcall info\n");
        return FAILURE;
    };

    fci.retval = &closure_result;
    fci.params = *params;
    fci.param_count = num_args;
    // fci.object = fcc.object = Z_OBJ_P(execute_data->object);

    fcc.initialized = 1;
    // fcc.called_scope = execute_data->called_scope;

    if (zend_call_function(&fci, &fcc TSRMLS_CC) != SUCCESS) {
          return FAILURE;
    }

    stackdriver_trace_modify_span_with_array(span, &closure_result);

    return SUCCESS;
}

static void stackdriver_trace_execute_callback(stackdriver_trace_span_t *span, zend_execute_data *execute_data, zval *span_options TSRMLS_DC)
{
    stackdriver_trace_callback cb;

    if (Z_TYPE_P(span_options) == IS_PTR) {
        cb = (stackdriver_trace_callback)Z_PTR_P(span_options);
        cb(span, execute_data TSRMLS_CC);
    } else if (Z_TYPE_P(span_options) == IS_ARRAY) {
        stackdriver_trace_modify_span_with_array(span, span_options);
    } else if (Z_TYPE_P(span_options) == IS_OBJECT) {
        if (Z_OBJCE_P(span_options) == zend_ce_closure) {
            stackdriver_trace_zend_fcall_closure(execute_data, span, span_options TSRMLS_CC);
        }
    }
}

static stackdriver_trace_span_t *stackdriver_trace_begin(zend_string *function_name, zend_execute_data *execute_data TSRMLS_DC)
{
    stackdriver_trace_span_t *span = emalloc(sizeof(stackdriver_trace_span_t));

    span->start = stackdriver_trace_now();
    span->name = zend_string_copy(function_name);
    span->span_id = php_mt_rand();
    span->labels = NULL;

    if (STACKDRIVER_G(current_span)) {
        span->parent = STACKDRIVER_G(current_span);
    } else {
        span->parent = NULL;
    }
    STACKDRIVER_G(current_span) = span;
    STACKDRIVER_G(spans)[STACKDRIVER_G(span_count)++] = span;

    return span;
}

static int stackdriver_trace_finish()
{
    stackdriver_trace_span_t *span = STACKDRIVER_G(current_span);

    if (!span) {
        return FAILURE;
    }

    // set current time for now
    span->stop = stackdriver_trace_now();

    STACKDRIVER_G(current_span) = span->parent;

    return SUCCESS;
}

static zend_string *stackdriver_generate_class_name(zend_string *class_name, zend_string *function_name)
{
    int len = class_name->len + function_name->len + 2;
    zend_string *result = zend_string_alloc(len, 0);

    strcpy(ZSTR_VAL(result), class_name->val);
    strcat(ZSTR_VAL(result), "::");
    strcat(ZSTR_VAL(result), function_name->val);
    return result;
}

static zend_string *stackdriver_trace_get_current_function_name()
{
    zend_execute_data *data;
    zend_string *result, *function_name;
    zend_function *current_function;

    data = EG(current_execute_data);

    // We don't have any current execution data -> don't care to trace
    if (!data) {
        return NULL;
    }

    // Fetch the function name from the current execution state
    current_function = data->func;
    function_name = current_function->common.function_name;

    // This is a special directive like 'require' -> don't care to trace
    if (!function_name) {
        return NULL;
    }

    // Fetch the current class if any
    if (current_function->common.scope) {
        result = stackdriver_generate_class_name(current_function->common.scope->name, function_name);
    } else {
        result = zend_string_copy(function_name);
    }

    return result;
}

PHP_FUNCTION(stackdriver_trace_begin)
{
    zend_string *function_name;
    zval *span_options;
    stackdriver_trace_span_t *span;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Sa", &function_name, &span_options) == FAILURE) {
        RETURN_FALSE;
    }

    span = stackdriver_trace_begin(function_name, execute_data TSRMLS_CC);
    stackdriver_trace_execute_callback(span, execute_data, span_options TSRMLS_CC);
    RETURN_TRUE;
}

PHP_FUNCTION(stackdriver_trace_finish)
{
    if (stackdriver_trace_finish() == SUCCESS) {
        RETURN_TRUE;
    }
    RETURN_FALSE;
}

static void stackdriver_trace_clear(TSRMLS_D)
{
    int i;
    stackdriver_trace_span_t *span;
    for (i = 0; i < STACKDRIVER_G(span_count); i++) {
        span = STACKDRIVER_G(spans)[i];
        if (span->labels) {
            efree(span->labels);
        }
        efree(span);
    }
    STACKDRIVER_G(span_count) = 0;
    STACKDRIVER_G(current_span) = NULL;
    STACKDRIVER_G(trace_id) = NULL;
    STACKDRIVER_G(trace_parent_span_id) = 0;
}

PHP_FUNCTION(stackdriver_trace_clear)
{
    stackdriver_trace_clear(TSRMLS_C);
    RETURN_TRUE;
}

PHP_FUNCTION(stackdriver_trace_set_context)
{
    zend_string *trace_id;
    long parent_span_id;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S|L", &trace_id, &parent_span_id) == FAILURE) {
        RETURN_FALSE;
    }

    STACKDRIVER_G(trace_id) = zend_string_copy(trace_id);
    STACKDRIVER_G(trace_parent_span_id) = parent_span_id;
}

PHP_FUNCTION(stackdriver_trace_context)
{
    stackdriver_trace_span_t *span = STACKDRIVER_G(current_span);
    object_init_ex(return_value, stackdriver_trace_context_ce);

    if (span) {
        zend_update_property_long(stackdriver_trace_context_ce, return_value, "spanId", sizeof("spanId") - 1, span->span_id);
    }
    if (STACKDRIVER_G(trace_id)) {
        zend_update_property_str(stackdriver_trace_context_ce, return_value, "traceId", sizeof("traceId") - 1, STACKDRIVER_G(trace_id));
    }
}

/**
 * This method replaces the internal zend_execute_ex method used to dispatch calls
 * to user space code. The original zend_execute_ex method is moved to
 * STACKDRIVER_G(_zend_execute_ex)
 */
void stackdriver_trace_execute_ex (zend_execute_data *execute_data TSRMLS_DC) {
    zend_string *function_name = stackdriver_trace_get_current_function_name();
    zval *trace_handler;
    stackdriver_trace_span_t *span;

    if (function_name) {
        trace_handler = stackdriver_trace_find_callback(function_name);

        if (trace_handler != NULL) {
            span = stackdriver_trace_begin(function_name, execute_data TSRMLS_CC);
            original_zend_execute_ex(execute_data TSRMLS_CC);
            stackdriver_trace_execute_callback(span, execute_data, trace_handler TSRMLS_CC);
            stackdriver_trace_finish();
        } else {
            original_zend_execute_ex(execute_data TSRMLS_CC);
        }
    } else {
        original_zend_execute_ex(execute_data TSRMLS_CC);
    }
}

/* {{{ proto int stackdriver_trace_function($function_name, $handler)
Trace a function call */
PHP_FUNCTION(stackdriver_trace_function)
{
    zend_string *function_name;
    zval *handler, *copy;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S|z", &function_name, &handler) == FAILURE) {
        RETURN_FALSE;
    }

    if (handler == NULL) {
        zval h;
        ZVAL_LONG(&h, 1);
        handler = &h;
    }

    PHP_STACKDRIVER_MAKE_STD_ZVAL(copy);
    ZVAL_ZVAL(copy, handler, 1, 0);

    zend_hash_update(STACKDRIVER_G(traced_functions), function_name, copy);
    RETURN_TRUE;
}

/* {{{ proto int stackdriver_trace_method($class_name, $function_name, $handler)
Trace a class method or instance method */
PHP_FUNCTION(stackdriver_trace_method)
{
    zend_function *fe;
    zend_string *class_name, *function_name, *key;
    zval *handler, *copy;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "SS|z", &class_name, &function_name, &handler) == FAILURE) {
        RETURN_FALSE;
    }

    if (handler == NULL) {
        zval h;
        ZVAL_LONG(&h, 1);
        handler = &h;
    }

    PHP_STACKDRIVER_MAKE_STD_ZVAL(copy);
    ZVAL_ZVAL(copy, handler, 1, 0);

    key = stackdriver_generate_class_name(class_name, function_name);
    zend_hash_update(STACKDRIVER_G(traced_functions), key, handler);

    RETURN_FALSE;
}


static int stackdriver_labels_to_zval_array(HashTable *ht, zval *label_array)
{
    ulong idx;
    zend_string *k;
    zval *v;
    HashTable *label_ht;

    array_init(label_array);
    label_ht = Z_ARRVAL_P(label_array);

    ZEND_HASH_FOREACH_KEY_VAL(ht, idx, k, v) {
        if (add_assoc_str(label_array, ZSTR_VAL(k), Z_STR_P(v)) != SUCCESS) {
            php_prinf("failed to add_assoc_zval\n");
            return FAILURE;
        }

        // php_printf("stored\n");
    } ZEND_HASH_FOREACH_END();

    return SUCCESS;
}

/* {{{ proto int stackdriver_trace_method()
Fetch the Object Handle ID from an instance */
PHP_FUNCTION(stackdriver_trace_list)
{
    int i;
    stackdriver_trace_span_t *trace_span;
    int num_spans = STACKDRIVER_G(span_count);
    zval labels[num_spans], spans[num_spans];

    // Set up return value to be an array of size num_spans
    array_init(return_value);

    for (i = 0; i < num_spans; i++) {
        object_init_ex(&spans[i], stackdriver_trace_span_ce);

        trace_span = STACKDRIVER_G(spans)[i];
        zend_update_property_long(stackdriver_trace_span_ce, &spans[i], "spanId", sizeof("spanId") - 1, trace_span->span_id);
        if (trace_span->parent) {
            zend_update_property_long(stackdriver_trace_span_ce, &spans[i], "parentSpanId", sizeof("parentSpanId") - 1, trace_span->parent->span_id);
        } else if (STACKDRIVER_G(trace_parent_span_id)) {
            zend_update_property_long(stackdriver_trace_span_ce, &spans[i], "parentSpanId", sizeof("parentSpanId") - 1, STACKDRIVER_G(trace_parent_span_id));
        }
        zend_update_property_str(stackdriver_trace_span_ce, &spans[i], "name", sizeof("name") - 1, trace_span->name);
        zend_update_property_double(stackdriver_trace_span_ce, &spans[i], "startTime", sizeof("startTime") - 1, trace_span->start);
        zend_update_property_double(stackdriver_trace_span_ce, &spans[i], "endTime", sizeof("endTime") - 1, trace_span->stop);

        array_init(&labels[i]);
        if (trace_span->labels) {
            stackdriver_labels_to_zval_array(trace_span->labels, &labels[i]);
        }
        zend_update_property(stackdriver_trace_span_ce, &spans[i], "labels", sizeof("labels") - 1, &labels[i]);

        add_next_index_zval(return_value, &spans[i]);
    }
}

/** BEGIN Stackdriver Trace Callbacks */

// Registers the specified method for automatic tracing. The name of the created span will be
// the provided function name.
//
// Example:
//    stackdriver_trace_register("get_sidebar");
//    stackdriver_trace_register("MyClass::someMethod");
static void stackdriver_trace_register(char *name)
{
    zval handler;
    zend_string *function_name = zend_string_init(name, strlen(name), 0);
    ZVAL_LONG(&handler, 1);

    zend_hash_add(STACKDRIVER_G(traced_functions), function_name, &handler);
}

// Registers the specified method for automatic tracing. The name of the created span will
// default to the provided function name. After executing the function to trace, the provided
// callback will be executed, allowing you to modify the created span.
static void stackdriver_trace_register_callback(char *name, stackdriver_trace_callback cb)
{
    zval handler;
    zend_string *function_name = zend_string_init(name, strlen(name), 0);
    ZVAL_PTR(&handler, cb);

    zend_hash_add(STACKDRIVER_G(traced_functions), function_name, &handler);
}



// Returns the zval property of the provided zval
// TODO: there must be a better way to get this than iterating over the HashTable
static zval *get_property(zval *obj, char *property_name)
{
    int offset = 0;
    zend_string *k;
    zend_class_entry *ce = obj->value.obj->ce;

    ZEND_HASH_FOREACH_STR_KEY(&ce->properties_info, k) {
        if (strcmp(property_name, ZSTR_VAL(k)) == 0) {
            return &obj->value.obj->properties_table[offset];
        }
        offset++;
    } ZEND_HASH_FOREACH_END();

    return NULL;
}

static void stackdriver_trace_callback_eloquent_query(stackdriver_trace_span_t *span, zend_execute_data *execute_data TSRMLS_DC)
{
    zend_class_entry *ce = NULL;
    zval *eloquent_model;

    span->name = zend_string_init("eloquent/get", 12, 0);

    // obj is the Illuminate\Database\Eloquent\Builder
    zval *obj = ((execute_data->This.value.obj) ? &(execute_data->This) : NULL);
    if (obj) {
        eloquent_model = get_property(obj, "model");
        if (eloquent_model) {
            ce = eloquent_model->value.obj->ce;
            stackdriver_trace_add_label_str(span, "model", ce->name);
        }
    }
}

static void stackdriver_trace_callback_laravel_view(stackdriver_trace_span_t *span, zend_execute_data *execute_data TSRMLS_DC)
{
    zval *path = EX_VAR_NUM(0);

    span->name = zend_string_init("laravel/view", 12, 0);

    if (path != NULL && Z_TYPE_P(path) == IS_STRING) {
        stackdriver_trace_add_label_str(span, "path", Z_STR_P(path));
    }
}

static void stackdriver_trace_setup_automatic_tracing()
{
    // Guzzle

    // curl
    stackdriver_trace_register("curl_exec");
    stackdriver_trace_register("curl_multi_add_handle");
    stackdriver_trace_register("curl_multi_remove_handle");

    // Wordpress
    stackdriver_trace_register("get_sidebar");
    stackdriver_trace_register("get_header");
    stackdriver_trace_register("get_footer");
    stackdriver_trace_register("load_textdomain");
    stackdriver_trace_register("setup_theme");
    stackdriver_trace_register("load_template");

    // Laravel
    stackdriver_trace_register_callback("Illuminate\\Database\\Eloquent\\Builder::getModels", stackdriver_trace_callback_eloquent_query);
    stackdriver_trace_register("Illuminate\\Database\\Eloquent\\Model::performInsert");
    stackdriver_trace_register("Illuminate\\Database\\Eloquent\\Model::performUpdate");
    stackdriver_trace_register("Illuminate\\Database\\Eloquent\\Model::delete");
    stackdriver_trace_register("Illuminate\\Database\\Eloquent\\Model::destroy");
    stackdriver_trace_register_callback("Illuminate\\View\\Engines\\CompilerEngine::get", stackdriver_trace_callback_laravel_view);

    // Symfony
    stackdriver_trace_register("Doctrine\\ORM\\Persisters\\BasicEntityPersister::load");
    stackdriver_trace_register("Doctrine\\ORM\\Persisters\\BasicEntityPersister::loadAll");
    stackdriver_trace_register("Doctrine\\ORM\\Persisters\\Entity\\BasicEntityPersister::load");
    stackdriver_trace_register("Doctrine\\ORM\\Persisters\\Entity\\BasicEntityPersister::loadAll");
    stackdriver_trace_register("Doctrine\\ORM\\AbstractQuery::execute");

    // PDO
    stackdriver_trace_register("PDO::exec");
    stackdriver_trace_register("PDO::query");
    stackdriver_trace_register("PDO::commit");
    stackdriver_trace_register("PDO::__construct");
    stackdriver_trace_register("PDOStatement::execute");

    // mysql
    stackdriver_trace_register("mysql_query");
    stackdriver_trace_register("mysql_connect");

    // mysqli
    stackdriver_trace_register("mysqli_query");
    stackdriver_trace_register("mysqli::query");
    stackdriver_trace_register("mysqli::prepare");
    stackdriver_trace_register("mysqli_prepare");
    stackdriver_trace_register("mysqli::commit");
    stackdriver_trace_register("mysqli_commit");
    stackdriver_trace_register("mysqli_connect");
    stackdriver_trace_register("mysqli::__construct");
    stackdriver_trace_register("mysqli::mysqli");
    stackdriver_trace_register("mysqli_stmt_execute");
    stackdriver_trace_register("mysqli_stmt::execute");

    // pg (postgres)
    stackdriver_trace_register("pg_query");
    stackdriver_trace_register("pg_query_params");
    stackdriver_trace_register("pg_execute");

    // memcache
    stackdriver_trace_register("Memcache::get");
    stackdriver_trace_register("Memcache::set");
    stackdriver_trace_register("Memcache::delete");
    stackdriver_trace_register("Memcache::flush");
    stackdriver_trace_register("Memcache::replace");
    stackdriver_trace_register("Memcache::increment");
    stackdriver_trace_register("Memcache::decrement");
}

int stackdriver_trace_minit(INIT_FUNC_ARGS)
{
    // Save original zend execute functions and use our own to instrument function calls
    original_zend_execute_ex = zend_execute_ex;
    zend_execute_ex = stackdriver_trace_execute_ex;

    stackdriver_trace_span_minit(INIT_FUNC_ARGS_PASSTHRU);
    stackdriver_trace_context_minit(INIT_FUNC_ARGS_PASSTHRU);

    return SUCCESS;
}

int stackdriver_trace_rinit(TSRMLS_D)
{
    ALLOC_HASHTABLE(STACKDRIVER_G(traced_functions));
    zend_hash_init(STACKDRIVER_G(traced_functions), 16, NULL, ZVAL_PTR_DTOR, 0);

    stackdriver_trace_setup_automatic_tracing();

    STACKDRIVER_G(current_span) = NULL;
    STACKDRIVER_G(spans) = emalloc(64 * sizeof(stackdriver_trace_span_t *));
    STACKDRIVER_G(span_count) = 0;
    STACKDRIVER_G(trace_id) = NULL;
    STACKDRIVER_G(trace_parent_span_id) = 0;

    return SUCCESS;
}

int stackdriver_trace_rshutdown(TSRMLS_D)
{
    stackdriver_trace_clear(TSRMLS_C);
    efree(STACKDRIVER_G(spans));

    return SUCCESS;
}
