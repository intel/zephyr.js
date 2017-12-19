// Copyright (c) 2017, Intel Corporation.

console.log("Test board API by manual");

var board = require("board");

var expectedName = [
    ["        Arduino 101", "arduino_101"],
    ["          FRDM-K64F", "frdm_k64f"],
    ["     nRF52-PCA10040", "nrf52_pca10040"],
    ["        Arduino Due", "arduino_due"],
    ["      Nucleo F411RE", "nucleo_f411re"],
    ["              Linux", "linux (partially simulating arduino_101)"],
    ["         96B_CARBON", "96b_carbon"],
    ["  OLIMEX_STM32_E407", "olimex_stm32_e407"],
    ["      STM32F4_DISCO", "stm32f4_disco"],
    ["             Others", "unknown"]
]

console.log("\n\rBoard name: " + board.name);
console.log("expected result:");
for (var i = 0; i < expectedName.length; i++) {
    console.log("    " + expectedName[i][0] + " : " + expectedName[i][1]);
}
