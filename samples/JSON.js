var JSON = require('json');

var testobj = {
    bool: false,
    string: "test string",
    number: 1.222,
    object: {
        bool1: true,
        string1: "string1",
        number1: 123.456,
        obj_array: [ 1, 2, 3, 4 ]
    },
    array: ["1", "2", "3", "4"]
}

print("stringify: " + JSON.stringify(testobj));
