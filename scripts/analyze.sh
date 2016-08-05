#!/bin/bash

# Static analysis tool to detect modules being used in a script so the compiler
# can include or exclude certain modules if they are/are not used.
# This will output gcc pre-processor defines (-D____) so it can be used in-line
# during the compile step.

if [ $# -lt 1 ]; then
    echo "Usage: ./analyze.sh <script>"
    exit
fi

if [ ! -e $1 ]; then
    echo "Could not find file $1"
    exit
fi

MODULES=''

SCRIPT=$1

gpio=$(grep require\([\"\']gpio[\"\']\) $SCRIPT)
if [ $? -eq 0 ]; then
    >&2 echo Using module: GPIO
    MODULES+=" -DBUILD_MODULE_GPIO"
fi
pwm=$(grep require\([\"\']pwm[\"\']\) $SCRIPT)
if [ $? -eq 0 ]; then
    >&2 echo Using module: PWM
    MODULES+=" -DBUILD_MODULE_PWM"
fi
uart=$(grep require\([\"\']uart[\"\']\) $SCRIPT)
if [ $? -eq 0 ]; then
    >&2 echo Using module: UART
    MODULES+=" -DBUILD_MODULE_UART"
fi
ble=$(grep require\([\"\']ble[\"\']\) $SCRIPT)
if [ $? -eq 0 ]; then
    >&2 echo Using module: BLE
    MODULES+=" -DBUILD_MODULE_BLE"
fi
aio=$(grep require\([\"\']aio[\"\']\) $SCRIPT)
if [ $? -eq 0 ]; then
    >&2 echo Using module: AIO
    MODULES+=" -DBUILD_MODULE_AIO"
fi
a101=$(grep require\([\"\']arduino101_pins[\"\']\) $SCRIPT)
if [ $? -eq 0 ]; then
    >&2 echo Using module: A101 Pins
    MODULES+=" -DBUILD_MODULE_A101"
fi
interval=$(grep setInterval $SCRIPT)
if [ $? -eq 0 ]; then
    MODULES+=" -DBUILD_MODULE_TIMER"
else
    timeout=$(grep setTimeout $SCRIPT)
    if [ $? -eq 0 ]; then
        MODULES+=" -DBUILD_MODULE_TIMER"
    else
        immediate=$(grep setImmediate $SCRIPT)
        if [ $? -eq 0 ]; then
            MODULES+=" -DBUILD_MODULE_TIMER"
        fi
    fi
fi

echo $MODULES
