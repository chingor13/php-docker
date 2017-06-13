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

void debug_ast(zend_ast *ast)
{
    int i, num_children;
    zend_ast_list *list;
    zend_ast_decl *decl;
    zend_ast_zval *azval;

    if (ast == NULL) {
        php_printf("null\n");
        return;
    }
    php_printf("ast kind: %d\n", ast->kind);


    if (ast->kind >> ZEND_AST_IS_LIST_SHIFT == 1) {
        list = (zend_ast_list*)ast;
        php_printf("list type\n");
        num_children = list->children;
        for (i = 0; i < num_children; i++) {
            debug_ast(list->child[i]);
        }
    } else if (ast->kind >> ZEND_AST_SPECIAL_SHIFT == 1) {
        switch(ast->kind) {
            case ZEND_AST_FUNC_DECL:
            case ZEND_AST_CLOSURE:
            case ZEND_AST_METHOD:
            case ZEND_AST_CLASS:
                php_printf("found declaration type\n");
                decl = (zend_ast_decl *)ast;

                for (i = 0; i < 4; i++) {
                    debug_ast(decl->child[i]);
                }

                break;
            case ZEND_AST_ZVAL:
            case ZEND_AST_ZNODE:
                php_printf("found zval type\n");
                azval = (zend_ast_zval *)ast;
                dump_zval(&azval->val);
                break;
            default:
                php_printf("Unknown special type\n");
        }
    } else if (ast->kind == ZEND_AST_CALL) {
        php_printf("zend call!!!!!!!!!!!!!!!!!!!\n");
        num_children = ast->kind >> ZEND_AST_NUM_CHILDREN_SHIFT;
        for(i = 0; i < num_children; i++) {
            debug_ast(ast->child[i]);
        }
    } else {
        num_children = ast->kind >> ZEND_AST_NUM_CHILDREN_SHIFT;
        for(i = 0; i < num_children; i++) {
            debug_ast(ast->child[i]);
        }
    }
}

static zend_ast *stackdriver_debugger_create_debugger_ast(zend_ast *current)
{
    zend_ast *newCall;
    zend_ast_zval *var;
    zend_ast_list *newList, *argList;

    var = emalloc(sizeof(zend_ast_zval));
    var->kind = ZEND_AST_ZVAL;
    ZVAL_STRING(&var->val, "stackdriver_debugger");
    // Z_STRVAL(var->val) = estrdup("stackdriver_debugger");
    // Z_STRLEN(var->val) = strlen("stackdriver_debugger");
    var->val.u2.lineno = current->lineno;
// dump_zval(&var->val);

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
    // return newCall;
}

static int stackdriver_debugger_inject(zend_ast *ast, int lineno)
{
    int i, num_children;
    zend_ast *current, *newAst;
    zend_ast_list *list;
    zend_ast_decl *decl;
    zend_ast_zval *azval;

    // php_printf("ast kind: %d\n", ast->kind);

    if (ast->kind >> ZEND_AST_IS_LIST_SHIFT == 1) {
        // php_printf("is list type\n");
        list = (zend_ast_list*)ast;

        // php_printf("found statement list %d\n", list->children);
        for (i = list->children - 1; i >= 0; i--) {
            current = list->child[i];
            // php_printf("child: %d, line: %d\n", i, current->lineno);
            if (current->lineno <= lineno) {
                // if not yet injected, inject the debugger code
                if (stackdriver_debugger_inject(current, lineno) != SUCCESS) {
                    // php_printf("inject HERE!!!! %p\n", list->child[i]);

                    newAst = stackdriver_debugger_create_debugger_ast(current);
                    // php_printf("debugging.......\n");
                    // debug_ast(newAst);
                    list->child[i] = newAst;
                    // list->child[i] = stackdriver_debugger_create_debugger_ast(current);
                    // debug_ast(list->child[i]);
                        // php_printf("inject HERE!!!! %p\n", list->child[i]);
                }
                return SUCCESS;
            }
        }

        // php_printf("failed to find injection point\n");
        return FAILURE;
    } else if (ast->kind >> ZEND_AST_SPECIAL_SHIFT == 1) {
        switch(ast->kind) {
            case ZEND_AST_FUNC_DECL:
            case ZEND_AST_CLOSURE:
            case ZEND_AST_METHOD:
            case ZEND_AST_CLASS:
                // php_printf("found declaration type\n");
                decl = (zend_ast_decl *)ast;

                return stackdriver_debugger_inject(decl->child[2], lineno);
                break;
            case ZEND_AST_ZVAL:
            case ZEND_AST_ZNODE:
                // php_printf("found zval type\n");
                azval = (zend_ast_zval *)ast;
                break;
            default:
                php_printf("Unknown special type\n");
        }
    } else {
        // number of nodes
        num_children = ast->kind >> ZEND_AST_NUM_CHILDREN_SHIFT;
        // php_printf("Node type: %d with %d children\n", ast->kind, num_children);
        if (num_children == 4) {
            // php_printf("Found for or foreach\n");
            // found for or foreach, the 4th child is the body
            return stackdriver_debugger_inject(ast->child[3], lineno);
        }
    }
    // php_printf("found deepest non-list type\n");
    return FAILURE;
}

static void stackdriver_debugger_ast_process(zend_ast *ast)
{
    char *filename = "/usr/local/google/home/chingor/php/php-docker/deb-package-builder/extensions/stackdriver/src/tests/debugger/common.php";
    int lineno = 16;
    int i;

    if (strcmp(ZSTR_VAL(CG(compiled_filename)), filename) == 0) {
        // php_printf("should debug file.\n");
        if (stackdriver_debugger_inject(ast, lineno) == SUCCESS) {
            // php_printf("injected!!!\n");
        } else {
            // php_printf("failed!!!\n");
        }
        // php_printf("debugging........................................\n");
// debug_ast(ast);
        // php_printf("found injection point type: %d\n", injection_point->kind);
    }
    // php_printf("compiled source file: %s\n", ZSTR_VAL(CG(compiled_filename)));
}

PHP_FUNCTION(stackdriver_debugger)
{
  php_printf("in debugger\n");

    // zend_string *var_name;
    // zval *entry, *orig_var;
    // zend_array *symbols = zend_rebuild_symbol_table();
    //
    // ZEND_HASH_FOREACH_STR_KEY_VAL(symbols, var_name, entry) {
    //     orig_var = zend_hash_find(symbols, var_name);
    //     if (orig_var) {
    //         php_printf("var name: %s\n", ZSTR_VAL(var_name));
    //
    //         dump_zval(orig_var);
    //     }
    // } ZEND_HASH_FOREACH_END();
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

    // php_printf("current file: %s:%d\n", ZSTR_VAL(EX(prev_execute_data)->func->op_array.filename), EX(prev_execute_data)->opline->lineno);

    full_filename = stackdriver_debugger_full_filename(filename, EX(prev_execute_data)->func->op_array.filename);
    // php_printf("should set breakpoint at %s:%d\n", ZSTR_VAL(full_filename), lineno);

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
