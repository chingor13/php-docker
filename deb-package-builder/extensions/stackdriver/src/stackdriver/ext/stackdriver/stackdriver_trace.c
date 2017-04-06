#include "php_stackdriver.h"
#include "stackdriver_trace.h"

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
        ht = Z_ARR_P(span_options);
        ZEND_HASH_FOREACH_KEY_VAL(ht, idx, k, v) {
            if (strcmp(ZSTR_VAL(k), "labels") == 0) {
                stackdriver_trace_add_labels(span, Z_ARR_P(v));
            } else if (ZSTR_VAL(k) == "startTime") {
                span->start = Z_DVAL_P(v);
            }
        } ZEND_HASH_FOREACH_END();
    }
}

struct stackdriver_trace_span_t *stackdriver_trace_begin(zend_string *function_name, zend_execute_data *execute_data TSRMLS_DC)
{
    struct stackdriver_trace_span_t *span = emalloc(sizeof(stackdriver_trace_span_t));

    span->start = stackdriver_trace_now();
    span->name = ZSTR_VAL(function_name);
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
    if (stackdriver_trace_finish()) {
        RETURN_TRUE;
    }
    RETURN_FALSE;
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

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Sz", &function_name, &handler) == FAILURE) {
        RETURN_FALSE;
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

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "SSz", &class_name, &function_name, &handler) == FAILURE) {
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
        array_init(&span);

        trace_span = STACKDRIVER_G(spans)[i];

        add_assoc_long(&span, "spanId", trace_span->span_id);
        if (trace_span->parent) {
            add_assoc_long(&span, "parentSpanId", trace_span->parent->span_id);
        }
        add_assoc_string(&span, "name", trace_span->name);
        add_assoc_double(&span, "startTime", trace_span->start);
        add_assoc_double(&span, "endTime", trace_span->stop);

        if (trace_span->labels) {
            zval labels;
            stackdriver_labels_to_zval_array(trace_span->labels, &labels);
            add_assoc_zval(&span, "labels", &labels);
        }

        add_next_index_zval(return_value, &span);
    }
}

/** BEGIN Stackdriver Trace Callbacks */

void stackdriver_trace_register(char *name)
{
    zval handler;
    zend_string *function_name = zend_string_init(name, strlen(name), 0);
    ZVAL_LONG(&handler, 1);

    zend_hash_add(STACKDRIVER_G(traced_functions), function_name, &handler);
}

void stackdriver_trace_register_callback(char *name, stackdriver_trace_callback cb)
{
    zval handler;
    zend_string *function_name = zend_string_init(name, strlen(name), 0);
    ZVAL_PTR(&handler, cb);

    zend_hash_add(STACKDRIVER_G(traced_functions), function_name, &handler);
}

zval *stackdriver_trace_find_callback(zend_string *function_name)
{
    return zend_hash_find(STACKDRIVER_G(traced_functions), function_name);
}

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

void stackdriver_trace_callback_eloquent_query(struct stackdriver_trace_span_t *span, zend_execute_data *data TSRMLS_DC)
{
    zend_class_entry *ce = NULL;
    zval *eloquent_model;
    zend_string *label;

    span->name = "eloquent/get";

    // obj is the Illuminate\Database\Eloquent\Builder
    zval *obj = ((data->This.value.obj) ? &(data->This) : NULL);
    if (obj) {
        eloquent_model = get_property(obj, "model");
        if (eloquent_model) {
            ce = eloquent_model->value.obj->ce;
            label = zend_string_init("model", 5, 0);
            stackdriver_trace_add_label(span, label, ce->name);
        }
    }
}
void stackdriver_trace_setup_automatic_tracing()
{
    // stackdriver_trace_register("Illuminate\\Foundation\\Application::boot");
    // stackdriver_trace_register("Illuminate\\Foundation\\Application::dispatch");
    // stackdriver_trace_register("Illuminate\\Session\\Middleware\\StartSession::startSession");
    // stackdriver_trace_register("Illuminate\\Session\\Middleware\\StartSession::collectGarbage");
    stackdriver_trace_register_callback("Illuminate\\Database\\Eloquent\\Builder::getModels", stackdriver_trace_callback_eloquent_query);
    stackdriver_trace_register("Illuminate\\Database\\Eloquent\\Model::performInsert");
    stackdriver_trace_register("Illuminate\\Database\\Eloquent\\Model::performUpdate");
    stackdriver_trace_register("Illuminate\\Database\\Eloquent\\Model::delete");
    stackdriver_trace_register("Illuminate\\Database\\Eloquent\\Model::destroy");
    // stackdriver_trace_register("Illuminate\\Events\\Dispatcher::fire");
    // stackdriver_trace_register("Illuminate\\View\\Engines\\CompilerEngine::get");
    // stackdriver_trace_register("Illuminate\\Routing\\Controller::callAction");
    // stackdriver_trace_register("PDO::exec");
    // stackdriver_trace_register("PDO::query");
    //
    // stackdriver_trace_register("mysql_query");
    // stackdriver_trace_register("mysqli_query");
    // stackdriver_trace_register("mysqli::query");
    // stackdriver_trace_register("mysqli::prepare");
    // stackdriver_trace_register("mysqli_prepare");
    //
    // stackdriver_trace_register("PDO::commit");
    // stackdriver_trace_register("mysqli::commit");
    // stackdriver_trace_register("mysqli_commit");
    //
    // stackdriver_trace_register("mysql_connect");
    // stackdriver_trace_register("mysqli_connect");
    // stackdriver_trace_register("mysqli::__construct");
    // stackdriver_trace_register("mysqli::mysqli");
    //
    // stackdriver_trace_register("PDO::__construct");
    //
    // stackdriver_trace_register("PDOStatement::execute");
    //
    // stackdriver_trace_register("mysqli_stmt_execute");
    // stackdriver_trace_register("mysqli_stmt::execute");
    //
    // stackdriver_trace_register("pg_query");
    // stackdriver_trace_register("pg_query_params");
    //
    // stackdriver_trace_register("pg_execute");
}

void stackdriver_trace_init()
{
    ALLOC_HASHTABLE(STACKDRIVER_G(traced_functions));
    zend_hash_init(STACKDRIVER_G(traced_functions), 16, NULL, ZVAL_PTR_DTOR, 0);

    stackdriver_trace_setup_automatic_tracing();

    STACKDRIVER_G(current_span) = NULL;
    STACKDRIVER_G(spans) = emalloc(64 * sizeof(struct stackdriver_trace_span_t *));
    STACKDRIVER_G(span_count) = 0;
}

void stackdriver_trace_teardown()
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
    efree(STACKDRIVER_G(spans));
}
