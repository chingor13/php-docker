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
#include "stackdriver_trace.h"
#include "stackdriver_trace_span.h"
#include "stackdriver_trace_context.h"
#include "Zend/zend_compile.h"

typedef void (*stackdriver_trace_callback)(struct stackdriver_trace_span_t *span, zend_execute_data *data TSRMLS_DC);
zval *stackdriver_trace_find_callback(zend_string *function_name);

void stackdriver_trace_add_label(struct stackdriver_trace_span_t *span, zend_string *k, zend_string *v)
{
    // instantiate labels if not already created
    if (span->labels == NULL) {
        span->labels = emalloc(sizeof(HashTable));
        zend_hash_init(span->labels, 4, NULL, ZVAL_PTR_DTOR, 0);
    }

    zend_hash_update_ptr(span->labels, k, v);
}

void stackdriver_trace_add_label_str(struct stackdriver_trace_span_t *span, char *k, zend_string *v)
{
    stackdriver_trace_add_label(span, zend_string_init(k, strlen(k), 0), v);
}

void stackdriver_trace_add_labels(struct stackdriver_trace_span_t *span, HashTable *ht)
{
    ulong idx;
    zend_string *k, *copy;
    zval *v;

    ZEND_HASH_FOREACH_KEY_VAL(ht, idx, k, v) {
        copy = zend_string_init(Z_STRVAL_P(v), strlen(Z_STRVAL_P(v)), 0);
        stackdriver_trace_add_label(span, k, copy);
    } ZEND_HASH_FOREACH_END();
}


void stackdriver_labels_to_zval_array(HashTable *ht, zval *return_value)
{
    ulong idx;
    zend_string *k, *v;

    array_init(return_value);

    ZEND_HASH_FOREACH_KEY_PTR(ht, idx, k, v) {
        add_assoc_string(return_value, ZSTR_VAL(k), ZSTR_VAL(v));
    } ZEND_HASH_FOREACH_END();
}

double stackdriver_trace_now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (double) (tv.tv_sec + tv.tv_usec / 1000000.00);
}

void stackdriver_trace_execute_callback(struct stackdriver_trace_span_t *span, zend_execute_data *execute_data, zval *span_options TSRMLS_DC)
{
    HashTable *ht;
    ulong idx;
    zend_string *k;
    zval *v;
    stackdriver_trace_callback cb;

    if (Z_TYPE_P(span_options) == IS_PTR) {
        cb = (stackdriver_trace_callback)Z_PTR_P(span_options);
        cb(span, execute_data TSRMLS_CC);
    } else if (Z_TYPE_P(span_options) == IS_ARRAY) {
        // php_printf("callback is an array");
        ht = Z_ARR_P(span_options);
        ZEND_HASH_FOREACH_KEY_VAL(ht, idx, k, v) {
            if (strcmp(ZSTR_VAL(k), "labels") == 0) {
                stackdriver_trace_add_labels(span, Z_ARR_P(v));
            } else if (strcmp(ZSTR_VAL(k), "startTime") == 0) {
                span->start = Z_DVAL_P(v);
            } else if (strcmp(ZSTR_VAL(k), "name") == 0) {
                span->name = Z_STR_P(v);
            }
        } ZEND_HASH_FOREACH_END();
    }
}

