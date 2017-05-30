PHP_ARG_ENABLE(stackdriver, whether to enable my extension,
[ --enable-stackdriver  Enable Stackdriver])

if test "$PHP_STACKDRIVER" = "yes"; then
  AC_DEFINE(HAVE_STACKDRIVER, 1, [Whether you have Stackdriver])
  PHP_NEW_EXTENSION(stackdriver, stackdriver.c stackdriver_trace.c stackdriver_trace_span.c stackdriver_trace_context.c stackdriver_debugger.c, $ext_shared)
fi
