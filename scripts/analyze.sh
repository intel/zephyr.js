#!/bin/bash

# Static analysis tool to detect modules being used in a script so the compiler
# can include or exclude certain modules if they are/are not used.
# This will output gcc pre-processor defines (-D____) so it can be used in-line
# during the compile step.

if [ $# -lt 2 ]; then
    echo "Usage: ./analyze.sh <board> <script>"
    exit
fi

if [ ! -e $2 ]; then
    echo "Could not find file $2"
    exit
fi

if [ -f "/tmp/zjs.js" ]; then
    rm /tmp/zjs.js
fi


MODULES=''
BOARD=$1
SCRIPT=$2
CONFIG=$3

echo "# Modules found in $SCRIPT:" > prj.conf.tmp
echo "# Modules found in $SCRIPT:" > arc/prj.conf.tmp
tmpstr="# ZJS flags set by analyze.js"

if [ -n "$CONFIG" ]; then
    tmpstr+=" and $CONFIG"
fi

echo $tmpstr > zjs.conf.tmp

function check_for_js_require()
{

    js_files=$(grep "require *( *['\"].*\.js['\"] *)" $SCRIPT | grep -o "['\"].*\.js['\"]" | tr -d \'\")

    for file in $js_files
    do
         >&2 echo "Javascript module included : $file"
         # Add the module JS to the temporary JS file
         cat "$ZJS_BASE/modules/$file" >> /tmp/zjs.js
    done

    # Add the primary JS file to the temporary JS file
    cat "$SCRIPT" >> /tmp/zjs.js
    SCRIPT=/tmp/zjs.js
}

function check_for_require()
{
    # effects: checks for a line with requires("gpio"), with single or double
    #            quotes, and optional whitespace before or after parentheses
    #          returns success (0) if found
    rval=$(grep "require *( *['\"]$1['\"] *)" $SCRIPT)
    return $?
}

function check_config_file()
{
    # effects: checks $1 for uncommented module to include from #CONFIG
    #          If found, it will be put in zjs.conf.tmp and inserted into src/Makefile

    if [ -n "$CONFIG" ]; then
        grep -v '^#' < $CONFIG | grep $1='y\|m' >> /dev/null
        return $?
    fi
    return 1;
}

# Check if buffer module required in the JS or config file
grep Buffer\([0-9]*\) $SCRIPT > /dev/null
if [ $? -eq 0 ] || check_config_file ZJS_BUFFER; then
    buffer=true;
else
    buffer=false;
fi

# Check for javascript modules
# Note this has to be done first in order to catch the reqirements
# of the imported JS files
check_for_js_require

# Check for native modules
if check_for_require events || check_config_file ZJS_EVENTS; then
    >&2 echo Using module: Events
    MODULES+=" -DBUILD_MODULE_EVENTS"
    echo "export ZJS_EVENTS=y" >> zjs.conf.tmp
fi

if check_for_require ocf || check_config_file ZJS_OCF; then
    >&2 echo Using module: OCF
    MODULES+=" -DBUILD_MODULE_OCF"
    echo "CONFIG_NETWORKING_WITH_IPV6=y" >> prj.conf.tmp
    echo "CONFIG_NET_TESTING=y" >> prj.conf.tmp
    echo "CONFIG_NETWORKING_IPV6_NO_ND=y" >> prj.conf.tmp
    echo "CONFIG_NETWORKING=y" >> prj.conf.tmp
    echo "CONFIG_NETWORKING_WITH_LOGGING=y" >> prj.conf.tmp
    echo "CONFIG_NETWORKING_WITH_LOOPBACK=y" >> prj.conf.tmp
    echo "CONFIG_NETWORKING_UART=y" >> prj.conf.tmp
    echo "CONFIG_NETWORKING_DEBUG_UART=y" >> prj.conf.tmp
    echo "CONFIG_NANO_TIMEOUTS=y" >> prj.conf.tmp
    echo "CONFIG_UDP_MAX_CONNECTIONS=30" >> prj.conf.tmp
    echo "CONFIG_IP_BUF_RX_SIZE=5" >> prj.conf.tmp
    echo "CONFIG_IP_BUF_TX_SIZE=5" >> prj.conf.tmp
    echo "CONFIG_NET_MAX_CONTEXTS=9" >> prj.conf.tmp
    echo "export ZJS_OCF=y" >> zjs.conf.tmp
fi

if check_for_require gpio || check_config_file ZJS_GPIO; then
    MODULES+=" -DBUILD_MODULE_GPIO"
    echo "CONFIG_GPIO=y" >> prj.conf.tmp
    echo "export ZJS_GPIO=y" >> zjs.conf.tmp
fi

if check_for_require performance || check_config_file ZJS_PERFORMANCE; then
    >&2 echo Using module: Performance
    MODULES+=" -DBUILD_MODULE_PERFORMANCE"
    echo "export ZJS_PERFORMANCE=y" >> zjs.conf.tmp
fi

if check_for_require pwm || check_config_file ZJS_PWM; then
    >&2 echo Using module: PWM
    MODULES+=" -DBUILD_MODULE_PWM"
    echo "CONFIG_PWM=y" >> prj.conf.tmp
    echo "CONFIG_PWM_QMSI_NUM_PORTS=4" >> prj.conf.tmp
    echo "export ZJS_PWM=y" >> zjs.conf.tmp
fi

if check_for_require uart || check_config_file ZJS_UART; then
    >&2 echo Using module: UART
    if [ $BOARD = "arduino_101" ]; then
        echo "CONFIG_GPIO=y" >> prj.conf.tmp
        echo "CONFIG_USB=y" >> prj.conf.tmp
        echo "CONFIG_USB_DW=y" >> prj.conf.tmp
        echo "CONFIG_USB_DEVICE_STACK=y" >> prj.conf.tmp
        echo "CONFIG_SYS_LOG_USB_DW_LEVEL=0" >> prj.conf.tmp
        echo "CONFIG_USB_CDC_ACM=y" >> prj.conf.tmp
        echo "CONFIG_SYS_LOG_USB_LEVEL=0" >> prj.conf.tmp
        echo "CONFIG_SERIAL=y" >> prj.conf.tmp
        echo "CONFIG_UART_LINE_CTRL=y" >> prj.conf.tmp
    fi
    echo "CONFIG_UART_INTERRUPT_DRIVEN=y" >> prj.conf.tmp
    MODULES+=" -DBUILD_MODULE_UART"
    MODULES+=" -DBUILD_MODULE_EVENTS"
    echo "export ZJS_EVENTS=y" >> zjs.conf.tmp
    echo "export ZJS_UART=y" >> zjs.conf.tmp
    buffer=true;
fi

if check_for_require ble || check_config_file ZJS_BLE; then
    >&2 echo Using module: BLE
    MODULES+=" -DBUILD_MODULE_BLE"
    echo "CONFIG_BLUETOOTH=y" >> prj.conf.tmp
    echo "CONFIG_BLUETOOTH_STACK_NBLE=y" >> prj.conf.tmp
    echo "CONFIG_NBLE=y" >> prj.conf.tmp
    echo "CONFIG_BLUETOOTH_SMP=y" >> prj.conf.tmp
    echo "CONFIG_BLUETOOTH_PERIPHERAL=y" >> prj.conf.tmp
    echo "CONFIG_BLUETOOTH_GATT_DYNAMIC_DB=y" >> prj.conf.tmp
    MODULES+=" -DBUILD_MODULE_EVENTS"
    echo "export ZJS_EVENTS=y" >> zjs.conf.tmp
    echo "export ZJS_BLE=y" >> zjs.conf.tmp
    buffer=true;
fi

if check_for_require aio || check_config_file ZJS_AIO; then
    >&2 echo Using module: AIO
    MODULES+=" -DBUILD_MODULE_AIO"
    echo "CONFIG_ADC=y" >> arc/prj.conf.tmp
    echo "export ZJS_AIO=y" >> zjs.conf.tmp
fi

if check_for_require i2c || check_config_file ZJS_I2C; then
    >&2 echo Using module: I2C
    MODULES+=" -DBUILD_MODULE_I2C"
    if [ $BOARD = "arduino_101" ]; then
        echo "CONFIG_I2C=y" >> arc/prj.conf.tmp
    else
        echo "CONFIG_I2C=y" >> prj.conf.tmp
    fi
    echo "export ZJS_I2C=y" >> zjs.conf.tmp
    buffer=true;
fi

if check_for_require grove_lcd || check_config_file ZJS_GROVE_LCD; then
    >&2 echo Using module: Grove LCD
    MODULES+=" -DBUILD_MODULE_GROVE_LCD"
    if [ $BOARD = "arduino_101" ]; then
        echo "CONFIG_I2C=y" >> arc/prj.conf.tmp
        echo "CONFIG_GROVE=y" >> arc/prj.conf.tmp
        echo "CONFIG_GROVE_LCD_RGB=y" >> arc/prj.conf.tmp
        echo "CONFIG_GROVE_LCD_RGB_INIT_PRIORITY=90" >> arc/prj.conf.tmp
    else
        echo "CONFIG_I2C=y" >> prj.conf.tmp
        echo "CONFIG_GROVE=y" >> prj.conf.tmp
        echo "CONFIG_GROVE_LCD_RGB=y" >> prj.conf.tmp
        echo "CONFIG_GROVE_LCD_RGB_INIT_PRIORITY=90" >> prj.conf.tmp
    fi
    echo "export ZJS_GROVE_LCD=y" >> zjs.conf.tmp
fi

if check_for_require arduino101_pins || check_config_file ZJS_ARDUINO101_PINS; then
    >&2 echo Using module: A101 Pins
    MODULES+=" -DBUILD_MODULE_A101"
    echo "export ZJS_ARDUINO101_PINS=y" >> zjs.conf.tmp
fi

interval=$(grep "setInterval\|setTimeout\|setImmediate" $SCRIPT)
if [ $? -eq 0 ] || check_config_file ZJS_TIMERS; then
    MODULES+=" -DBUILD_MODULE_TIMER"
    echo "export ZJS_TIMERS=y" >> zjs.conf.tmp
fi

if $buffer && [[ $MODULE != *"BUILD_MODULE_BUFFER"* ]]; then
    >&2 echo Using module: Buffer
    MODULES+=" -DBUILD_MODULE_BUFFER"
    echo "export ZJS_BUFFER=y" >> zjs.conf.tmp
fi
sensor=$(grep -E Accelerometer\|Gyroscope\|AmbientLightSensor $SCRIPT)
if [ $? -eq 0 ] || check_config_file ZJS_SENSOR; then
    >&2 echo Using module: Sensor
    MODULES+=" -DBUILD_MODULE_SENSOR"
    echo "export ZJS_SENSOR=y" >> zjs.conf.tmp
    if [ $BOARD = "arduino_101" ]; then
        bmi160=$(grep -E Accelerometer\|Gyroscope $SCRIPT)
        if [ $? -eq 0 ]; then
            MODULES+=" -DBUILD_MODULE_SENSOR_TRIGGER"
            echo "CONFIG_SENSOR=y" >> arc/prj.conf.tmp
            echo "CONFIG_GPIO=y" >> prj.conf.tmp
            echo "CONFIG_GPIO_QMSI=y" >> prj.conf.tmp
            echo "CONFIG_GPIO_QMSI_0_PRI=2" >> prj.conf.tmp
            echo "CONFIG_GPIO_QMSI_1=y" >> prj.conf.tmp
            echo "CONFIG_GPIO_QMSI_1_NAME=\"GPIO_1\"" >> prj.conf.tmp
            echo "CONFIG_GPIO_QMSI_1_PRI=2" >> prj.conf.tmp
            echo "CONFIG_SPI=y" >> arc/prj.conf.tmp
            echo "CONFIG_BMI160=y" >> arc/prj.conf.tmp
            echo "CONFIG_BMI160_INIT_PRIORITY=80" >> arc/prj.conf.tmp
            echo "CONFIG_BMI160_NAME=\"bmi160\"" >> arc/prj.conf.tmp
            echo "CONFIG_BMI160_SPI_PORT_NAME=\"SPI_1\"" >> arc/prj.conf.tmp
            echo "CONFIG_BMI160_SLAVE=1" >> arc/prj.conf.tmp
            echo "CONFIG_BMI160_SPI_BUS_FREQ=88" >> arc/prj.conf.tmp
            echo "CONFIG_BMI160_TRIGGER=y" >> arc/prj.conf.tmp
            echo "CONFIG_BMI160_TRIGGER_OWN_THREAD=y" >> arc/prj.conf.tmp
        fi
        bmi160=$(grep -E AmbientLightSensor $SCRIPT)
        if [ $? -eq 0 ]; then
            MODULES+=" -DBUILD_MODULE_SENSOR_LIGHT"
            echo "CONFIG_ADC=y" >> arc/prj.conf.tmp
        fi
    fi
fi

console=$(grep console $SCRIPT)
if [ $? -eq 0 ] || check_config_file ZJS_CONSOLE && [[ $MODULE != *"BUILD_MODULE_CONSOLE"* ]]; then
    >&2 echo Using module: Console
    MODULES+=" -DBUILD_MODULE_CONSOLE"
    echo "export ZJS_CONSOLE=y" >> zjs.conf.tmp
fi

echo "# ---------------------------" >> zjs.conf.tmp
echo $MODULES
