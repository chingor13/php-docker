--TEST--
Stackdriver Trace: Trace Context
--FILE--
<?php

require_once(__DIR__ . '/common.php');

// 1: Sanity test a simple profile run
stackdriver_trace_method("Foo", "context");
stackdriver_trace_set_context("traceid", 1234);

$f = new Foo();
$context = $f->context();
$traces = stackdriver_trace_list();
echo "Number of traces: " . count($traces) . "\n";
$span = $traces[0];

$test = $span->spanId() == $context->spanId();
echo "Span id matches context's span id: $test\n";

echo "Span parent id: {$span->parentSpanId()}\n";
echo "Context trace id: {$context->traceId()}\n";
?>
--EXPECT--
Number of traces: 1
Span id matches context's span id: 1
Span parent id: 1234
Context trace id: traceid
