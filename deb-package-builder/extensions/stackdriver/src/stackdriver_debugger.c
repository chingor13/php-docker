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
#include "ext/standard/php_string.h"
#include "Zend/zend_virtual_cwd.h"

// True global for storing the original zend_ast_process
static void (*original_zend_ast_process)(zend_ast*);

/**
 * This method generates a new abstract syntax tree that injects a function call to
 * `stackdriver_debugger()`.
 *
 * TODO: store variables
 * TODO: store call stack
 * TODO: deregister
 * TODO: add conditional triggering of debugger function
 * TODO: catch exceptions
 *
 * Format:
 *
 *   ZEND_AST_STMT_LIST
 *   - ZEND_AST_CALL
 *     - ZEND_AST_ZVAL (string, "stackdriver_debugger")
 *     - ZEND_AST_ARG_LIST (empty list)
 *   - original zend_ast node
 *
 * Note: we are emalloc-ing memory here, but it is expected that the PHP internals recursively
 * walk the syntax tree and free allocated memory. This method cannot leave dangling pointers or
 * the allocated memory may never be freed.
 */
static zend_ast *stackdriver_debugger_create_debugger_ast(zend_ast *current)
{
    zend_ast *newCall;
    zend_ast_zval *var;
    zend_ast_list *newList, *argList;

    var = emalloc(sizeof(zend_ast_zval));
    var->kind = ZEND_AST_ZVAL;
    ZVAL_STRING(&var->val, "stackdriver_debugger");
    var->val.u2.lineno = current->lineno;

    argList = emalloc(sizeof(zend_ast_list));
    argList->kind = ZEND_AST_ARG_LIST;
    argList->lineno = current->lineno;
    argList->children = 0;

    newCall = emalloc(sizeof(zend_ast) + sizeof(zend_ast*));
    newCall->kind = ZEND_AST_CALL;
    newCall->lineno = current->lineno;
    newCall->child[0] = (zend_ast*)var;
    newCall->child[1] = (zend_ast*)argList;

    // create a new statement list
    newList = emalloc(sizeof(zend_ast_list) + sizeof(zend_ast*));
    newList->kind = ZEND_AST_STMT_LIST;
    newList->lineno = current->lineno;
    newList->children = 2;
    newList->child[0] = newCall;
    newList->child[1] = current;

    return (zend_ast *)newList;
}

/**
 * This method walks through the AST looking for the last non-list statement to replace
 * with a new AST node that first calls the `stackdriver_debugger()` function, then
 * calls the original statement.
 *
 * This function returns SUCCESS if we have already injected into the syntax tree.
 * Otherwise, the function returns FAILURE.
 */
static int stackdriver_debugger_inject(zend_ast *ast, int lineno)
{
    int i, num_children;
    zend_ast *current;
    zend_ast_list *list;
    zend_ast_decl *decl;
    zend_ast_zval *azval;

    if (ast->kind >> ZEND_AST_IS_LIST_SHIFT == 1) {
        list = (zend_ast_list*)ast;

        for (i = list->children - 1; i >= 0; i--) {
            current = list->child[i];
            if (current->lineno <= lineno) {
                // if not yet injected, inject the debugger code
                if (stackdriver_debugger_inject(current, lineno) != SUCCESS) {
                    list->child[i] = stackdriver_debugger_create_debugger_ast(current);
                }
                return SUCCESS;
            }
        }
        return FAILURE;
    } else if (ast->kind >> ZEND_AST_SPECIAL_SHIFT == 1) {
        switch(ast->kind) {
            case ZEND_AST_FUNC_DECL:
            case ZEND_AST_CLOSURE:
            case ZEND_AST_METHOD:
            case ZEND_AST_CLASS:
                decl = (zend_ast_decl *)ast;
                return stackdriver_debugger_inject(decl->child[2], lineno);
            case ZEND_AST_ZVAL:
            case ZEND_AST_ZNODE:
                azval = (zend_ast_zval *)ast;
                break;
            default:
                php_printf("Unknown special type\n");
        }
    } else {
        // number of nodes
        num_children = ast->kind >> ZEND_AST_NUM_CHILDREN_SHIFT;
        if (num_children == 4) {
            // found for or foreach, the 4th child is the body
            return stackdriver_debugger_inject(ast->child[3], lineno);
        }
    }
    return FAILURE;
}

/**
 * This function replaces the original `zend_ast_process` function. If one was previously provided, call
 * that one after this one.
 */
