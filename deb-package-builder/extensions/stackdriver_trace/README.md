# Stackdriver PHP Extension

This extension enables the following services:

* [Stackdriver Trace](https://cloud.google.com/trace/)

## Stackdriver Trace

Stackdriver Trace is a free, open-source distributed tracing implementation
based on the [Dapper Paper](https://research.google.com/pubs/pub36356.html).
This extension allows you to "watch" class methd and function calls in order to
automatically collect nested spans (labelled timing data).

This library can work in conjunction with the PHP library
[google/cloud-trace](https://packagist.org/packages/google/cloud-trace) in order
to send collected span data to a backend storage server.

This extension also maintains the current trace span context - the current span
the code is currently executing within. Whenever a span is created, it's parent
is set the the current span, and this new span becomes the current trace span
context.

### Trace a class method

Whenever the class' method is called, create a trace span.

```php
stackdriver_trace_method(Foobar::class, '__construct');
```

You can specify the span data to use:

```php
stackdriver_trace_method(Foobar::class, '__construct', [
  'name' => 'Foobar::__construct',
  'labels' => [
    'foo' => 'bar'
  ]
]);
```

You can also provide a Closure as a callback in order specify the span data used
for the created span.

```php
stackdriver_trace_method(Foobar::class, '__construct', function () {
  return [
    'name' => 'Foobar::__construct',
    'labels' => [
      'foo' => 'bar'
    ]
  ];
});
```

### Trace a function

```php
stackdriver_trace_function('my_function');
```

Whenever the function is called, create a trace span.

```php
stackdriver_trace_function('var_dump');
```

You can specify the span data to use:

```php
stackdriver_trace_function('var_dump', [
  'name' => 'var_dump',
  'labels' => [
    'foo' => 'bar'
  ]
]);
```

You can also provide a Closure as a callback in order specify the span data used
for the created span.

```php
stackdriver_trace_function('var_dump', function () {
  return [
    'name' => 'Foobar::__construct',
    'labels' => [
      'foo' => 'bar'
    ]
  ];
});
```

### List spans

Retrieve an array of collected spans.

```php
$spans = stackdriver_trace_list_spans();
var_dump($spans);
```

### Get current trace context

```php
$context = stackdriver_trace_context();
var_dump($context);
```

### Start a span

```php
stackdriver_trace_start_span($spanOptions);
```

### Finish a span

```php
stackdriver_trace_finish_span($spanOptions);
```
