# Stackdriver PHP Extension

This extension enables the following services:

* [Stackdriver Trace](https://cloud.google.com/trace/)

## Stackdriver Trace

### Trace a class method

```php
stackdriver_trace_method(Foobar::class, '__construct');
```

### Trace a function

```php
stackdriver_trace_function('my_function');
```

### List spans

```php
stackdriver_trace_list_spans();
```

### Get current trace context

```php
stackdriver_trace_context();
```

### Start a span

```php
stackdriver_trace_start_span($spanOptions);
```

### Finish a span

```php
stackdriver_trace_finish_span($spanOptions);
```
