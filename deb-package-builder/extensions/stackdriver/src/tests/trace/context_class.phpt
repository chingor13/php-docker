--TEST--
Stackdriver Trace: Context Class Test
--FILE--
<?php

require_once(__DIR__ . '/common.php');

$context = new Stackdriver\Trace\Context([
    'spanId' => 1234,
    'traceId' => 'foo'
]);

echo "Span id: {$context->spanId()}\n";

echo "Trace id: {$context->traceId()}\n";;

?>
--EXPECT--
Span id: 1234
Trace id: foo
