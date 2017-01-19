// Copyright (c) 2016-2017, Intel Corporation.

console.log("TESTING LOG...");
console.log(1);
console.log(-1);
console.log(123.456);
console.log(-123.456);
console.log({int_val: 1});

console.error("\nTESTING ERROR...");
console.error(1);
console.error(123.456);
console.error(-123.456);
console.error({int_val: 1});

console.log("\nTESTING TIME...");
console.time('timer-1');
setTimeout(function() {
    console.timeEnd('timer-1');
}, 2000);
