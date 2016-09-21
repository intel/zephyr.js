// Copyright (c) 2016, Intel Corporation.

// Test to see whether eval is working; it shouldn't be, as it is a security
// risk

print("Test of the eval() function");

try {
    eval("print('Oh no, eval is working!')");
}
catch (err) {
    print("Hooray, trying to use eval failed!");
}
