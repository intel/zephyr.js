// Copyright (c) 2017-2018, Intel Corporation.

console.log("Test board APIs");

var board = require("board");
var assert = require("Assert.js");

assert(typeof board.name === "string",
       "board: the type of name is 'string'");

var name = board.name;
board.name = name + "board";
assert(name === board.name, "board: the name property is read-only");

assert(board.version === "0.1", "board: version value is '0.1'");
assert(typeof board.version === "string",
       "board: the type of version is 'string'");

var version = board.version;
board.version = version + "2345";
assert(version === board.version, "board: the version property is read-only");

assert.result();
