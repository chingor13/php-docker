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
