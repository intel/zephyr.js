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

echo "# Modules found in $1:" > prj.conf.tmp

function check_for_require()
{
    # effects: checks for a line with requires("gpio"), with single or double
    #            quotes, and optional whitespace before or after parentheses
    #          returns success (0) if found
    rval=$(grep "require *( *['\"]$1['\"] *)" $SCRIPT)
    return $?
}

check_for_require events
if [ $? -eq 0 ]; then
    >&2 echo Using module: EVENTS
    MODULES+=" -DBUILD_MODULE_EVENTS"
fi
check_for_require gpio
if [ $? -eq 0 ]; then
    >&2 echo Using module: GPIO
    MODULES+=" -DBUILD_MODULE_GPIO"
    echo "CONFIG_GPIO=y" >> prj.conf.tmp
fi
check_for_require pwm
if [ $? -eq 0 ]; then
    >&2 echo Using module: PWM
    MODULES+=" -DBUILD_MODULE_PWM"
    echo "CONFIG_PWM=y" >> prj.conf.tmp
    echo "CONFIG_PWM_QMSI_NUM_PORTS=4" >> prj.conf.tmp
fi
check_for_require uart
if [ $? -eq 0 ]; then
    >&2 echo Using module: UART
    MODULES+=" -DBUILD_MODULE_UART"
fi
check_for_require ble
if [ $? -eq 0 ]; then
    >&2 echo Using module: BLE
    MODULES+=" -DBUILD_MODULE_BLE"
    echo "CONFIG_BLUETOOTH=y" >> prj.conf.tmp
    echo "CONFIG_BLUETOOTH_LE=y" >> prj.conf.tmp
    echo "CONFIG_BLUETOOTH_SMP=y" >> prj.conf.tmp
    echo "CONFIG_BLUETOOTH_PERIPHERAL=y" >> prj.conf.tmp
    echo "CONFIG_BLUETOOTH_GATT_DYNAMIC_DB=y" >> prj.conf.tmp
fi
check_for_require aio
if [ $? -eq 0 ]; then
    >&2 echo Using module: AIO
    MODULES+=" -DBUILD_MODULE_AIO"
fi
check_for_require i2c
if [ $? -eq 0 ]; then
    >&2 echo Using module: I2C
    MODULES+=" -DBUILD_MODULE_I2C"
    >&2 echo Using module: Buffer
    MODULES+=" -DBUILD_MODULE_BUFFER"
fi
check_for_require grove_lcd
if [ $? -eq 0 ]; then
    >&2 echo Using module: GROVE_LCD
    MODULES+=" -DBUILD_MODULE_GROVE_LCD"
fi
check_for_require arduino101_pins
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
buffer=$(grep Buffer\([0-9]*\) $SCRIPT)
if [ $? -eq 0 ] && [[ $MODULE != *"BUILD_MODULE_BUFFER"* ]]; then
    >&2 echo Using module: Buffer
    MODULES+=" -DBUILD_MODULE_BUFFER"
fi

console=$(grep console $SCRIPT)
if [ $? -eq 0 ] && [[ $MODULE != *"BUILD_MODULE_CONSOLE"* ]]; then
    >&2 echo Using module: Console
    MODULES+=" -DBUILD_MODULE_CONSOLE"
fi

echo $MODULES