struct stackdriver_trace_span_t *stackdriver_trace_begin(zend_string *function_name, zend_execute_data *execute_data TSRMLS_DC)
{
    struct stackdriver_trace_span_t *span = emalloc(sizeof(stackdriver_trace_span_t));

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

int stackdriver_trace_finish()
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

zend_string *stackdriver_generate_class_name(zend_string *class_name, zend_string *function_name)
{
    int len = class_name->len + function_name->len + 2;
    zend_string *result = zend_string_alloc(len, 0);

    strcpy(ZSTR_VAL(result), class_name->val);
    strcat(ZSTR_VAL(result), "::");
    strcat(ZSTR_VAL(result), function_name->val);
    return result;
}

zend_string *stackdriver_trace_get_current_function_name()
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
    struct stackdriver_trace_span_t *span;

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

void stackdriver_trace_clear(TSRMLS_D)
{
    int i;
    struct stackdriver_trace_span_t *span;
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
    struct stackdriver_trace_span_t *span = STACKDRIVER_G(current_span);
    object_init_ex(return_value, stackdriver_trace_context_ce);

    if (span) {
        zend_update_property_long(stackdriver_trace_context_ce, return_value, "spanId", sizeof("spanId") - 1, span->span_id);
    }
    if (STACKDRIVER_G(trace_id)) {
        zend_update_property_str(stackdriver_trace_context_ce, return_value, "traceId", sizeof("traceId") - 1, STACKDRIVER_G(trace_id));
    }
}

void stackdriver_trace_execute_internal(zend_execute_data *execute_data,
                                                      struct _zend_fcall_info *fci, int ret TSRMLS_DC) {
    php_printf("before internal\n");
    STACKDRIVER_G(_zend_execute_internal)(execute_data, fci, ret TSRMLS_CC);
    php_printf("after internal\n");
}

/**
 * This method replaces the internal zend_execute_ex method used to dispatch calls
 * to user space code. The original zend_execute_ex method is moved to
 * STACKDRIVER_G(_zend_execute_ex)
 */
void stackdriver_trace_execute_ex (zend_execute_data *execute_data TSRMLS_DC) {
    zend_string *function_name = stackdriver_trace_get_current_function_name();
    zval *trace_handler;
    struct stackdriver_trace_span_t *span;

    if (function_name) {
        trace_handler = stackdriver_trace_find_callback(function_name);

        if (trace_handler != NULL) {
            span = stackdriver_trace_begin(function_name, execute_data TSRMLS_CC);
            STACKDRIVER_G(_zend_execute_ex)(execute_data TSRMLS_CC);
            stackdriver_trace_execute_callback(span, execute_data, trace_handler TSRMLS_CC);
            stackdriver_trace_finish();
        } else {
            STACKDRIVER_G(_zend_execute_ex)(execute_data TSRMLS_CC);
        }
    } else {
        STACKDRIVER_G(_zend_execute_ex)(execute_data TSRMLS_CC);
    }
}

/* {{{ proto int stackdriver_trace_function($function_name, $handler)
Trace a function call */
PHP_FUNCTION(stackdriver_trace_function)
{
    zend_string *function_name;
    zval *handler;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S|z", &function_name, &handler) == FAILURE) {
        RETURN_FALSE;
    }

    if (handler == NULL) {
        zval h;
        ZVAL_LONG(&h, 1);
        handler = &h;
    }

    zend_hash_update(STACKDRIVER_G(traced_functions), function_name, handler);
    RETURN_TRUE;
}

/* {{{ proto int stackdriver_trace_method($class_name, $function_name, $handler)
Trace a class method or instance method */
PHP_FUNCTION(stackdriver_trace_method)
{
    zend_function *fe;
    zend_string *class_name, *function_name, *key;
    zval *handler;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "SS|z", &class_name, &function_name, &handler) == FAILURE) {
        RETURN_FALSE;
    }

    key = stackdriver_generate_class_name(class_name, function_name);
    zend_hash_update(STACKDRIVER_G(traced_functions), key, handler);

    RETURN_FALSE;
}

