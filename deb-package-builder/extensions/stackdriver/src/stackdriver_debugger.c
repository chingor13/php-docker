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
#include "zend_language_scanner.h"
#include "zend_language_parser.h"
#include "debug.h"

// True global for storing the original zend_ast_process
static void (*original_zend_ast_process)(zend_ast*);

static void stackdriver_debugger_ast_process(zend_ast *ast)
{
    if (strcmp(ZSTR_VAL(CG(compiled_filename)), "/usr/local/google/home/chingor/php/php-docker/deb-package-builder/extensions/stackdriver/src/tests/debugger/common.php") == 0) {
        php_printf("should debug file.\n");
    }
    // php_printf("compiled source file: %s\n", ZSTR_VAL(CG(compiled_filename)));
}

PHP_FUNCTION(stackdriver_debugger)
{
    zend_string *var_name;
    zval *entry, *orig_var;
    zend_array *symbols = zend_rebuild_symbol_table();

    ZEND_HASH_FOREACH_STR_KEY_VAL(symbols, var_name, entry) {
        orig_var = zend_hash_find(symbols, var_name);
        if (orig_var) {
            php_printf("var name: %s\n", ZSTR_VAL(var_name));

            dump_zval(orig_var);
        }
    } ZEND_HASH_FOREACH_END();
}

zend_string *stackdriver_debugger_full_filename(zend_string *relative_or_full_path, zend_string *current_file)
{
    // TODO: fix the relative path logic
    return relative_or_full_path;
}

PHP_FUNCTION(stackdriver_debugger_add_snapshot)
{
    zend_string *filename, *full_filename, *condition;
    zend_long lineno;
    zval *snapshot_ptr;
    stackdriver_debugger_snapshot_t *snapshot;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Sl|S", &filename, &lineno, &condition) == FAILURE) {
        RETURN_FALSE;
    }

    php_printf("current file: %s:%d\n", ZSTR_VAL(EX(prev_execute_data)->func->op_array.filename), EX(prev_execute_data)->opline->lineno);

    full_filename = stackdriver_debugger_full_filename(filename, EX(prev_execute_data)->func->op_array.filename);
    php_printf("should set breakpoint at %s:%d\n", ZSTR_VAL(full_filename), lineno);

    PHP_STACKDRIVER_MAKE_STD_ZVAL(snapshot_ptr);
    snapshot = emalloc(sizeof(stackdriver_debugger_snapshot_t));
    snapshot->filename = filename;
    snapshot->lineno = lineno;
    if (condition != NULL) {
        snapshot->condition = condition;
    }

    ZVAL_PTR(snapshot_ptr, snapshot);

    zend_hash_update(STACKDRIVER_G(debugger_snapshots), filename, snapshot_ptr);
}

int stackdriver_debugger_minit(INIT_FUNC_ARGS)
{
    // Save original zend_ast_process function and use our own to modify the ast
    original_zend_ast_process = zend_ast_process;
    zend_ast_process = stackdriver_debugger_ast_process;

    return SUCCESS;
}

int stackdriver_debugger_rinit(TSRMLS_D)
{
    ALLOC_HASHTABLE(STACKDRIVER_G(debugger_snapshots));
    zend_hash_init(STACKDRIVER_G(debugger_snapshots), 16, NULL, ZVAL_PTR_DTOR, 0);

    return SUCCESS;
}

int stackdriver_debugger_rshutdown(TSRMLS_D)
{
    return SUCCESS;
}
