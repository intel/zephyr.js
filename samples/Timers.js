var count = 0;

var i = setInterval(function(a, b) {
    print("Interval #" + count + ' arg1: ' + a + ' arg2: ' + b);
    count++;
}, 1000, 1, 2);

setTimeout(function(a) {
    print("Timeout, clearing interval");
    clearInterval(a);
}, 5000, i);