/* {{{ proto int stackdriver_trace_method()
Fetch the Object Handle ID from an instance */
PHP_FUNCTION(stackdriver_trace_list)
{
    int i;
    stackdriver_trace_span_t *trace_span;
    int num_spans = STACKDRIVER_G(span_count);
    ulong idx;
    zend_string *k;
    zval *v;

    array_init(return_value);

    for (i = 0; i < num_spans; i++) {
        zval span;
        object_init_ex(&span, stackdriver_trace_span_ce);

        trace_span = STACKDRIVER_G(spans)[i];
        zend_update_property_long(stackdriver_trace_span_ce, &span, "spanId", sizeof("spanId") - 1, trace_span->span_id);
        if (trace_span->parent) {
            zend_update_property_long(stackdriver_trace_span_ce, &span, "parentSpanId", sizeof("parentSpanId") - 1, trace_span->parent->span_id);
        } else if (STACKDRIVER_G(trace_parent_span_id)) {
            zend_update_property_long(stackdriver_trace_span_ce, &span, "parentSpanId", sizeof("parentSpanId") - 1, STACKDRIVER_G(trace_parent_span_id));
        }
        zend_update_property_str(stackdriver_trace_span_ce, &span, "name", sizeof("name") - 1, trace_span->name);
        zend_update_property_double(stackdriver_trace_span_ce, &span, "startTime", sizeof("startTime") - 1, trace_span->start);
        zend_update_property_double(stackdriver_trace_span_ce, &span, "endTime", sizeof("endTime") - 1, trace_span->stop);
        if (trace_span->labels) {
            zval labels;
            stackdriver_labels_to_zval_array(trace_span->labels, &labels);
            zend_update_property(stackdriver_trace_span_ce, &span, "labels", sizeof("labels") - 1, &labels);
        }

        add_next_index_zval(return_value, &span);
    }
}

/** BEGIN Stackdriver Trace Callbacks */

// Registers the specified method for automatic tracing. The name of the created span will be
// the provided function name.
//
// Example:
//    stackdriver_trace_register("get_sidebar");
//    stackdriver_trace_register("MyClass::someMethod");
void stackdriver_trace_register(char *name)
{
    zval handler;
    zend_string *function_name = zend_string_init(name, strlen(name), 0);
    ZVAL_LONG(&handler, 1);

    zend_hash_add(STACKDRIVER_G(traced_functions), function_name, &handler);
}

// Registers the specified method for automatic tracing. The name of the created span will
// default to the provided function name. After executing the function to trace, the provided
// callback will be executed, allowing you to modify the created span.
void stackdriver_trace_register_callback(char *name, stackdriver_trace_callback cb)
{
    zval handler;
    zend_string *function_name = zend_string_init(name, strlen(name), 0);
    ZVAL_PTR(&handler, cb);

    zend_hash_add(STACKDRIVER_G(traced_functions), function_name, &handler);
}

// Returns the callback handler for the specified function name.
// This is always a zval* and should be either an array or a pointer to a callback function.
zval *stackdriver_trace_find_callback(zend_string *function_name)
{
    return zend_hash_find(STACKDRIVER_G(traced_functions), function_name);
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

void stackdriver_trace_callback_eloquent_query(struct stackdriver_trace_span_t *span, zend_execute_data *execute_data TSRMLS_DC)
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

void stackdriver_trace_callback_laravel_view(struct stackdriver_trace_span_t *span, zend_execute_data *execute_data TSRMLS_DC)
{
    zval *path = EX_VAR_NUM(0);

    span->name = zend_string_init("laravel/view", 12, 0);

    if (path != NULL && Z_TYPE_P(path) == IS_STRING) {
        stackdriver_trace_add_label_str(span, "path", Z_STR_P(path));
    }
}

void stackdriver_trace_setup_automatic_tracing()
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

void stackdriver_trace_init(TSRMLS_D)
{
    ALLOC_HASHTABLE(STACKDRIVER_G(traced_functions));
    zend_hash_init(STACKDRIVER_G(traced_functions), 16, NULL, ZVAL_PTR_DTOR, 0);

    stackdriver_trace_setup_automatic_tracing();

    STACKDRIVER_G(current_span) = NULL;
    STACKDRIVER_G(spans) = emalloc(64 * sizeof(struct stackdriver_trace_span_t *));
    STACKDRIVER_G(span_count) = 0;
    STACKDRIVER_G(trace_id) = NULL;
    STACKDRIVER_G(trace_parent_span_id) = 0;
}

void stackdriver_trace_teardown(TSRMLS_D)
{
    stackdriver_trace_clear(TSRMLS_C);
    efree(STACKDRIVER_G(spans));
}