static void stackdriver_debugger_ast_process(zend_ast *ast)
{
    HashTable *ht;
    stackdriver_debugger_snapshot_t *snapshot;

    zval *debugger_snapshots = zend_hash_find(STACKDRIVER_G(debugger_snapshots), CG(compiled_filename));
    if (debugger_snapshots != NULL) {
        php_printf("found debugger snapshots! %s\n", ZSTR_VAL(CG(compiled_filename)));
        ht = Z_ARR_P(debugger_snapshots);

        ZEND_HASH_FOREACH_PTR(ht, snapshot) {
            // stackdriver_debugger_inject(ast, snapshot);
            php_printf("snapshot %p\n", snapshot);
            dump_zval(_z);
            if (snapshot->id) {
                php_printf("snapshot id ptr: %p\n", snapshot->id);
                php_printf("snapshot id: %s\n", ZSTR_VAL(snapshot->id));
            }
            php_printf("snapshot line: %d\n", snapshot->lineno);
            // php_printf("snapshot id: %s, line: %d\n", snapshot->id, snapshot->lineno);
        } ZEND_HASH_FOREACH_END();

    }
    char *filename = "/usr/local/google/home/chingor/php/php-docker/deb-package-builder/extensions/stackdriver/src/tests/debugger/common.php";
    int lineno = 16;
    int i;

    if (strcmp(ZSTR_VAL(CG(compiled_filename)), filename) == 0) {
        if (stackdriver_debugger_inject(ast, lineno) == SUCCESS) {
            // php_printf("injected!!!\n");
        } else {
            // php_printf("failed!!!\n");
        }
    }

    // call the original zend_ast_process function if one was set
    if (original_zend_ast_process) {
        original_zend_ast_process(ast);
    }
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

            // dump_zval(orig_var);
        }
    } ZEND_HASH_FOREACH_END();
}

zend_string *stackdriver_debugger_full_filename(zend_string *relative_or_full_path, zend_string *current_file)
{
    zend_string *basename = php_basename(ZSTR_VAL(current_file), ZSTR_LEN(current_file), NULL, 0);
    size_t dirlen = php_dirname(ZSTR_VAL(current_file), ZSTR_LEN(current_file));
    zend_string *dirname = zend_string_init(ZSTR_VAL(current_file), dirlen, 0);
    php_printf("basename: %s\n", ZSTR_VAL(basename));
    php_printf("dirname: %s\n", ZSTR_VAL(dirname));

    zend_string *fullname = strpprintf(ZSTR_LEN(dirname) + 2 + ZSTR_LEN(current_file), "%s%c%s", ZSTR_VAL(dirname), DEFAULT_SLASH, ZSTR_VAL(relative_or_full_path));
    zend_string_release(basename);
    zend_string_release(dirname);

    php_printf("Calculate full filename: %s, current: %s\n", ZSTR_VAL(relative_or_full_path), ZSTR_VAL(current_file));
    php_printf("fullname: %s\n", ZSTR_VAL(fullname));

    return fullname;
}

PHP_FUNCTION(stackdriver_debugger_add_snapshot)
{
    zend_string *filename, *full_filename, *snapshot_id = NULL, *condition = NULL;
    zend_long lineno;
    zval *snapshots, *snapshot_ptr;
    stackdriver_debugger_snapshot_t *snapshot;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Sl|SS", &filename, &lineno, &snapshot_id, &condition) == FAILURE) {
        RETURN_FALSE;
    }

    full_filename = stackdriver_debugger_full_filename(filename, EX(prev_execute_data)->func->op_array.filename);

    // TODO: clean this up in rshutdown
    PHP_STACKDRIVER_MAKE_STD_ZVAL(snapshot_ptr);
    snapshot = emalloc(sizeof(stackdriver_debugger_snapshot_t));

    if (snapshot_id == NULL) {
        snapshot->id = strpprintf(20, "%d", php_mt_rand());
        php_printf("generating snapshot id\n");
    } else {
        php_printf("provided snapshot id\n");
        snapshot->id = snapshot_id;
    }
    snapshot->filename = full_filename;
    snapshot->lineno = lineno;
    if (condition != NULL) {
        snapshot->condition = condition;
    }

    ZVAL_PTR(snapshot_ptr, snapshot);

    snapshots = zend_hash_find(STACKDRIVER_G(debugger_snapshots), full_filename);
    if (snapshots == NULL) {
        // initialize snapshots as array
        // TODO: clean this up in rshutdown
        PHP_STACKDRIVER_MAKE_STD_ZVAL(snapshots);
        array_init(snapshots);
    }

    add_next_index_zval(snapshots, snapshot_ptr);

    zend_hash_update(STACKDRIVER_G(debugger_snapshots), full_filename, snapshots);
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
