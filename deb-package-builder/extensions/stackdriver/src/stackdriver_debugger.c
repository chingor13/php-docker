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
#include "stackdriver_debugger.h"

PHP_FUNCTION(stackdriver_debugger)
{
    zend_string *var_name;
    zval *entry, *orig_var;
    zend_array *symbols = zend_rebuild_symbol_table();

    ZEND_HASH_FOREACH_STR_KEY_VAL(symbols, var_name, entry) {
        orig_var = zend_hash_find(symbols, var_name);
        if (orig_var) {
            php_printf("var name: %s\n", ZSTR_VAL(var_name));

            debug_var(orig_var);
        }
    } ZEND_HASH_FOREACH_END();
}
