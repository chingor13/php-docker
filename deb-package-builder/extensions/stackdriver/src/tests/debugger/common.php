<?php

$foo = 'bar';
$asdf = ['qwer', 5, 20.1];

function foo()
{
    $x = 1;
    return 'bar';
}

function loop($times)
{
    $sum = 0;
    for ($i = 0; $i < $times; $i++) {
        $sum += $i;
    }
    return $sum;
}
