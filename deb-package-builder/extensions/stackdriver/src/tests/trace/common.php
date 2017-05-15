<?php

namespace {
    function bar() {
      return 1;
    }

    function foo($x) {
      $sum = 0;
      for ($idx = 0; $idx < $x; $idx++) {
         $sum += bar();
      }
      return $sum;
    }

    class Foo {
        public static function asdf() {
            return 'qwer';
        }

        public function bar() {
            return 1;
        }

        public function context() {
            return stackdriver_trace_context();
        }
    }
}

namespace Illuminate\Database\Eloquent {
    // fake class with method we know is traced
    class Model
    {
        public function delete() {

        }
    }
}
