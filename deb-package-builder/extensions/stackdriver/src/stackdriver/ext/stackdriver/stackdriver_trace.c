#include "php_stackdriver.h"
#include "stackdriver_trace.h"

void stackdriver_trace_add_label_zval(struct stackdriver_trace_span_t *span, zend_string *key, zval *value)
{
    if (span->labels == NULL) {
        span->labels = emalloc(sizeof(HashTable));
        zend_hash_init(span->labels, 4, NULL, ZVAL_PTR_DTOR, 0);
    }

    zend_hash_update(span->labels, key, value);
}

void stackdriver_trace_add_label(struct stackdriver_trace_span_t *span, zend_string *key, zend_string *value)
{
    zval data;
    ZVAL_STR(&data, value);
    stackdriver_trace_add_label_zval(span, key, &data);
}

void stackdriver_trace_add_label_str(struct stackdriver_trace_span_t *span, char *key, zend_string *value)
{
    zend_string *zend_key = zend_string_init(key, strlen(key), 0);
    stackdriver_trace_add_label(span, zend_key, value);
    zend_string_release(zend_key);
}

int stackdriver_trace_begin(zend_string *function_name, zval *handler)
{
    struct stackdriver_trace_span_t *span = emalloc(sizeof(stackdriver_trace_span_t));
    gettimeofday(&span->start, NULL);
    span->name = ZSTR_VAL(function_name);
    span->span_id = php_mt_rand();

    if (STACKDRIVER_G(current_span)) {
        span->parent = STACKDRIVER_G(current_span);
    }
    STACKDRIVER_G(current_span) = span;
    STACKDRIVER_G(spans)[STACKDRIVER_G(span_count)++] = span;

    stackdriver_trace_add_label_str(span, "function_name", function_name);

    return SUCCESS;
}


int stackdriver_trace_finish()
{
    stackdriver_trace_span_t *span = STACKDRIVER_G(current_span);

    if (!span) {
        return SUCCESS;
    }

    gettimeofday(&(span->stop), NULL);

    STACKDRIVER_G(current_span) = span->parent;

    return SUCCESS;
}

PHP_FUNCTION(stackdriver_trace_begin)
{
    zval *spanOptions;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &spanOptions) == FAILURE) {
        RETURN_FALSE;
    }
    php_stackdriver_trace_begin(spanOptions);
    RETURN_TRUE;
}

PHP_FUNCTION(stackdriver_trace_finish)
{
    php_stackdriver_trace_finish();
    RETURN_TRUE;
}

void stackdriver_trace_execute_internal(zend_execute_data *execute_data,
                                                      struct _zend_fcall_info *fci, int ret TSRMLS_DC) {
    php_printf("before internal\n");
    STACKDRIVER_G(_zend_execute_internal)(execute_data, fci, ret TSRMLS_CC);
    php_printf("after internal\n");
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

void stackdriver_trace_execute_ex (zend_execute_data *execute_data TSRMLS_DC) {
    zend_string *function_name = stackdriver_trace_get_current_function_name();
    zval *trace_handler;

    if (function_name) {
        trace_handler = zend_hash_find(STACKDRIVER_G(traced_functions), function_name);

        if (trace_handler != NULL) {
            stackdriver_trace_begin(function_name, trace_handler);
            STACKDRIVER_G(_zend_execute_ex)(execute_data TSRMLS_CC);
            stackdriver_trace_finish();
        } else {
            STACKDRIVER_G(_zend_execute_ex)(execute_data TSRMLS_CC);
        }

        // clean up the zend_string_init from stackdriver_trace_get_current_function_name();
        // zend_string_release(function_name);
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

void stackdriver_trace_register(char *name)
{
    zval handler;
    zend_string *function_name = zend_string_init(name, strlen(name), 0);
    ZVAL_LONG(&handler, 1);

    zend_hash_add(STACKDRIVER_G(traced_functions), function_name, &handler);
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

double stackdriver_timeval_to_timestamp(struct timeval *tv) {
    return (double) (tv->tv_sec + tv->tv_usec / 1000000.00);
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

        add_assoc_long(&span, "id", trace_span->span_id);
        if (trace_span->parent) {
            add_assoc_long(&span, "parent_id", trace_span->parent->span_id);
        }
        add_assoc_string(&span, "name", trace_span->name);
        add_assoc_double(&span, "start", stackdriver_timeval_to_timestamp(&trace_span->start));
        add_assoc_double(&span, "stop", stackdriver_timeval_to_timestamp(&trace_span->stop));

        if (trace_span->labels) {
            zval labels;
            array_init(&labels);

            ZEND_HASH_FOREACH_KEY_VAL(trace_span->labels, idx, k, v) {
                add_assoc_zval(&labels, ZSTR_VAL(k), v);
            } ZEND_HASH_FOREACH_END();
            add_assoc_zval(&span, "labels", &labels);
        }

        add_next_index_zval(return_value, &span);
    }
}

void stackdriver_trace_setup_automatic_tracing()
{
    // stackdriver_trace_register("Illuminate\\Foundation\\Application::boot");
    // stackdriver_trace_register("Illuminate\\Foundation\\Application::dispatch");
    // stackdriver_trace_register("Illuminate\\Session\\Middleware\\StartSession::startSession");
    // stackdriver_trace_register("Illuminate\\Session\\Middleware\\StartSession::collectGarbage");
    stackdriver_trace_register("Illuminate\\Database\\Eloquent\\Builder::getModels");
    stackdriver_trace_register("Illuminate\\Database\\Eloquent\\Model::performInsert");
    stackdriver_trace_register("Illuminate\\Database\\Eloquent\\Model::performUpdate");
    stackdriver_trace_register("Illuminate\\Database\\Eloquent\\Model::delete");
    stackdriver_trace_register("Illuminate\\Database\\Eloquent\\Model::destroy");
    // stackdriver_trace_register("Illuminate\\Events\\Dispatcher::fire");
    // stackdriver_trace_register("Illuminate\\View\\Engines\\CompilerEngine::get");
    // stackdriver_trace_register("Illuminate\\Routing\\Controller::callAction");
    stackdriver_trace_register("PDO::exec");
    stackdriver_trace_register("PDO::query");

    stackdriver_trace_register("mysql_query");
    stackdriver_trace_register("mysqli_query");
    stackdriver_trace_register("mysqli::query");
    stackdriver_trace_register("mysqli::prepare");
    stackdriver_trace_register("mysqli_prepare");

    stackdriver_trace_register("PDO::commit");
    stackdriver_trace_register("mysqli::commit");
    stackdriver_trace_register("mysqli_commit");

    stackdriver_trace_register("mysql_connect");
    stackdriver_trace_register("mysqli_connect");
    stackdriver_trace_register("mysqli::__construct");
    stackdriver_trace_register("mysqli::mysqli");

    stackdriver_trace_register("PDO::__construct");

    stackdriver_trace_register("PDOStatement::execute");

    stackdriver_trace_register("mysqli_stmt_execute");
    stackdriver_trace_register("mysqli_stmt::execute");

    stackdriver_trace_register("pg_query");
    stackdriver_trace_register("pg_query_params");

    stackdriver_trace_register("pg_execute");
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
    efree(STACKDRIVER_G(spans));
}
