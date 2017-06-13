<?php

// $foo = 'bar';
// $asdf = ['qwer', 5, 20.1];
//
function foo($val) {
    $x = 1;
    stackdriver_debugger();
    return $val;
}

function loop($times)
{
    $sum = 0;
    for ($i = 0; $i < $times; $i++) {
        $sum += $i;//foo($i);
    }
    return $sum;
}

loop(3);
// foo(3);
// stackdriver_debugger();

// echo 'foo' . PHP_EOL;
//
// class FooBar {
//     function asdf() {
//         return 'asdf';
//     }
// }
