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

/*
 * This is the implementation of the Stackdriver\Trace\Span class. The PHP equivalent is:
 *
 * namespace Stackdriver\Trace;
 *
 * class Span {
 *   protected $name = "unknown";
 *   protected $spanId;
 *   protected $parentSpanId;
 *   protected $startTime;
 *   protected $endTime;
 *   protected $labels;
 *
 *   public function __construct(array $spanOptions)
 *   {
 *     foreach ($spanOptions as $k => $v) {
 *       $this->__set($k, $v);
 *     }
 *   }
 *
 *   public function name()
 *   {
 *     return $this->name;
 *   }
 *
 *   public function spanId()
 *   {
 *     return $this->spanId;
 *   }
 *
 *   public function spanId()
 *   {
 *     return $this->parentSpanId;
 *   }
 *
 *   public function startTime()
 *   {
 *     return $this->startTime;
 *   }
 *
 *   public function endTime()
 *   {
 *     return $this->endTime;
 *   }
 *
 *   public function labels()
 *   {
 *     return $this->labels;
 *   }
 * }
 */

#include "stackdriver_trace_span.h"

extern zend_class_entry* stackdriver_trace_span_ce = NULL;

ZEND_BEGIN_ARG_INFO(StackdriverTraceSpan_construct_arginfo, 0)
	ZEND_ARG_ARRAY_INFO(0, spanOptions, 0)
ZEND_END_ARG_INFO();

/* {{{ proto Stackdriver\Trace\Span::__construct(array $spanOptions) }}} */
static PHP_METHOD(StackdriverTraceSpan, __construct) {
    zval *zval_span_options, *v;
    ulong idx;
    zend_string *k;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &zval_span_options) == FAILURE) {
        RETURN_FALSE;
    }

    zend_array *span_options = Z_ARR_P(zval_span_options);
    ZEND_HASH_FOREACH_KEY_VAL(span_options, idx, k, v) {
        zend_update_property(stackdriver_trace_span_ce, getThis(), ZSTR_VAL(k), strlen(ZSTR_VAL(k)), v);
    } ZEND_HASH_FOREACH_END();
}

/* {{{ proto Stackdriver\Trace\Span::name() }}} */
static PHP_METHOD(StackdriverTraceSpan, name) {
    zval *val, rv;

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }

    val = zend_read_property(stackdriver_trace_span_ce, getThis(), "name", sizeof("name") - 1, 1, &rv);

    RETURN_ZVAL(val, 1, 0);
}

/* {{{ proto Stackdriver\Trace\Span::spanId() }}} */
static PHP_METHOD(StackdriverTraceSpan, spanId) {
    zval *val, rv;

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }

    val = zend_read_property(stackdriver_trace_span_ce, getThis(), "spanId", sizeof("spanId") - 1, 1, &rv);

    RETURN_ZVAL(val, 1, 0);
}

/* {{{ proto Stackdriver\Trace\Span::parentSpanId() }}} */
static PHP_METHOD(StackdriverTraceSpan, parentSpanId) {
    zval *val, rv;

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }

    val = zend_read_property(stackdriver_trace_span_ce, getThis(), "parentSpanId", sizeof("parentSpanId") - 1, 1, &rv);

    RETURN_ZVAL(val, 1, 0);
}

/* {{{ proto Stackdriver\Trace\Span::labels() }}} */
static PHP_METHOD(StackdriverTraceSpan, labels) {
    zval *val, rv;

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }

    val = zend_read_property(stackdriver_trace_span_ce, getThis(), "labels", sizeof("labels") - 1, 1, &rv);

    RETURN_ZVAL(val, 1, 0);
}

/* {{{ proto Stackdriver\Trace\Span::startTime() }}} */
static PHP_METHOD(StackdriverTraceSpan, startTime) {
    zval *val, rv;

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }

    val = zend_read_property(stackdriver_trace_span_ce, getThis(), "startTime", sizeof("startTime") - 1, 1, &rv);

    RETURN_ZVAL(val, 1, 0);
}

/* {{{ proto Stackdriver\Trace\Span::endTime() }}} */
static PHP_METHOD(StackdriverTraceSpan, endTime) {
    zval *val, rv;

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }

    val = zend_read_property(stackdriver_trace_span_ce, getThis(), "endTime", sizeof("endTime") - 1, 1, &rv);

    RETURN_ZVAL(val, 1, 0);
}

static zend_function_entry stackdriver_trace_span_methods[] = {
    PHP_ME(StackdriverTraceSpan, __construct, StackdriverTraceSpan_construct_arginfo, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(StackdriverTraceSpan, name, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(StackdriverTraceSpan, spanId, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(StackdriverTraceSpan, parentSpanId, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(StackdriverTraceSpan, labels, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(StackdriverTraceSpan, startTime, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(StackdriverTraceSpan, endTime, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

int stackdriver_trace_span_minit(INIT_FUNC_ARGS) {
    zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, "Stackdriver\\Trace\\Span", stackdriver_trace_span_methods);
    stackdriver_trace_span_ce = zend_register_internal_class(&ce);

    zend_declare_property_string(
      stackdriver_trace_span_ce, "name", sizeof("name") - 1, "unknown", ZEND_ACC_PROTECTED TSRMLS_CC
    );
    zend_declare_property_null(stackdriver_trace_span_ce, "spanId", sizeof("spanId") - 1, ZEND_ACC_PROTECTED TSRMLS_CC);
    zend_declare_property_null(stackdriver_trace_span_ce, "parentSpanId", sizeof("parentSpanId") - 1, ZEND_ACC_PROTECTED TSRMLS_CC);
    zend_declare_property_null(stackdriver_trace_span_ce, "startTime", sizeof("startTime") - 1, ZEND_ACC_PROTECTED TSRMLS_CC);
    zend_declare_property_null(stackdriver_trace_span_ce, "endTime", sizeof("endTime") - 1, ZEND_ACC_PROTECTED TSRMLS_CC);
    zend_declare_property_null(stackdriver_trace_span_ce, "labels", sizeof("labels") - 1, ZEND_ACC_PROTECTED TSRMLS_CC);

    return SUCCESS;
}
