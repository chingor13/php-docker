<?php

function bar() {
  return 1;
}

function foo($x) {
  $sum = 0;
  for ($idx = 0; $idx < 2; $idx++) {
     $sum += bar();
  }
  return strlen("hello: {$x}");
}

class Foo {
    public function bar() {
        return 1;
    }
}
