#!/bin/bash
cd src

echo "generate script.h from blink.js"
jsrunner -f blink.js

cd ..

echo "Build..."
make flash
