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

JSTMPFILE=js.tmp
if [ -f $JSTMPFILE ]; then
    rm $JSTMPFILE
fi

MODULES=''
BOARD=$1
SCRIPT=$2
CONFIG=$3
ASHELL=$4

PRJFILE=prj.conf.tmp
ARCPRJFILE=arc/prj.conf.tmp
CONFFILE=zjs.conf.tmp
FEATUREFILE=outdir/$BOARD/jerry_feature.profile

if [ -f $FEATUREFILE ]; then
    cp $FEATUREFILE $FEATUREFILE.bak
fi
mkdir -p outdir/$BOARD/ && cp fragments/jerry_feature.base $FEATUREFILE

echo "# Modules found in $SCRIPT:" > $PRJFILE
echo "# Modules found in $SCRIPT:" > $ARCPRJFILE
tmpstr="# ZJS flags set by analyze.sh"

if [ -n "$CONFIG" ]; then
    tmpstr+=" and $CONFIG"
fi

echo $tmpstr > $CONFFILE

function check_for_js_require()
{
    js_files=$(grep "require *( *['\"].*\.js['\"] *)" $SCRIPT | grep -o "['\"].*\.js['\"]" | tr -d \'\")

    for file in $js_files
    do
         >&2 echo "JavaScript module included: $file"
         # Add the module JS to the temporary JS file
         cat "$ZJS_BASE/modules/$file" >> $JSTMPFILE
    done

    # Add the primary JS file to the temporary JS file
    cat "$SCRIPT" >> $JSTMPFILE
    SCRIPT=$JSTMPFILE
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
    #          If found, it will be put in zjs.conf.tmp and inserted into
    #          src/Makefile
    if [ -n "$CONFIG" ]; then
        grep -v '^#' < $CONFIG | grep $1='y\|m' >> /dev/null
        return $?
    fi
    return 1;
}

function check_for_feature()
{
    # effects: checks script for occurrence of a JerryScript built-in feature
    #          e.g. Math, Promise, JSON. If found it will enable that feature
    #          in the feature profile.
    rval=$(grep $1 $SCRIPT)
    return $?
}

function feature_on()
{
    >&2 echo Enabling JrS feature: $1
    sed -i "s/$1/#&/" $FEATUREFILE
}

# Check if buffer module required in the JS or config file
grep Buffer\(.*\) $SCRIPT > /dev/null
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
if check_for_require ws || check_config_file ZJS_WS; then
    >&2 echo Using module: ws
    MODULES+=" -DBUILD_MODULE_WS -DBUILD_MODULE_BUFFER -DBUILD_MODULE_EVENTS"
    echo "export ZJS_WS=y" >> $CONFFILE
    echo "export ZJS_BUFFER=y" >> $CONFFILE
    echo "export ZJS_EVENTS=y" >> $CONFFILE
    echo "export ZJS_NET_CONFIG=y" >> $CONFFILE
    echo "CONFIG_NETWORKING=y" >> $PRJFILE
    echo "CONFIG_NET_IPV6=y" >> $PRJFILE
    echo "CONFIG_NET_IPV4=y" >> $PRJFILE
    echo "CONFIG_NET_TCP=y" >> $PRJFILE
    echo "CONFIG_TEST_RANDOM_GENERATOR=y" >> $PRJFILE
    echo "CONFIG_INIT_STACKS=y" >> $PRJFILE
    echo "CONFIG_PRINTK=y" >> $PRJFILE
    echo "CONFIG_NET_STATISTICS=y" >> $PRJFILE
    echo "CONFIG_NET_NBUF_RX_COUNT=9" >> $PRJFILE
    echo "CONFIG_NET_NBUF_TX_COUNT=9" >> $PRJFILE
    echo "CONFIG_NET_NBUF_RX_DATA_COUNT=13" >> $PRJFILE
    echo "CONFIG_NET_NBUF_TX_DATA_COUNT=13" >> $PRJFILE
    echo "CONFIG_NET_IF_UNICAST_IPV6_ADDR_COUNT=3" >> $PRJFILE
    echo "CONFIG_NET_IF_MCAST_IPV6_ADDR_COUNT=2" >> $PRJFILE
    echo "CONFIG_NET_MAX_CONTEXTS=10" >> $PRJFILE
    echo "CONFIG_NET_MGMT=y" >> $PRJFILE
    echo "CONFIG_NET_MGMT_EVENT=y" >> $PRJFILE
    echo "CONFIG_MBEDTLS=y" >> $PRJFILE
    echo "CONFIG_MBEDTLS_BUILTIN=y" >> $PRJFILE
    echo "CONFIG_MBEDTLS_CFG_FILE=\"$ZJS_BASE/src/zjs_mbedtls_config.h\"" >> $PRJFILE
    echo "CONFIG_PRINTK=y" >> $PRJFILE

    if [ $BOARD = "qemu_x86" ]; then
        echo "CONFIG_NET_SLIP_TAP=y" >> $PRJFILE
    elif [ $BOARD = "arduino_101" ]; then
        echo "CONFIG_BLUETOOTH=y" >> $PRJFILE
        echo "CONFIG_BLUETOOTH_SMP=y" >> $PRJFILE
        echo "CONFIG_BLUETOOTH_SIGNING=y" >> $PRJFILE
        echo "CONFIG_BLUETOOTH_PERIPHERAL=y" >> $PRJFILE
        echo "CONFIG_BLUETOOTH_L2CAP_DYNAMIC_CHANNEL=y" >> $PRJFILE
        echo "CONFIG_NETWORKING_WITH_6LOWPAN=y" >> $PRJFILE
        echo "CONFIG_6LOWPAN_COMPRESSION_IPHC=y" >> $PRJFILE
        echo "CONFIG_NET_L2_BLUETOOTH_ZEP1656=y" >> $PRJFILE
        echo "CONFIG_NET_L2_BLUETOOTH=y" >> $PRJFILE
    fi
fi


# Check for native modules
if check_for_require net || check_config_file ZJS_NET; then
    >&2 echo Using module: net
    MODULES+=" -DBUILD_MODULE_NET -DBUILD_MODULE_BUFFER -DBUILD_MODULE_EVENTS"
    echo "export ZJS_NET=y" >> $CONFFILE
    echo "export ZJS_BUFFER=y" >> $CONFFILE
    echo "export ZJS_EVENTS=y" >> $CONFFILE
    echo "export ZJS_NET_CONFIG=y" >> $CONFFILE
    echo "CONFIG_NETWORKING=y" >> $PRJFILE
    echo "CONFIG_NET_IPV6=y" >> $PRJFILE
    echo "CONFIG_NET_IPV4=y" >> $PRJFILE
    echo "CONFIG_NET_TCP=y" >> $PRJFILE
    echo "CONFIG_TEST_RANDOM_GENERATOR=y" >> $PRJFILE
    echo "CONFIG_INIT_STACKS=y" >> $PRJFILE
    echo "CONFIG_PRINTK=y" >> $PRJFILE
    echo "CONFIG_NET_STATISTICS=y" >> $PRJFILE
    echo "CONFIG_NET_NBUF_RX_COUNT=14" >> $PRJFILE
    echo "CONFIG_NET_NBUF_TX_COUNT=14" >> $PRJFILE
    echo "CONFIG_NET_NBUF_DATA_COUNT=30" >> $PRJFILE
    echo "CONFIG_NET_IF_UNICAST_IPV6_ADDR_COUNT=3" >> $PRJFILE
    echo "CONFIG_NET_IF_MCAST_IPV6_ADDR_COUNT=2" >> $PRJFILE
    echo "CONFIG_NET_MAX_CONTEXTS=10" >> $PRJFILE
    echo "CONFIG_NET_MGMT=y" >> $PRJFILE
    echo "CONFIG_NET_MGMT_EVENT=y" >> $PRJFILE

    if [ $BOARD = "qemu_x86" ]; then
        echo "CONFIG_NET_SLIP_TAP=y" >> $PRJFILE
    elif [ $BOARD = "arduino_101" ] || [ $BOARD = "nrf52_pca10040" ]; then
        echo "CONFIG_BLUETOOTH=y" >> $PRJFILE
        echo "CONFIG_BLUETOOTH_SMP=y" >> $PRJFILE
        echo "CONFIG_BLUETOOTH_SIGNING=y" >> $PRJFILE
        echo "CONFIG_BLUETOOTH_PERIPHERAL=y" >> $PRJFILE
        echo "CONFIG_BLUETOOTH_L2CAP_DYNAMIC_CHANNEL=y" >> $PRJFILE
        echo "CONFIG_NETWORKING_WITH_6LOWPAN=y" >> $PRJFILE
        echo "CONFIG_6LOWPAN_COMPRESSION_IPHC=y" >> $PRJFILE
        echo "CONFIG_NET_L2_BLUETOOTH_ZEP1656=y" >> $PRJFILE
        echo "CONFIG_NET_L2_BLUETOOTH=y" >> $PRJFILE
    elif [ $BOARD = "frdm_k64f" ]; then
        echo "CONFIG_NET_L2_ETHERNET=y" >> $PRJFILE
    fi
fi

if check_for_require dgram || check_config_file ZJS_DGRAM; then
    >&2 echo Using module: Dgram
    MODULES+=" -DBUILD_MODULE_DGRAM -DBUILD_MODULE_BUFFER"
    echo "export ZJS_DGRAM=y" >> $CONFFILE
    echo "export ZJS_BUFFER=y" >> $CONFFILE
    echo "export ZJS_NET_CONFIG=y" >> $CONFFILE
    echo "CONFIG_NETWORKING=y" >> $PRJFILE
    echo "CONFIG_NET_IPV6=y" >> $PRJFILE
    echo "CONFIG_NET_IPV4=y" >> $PRJFILE
    echo "CONFIG_NET_UDP=y" >> $PRJFILE

    # Buffer number options - these should be handled more generally
    echo "CONFIG_NET_NBUF_RX_COUNT=10" >> $PRJFILE
    echo "CONFIG_NET_NBUF_TX_COUNT=10" >> $PRJFILE
    echo "CONFIG_NET_NBUF_DATA_COUNT=30" >> $PRJFILE

    # Debug options
    echo "CONFIG_TEST_RANDOM_GENERATOR=y" >> $PRJFILE
    echo "CONFIG_PRINTK=y" >> $PRJFILE
    echo "CONFIG_NET_LOG=y" >> $PRJFILE
    echo "CONFIG_NET_STATISTICS=y" >> $PRJFILE

    if [ $BOARD = "qemu_x86" ]; then
        echo "CONFIG_NET_SLIP_TAP=y" >> $PRJFILE
    # BLE Options
    elif [ $BOARD = "arduino_101" ] || [ $BOARD = "nrf52_pca10040" ]; then
        echo "CONFIG_BLUETOOTH=y" >> $PRJFILE
        echo "CONFIG_BLUETOOTH_SMP=y" >> $PRJFILE
        echo "CONFIG_BLUETOOTH_SIGNING=y" >> $PRJFILE
        echo "CONFIG_BLUETOOTH_PERIPHERAL=y" >> $PRJFILE
        echo "CONFIG_BLUETOOTH_L2CAP_DYNAMIC_CHANNEL=y" >> $PRJFILE
        echo "CONFIG_NETWORKING_WITH_6LOWPAN=y" >> $PRJFILE
        echo "CONFIG_6LOWPAN_COMPRESSION_IPHC=y" >> $PRJFILE
        echo "CONFIG_NET_L2_BLUETOOTH_ZEP1656=y" >> $PRJFILE
        echo "CONFIG_NET_L2_BLUETOOTH=y" >> $PRJFILE
    elif [ $BOARD = "frdm_k64f" ]; then
        echo "CONFIG_NET_L2_ETHERNET=y" >> $PRJFILE
    fi
fi

if check_for_require events || check_config_file ZJS_EVENTS; then
    >&2 echo Using module: Events
    MODULES+=" -DBUILD_MODULE_EVENTS"
    echo "export ZJS_EVENTS=y" >> $CONFFILE
fi

if check_for_require ocf || check_config_file ZJS_OCF; then
    >&2 echo Using module: OCF
    MODULES+=" -DBUILD_MODULE_OCF"
    MODULES+=" -DBUILD_MODULE_EVENTS"
    MODULES+=" -DBUILD_MODULE_PROMISE"
    MODULES+=" -DJERRY_PORT_ENABLE_JOBQUEUE"
    if grep -q "require *( *['\"]ocf['\"] *)*;" $SCRIPT; then
        OCF_OBJ=$(grep "require('ocf')" $SCRIPT  | cut -d'=' -f1 | cut -d' ' -f2)
        if grep -q "$OCF_OBJ.client" $SCRIPT; then
            >&2 echo Using module: OCF client
            MODULES+=" -DOC_CLIENT"
        fi
        if grep -q "$OCF_OBJ.server" $SCRIPT; then
            >&2 echo Using module: OCF server
            MODULES+=" -DOC_SERVER"
        fi
    else
        if grep -q "require *( *['\"]ocf['\"] *).server*" $SCRIPT; then
            >&2 echo Using module: OCF server
            MODULES+=" -DOC_SERVER"
        fi
        if grep -q "require *( *['\"]ocf['\"] *).client*" $SCRIPT; then
            >&2 echo Using module: OCF client
            MODULES+=" -DOC_CLIENT"
        fi
    fi

    if [ $BOARD = "qemu_x86" ]; then
        echo "CONFIG_NET_LOG=y" >> $PRJFILE
        echo "CONFIG_NET_SLIP=y" >> $PRJFILE
        echo "CONFIG_NET_SLIP_TAP=y" >> $PRJFILE
    elif [ $BOARD = "arduino_101" ] || [ $BOARD = "nrf52_pca10040" ]; then
        echo "CONFIG_BLUETOOTH=y" >> $PRJFILE
        echo "CONFIG_BLUETOOTH_SMP=y" >> $PRJFILE
        echo "CONFIG_BLUETOOTH_SIGNING=y" >> $PRJFILE
        echo "CONFIG_BLUETOOTH_PERIPHERAL=y" >> $PRJFILE
        echo "CONFIG_BLUETOOTH_L2CAP_DYNAMIC_CHANNEL=y" >> $PRJFILE
        echo "CONFIG_NETWORKING_WITH_6LOWPAN=y" >> $PRJFILE
        echo "CONFIG_6LOWPAN_COMPRESSION_IPHC=y" >> $PRJFILE
        echo "CONFIG_NET_L2_BLUETOOTH_ZEP1656=y" >> $PRJFILE
        echo "CONFIG_NET_L2_BLUETOOTH=y" >> $PRJFILE
    elif [ $BOARD = "frdm_k64f" ]; then
        echo "CONFIG_NET_L2_ETHERNET=y" >> $PRJFILE
    fi

    echo "CONFIG_NETWORKING=y" >> $PRJFILE
    echo "CONFIG_NET_IPV6=y" >> $PRJFILE
    echo "CONFIG_NET_UDP=y" >> $PRJFILE
    echo "CONFIG_TEST_RANDOM_GENERATOR=y" >> $PRJFILE
    echo "CONFIG_INIT_STACKS=y" >> $PRJFILE
    echo "CONFIG_NET_NBUF_RX_COUNT=14" >> $PRJFILE
    echo "CONFIG_NET_NBUF_TX_COUNT=14" >> $PRJFILE
    echo "CONFIG_NET_IF_UNICAST_IPV6_ADDR_COUNT=1" >> $PRJFILE
    echo "CONFIG_NET_IF_MCAST_IPV6_ADDR_COUNT=1" >> $PRJFILE
    echo "CONFIG_NET_MAX_CONTEXTS=3" >> $PRJFILE

    echo "export ZJS_OCF=y" >> $CONFFILE
    echo "export ZJS_EVENTS=y" >> $CONFFILE
    echo "export ZJS_NET_CONFIG=y" >> $CONFFILE
    echo "export ZJS_PROMISE=y" >> $CONFFILE
    feature_on CONFIG_DISABLE_ES2015_PROMISE_BUILTIN
    feature_on CONFIG_DISABLE_ES2015_BUILTIN
    feature_on CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
fi

if check_for_require gpio || check_config_file ZJS_GPIO; then
    >&2 echo Using module: GPIO
    MODULES+=" -DBUILD_MODULE_GPIO"
    echo "CONFIG_GPIO=y" >> $PRJFILE
    echo "export ZJS_BOARD=y" >> $CONFFILE
    echo "export ZJS_GPIO=y" >> $CONFFILE
    gpio=true
fi

if check_for_require performance || check_config_file ZJS_PERFORMANCE; then
    >&2 echo Using module: Performance
    MODULES+=" -DBUILD_MODULE_PERFORMANCE"
    echo "export ZJS_PERFORMANCE=y" >> $CONFFILE
fi

if check_for_require pwm || check_config_file ZJS_PWM; then
    >&2 echo Using module: PWM
    MODULES+=" -DBUILD_MODULE_PWM"
    echo "CONFIG_PWM=y" >> $PRJFILE
    echo "CONFIG_PWM_QMSI_NUM_PORTS=4" >> $PRJFILE
    echo "export ZJS_PWM=y" >> $CONFFILE
fi

if check_for_require uart || check_config_file ZJS_UART; then
    >&2 echo Using module: UART
    if [ $BOARD = "arduino_101" ]; then
        echo "CONFIG_GPIO=y" >> $PRJFILE
        echo "CONFIG_USB=y" >> $PRJFILE
        echo "CONFIG_USB_DW=y" >> $PRJFILE
        echo "CONFIG_USB_DEVICE_STACK=y" >> $PRJFILE
        echo "CONFIG_SYS_LOG_USB_DW_LEVEL=0" >> $PRJFILE
        if [ "$ASHELL" != "y" ]; then
            echo "CONFIG_USB_CDC_ACM=y" >> $PRJFILE
        fi
        echo "CONFIG_SYS_LOG_USB_LEVEL=0" >> $PRJFILE
        echo "CONFIG_UART_LINE_CTRL=y" >> $PRJFILE
    fi
    echo "CONFIG_SERIAL=y" >> $PRJFILE
    echo "CONFIG_UART_INTERRUPT_DRIVEN=y" >> $PRJFILE
    MODULES+=" -DBUILD_MODULE_UART"
    MODULES+=" -DBUILD_MODULE_EVENTS"
    echo "export ZJS_EVENTS=y" >> $CONFFILE
    echo "export ZJS_UART=y" >> $CONFFILE
    buffer=true;
fi

if check_for_require ble || check_config_file ZJS_BLE; then
    >&2 echo Using module: BLE
    MODULES+=" -DBUILD_MODULE_BLE"
    echo "CONFIG_BLUETOOTH=y" >> $PRJFILE
    echo "CONFIG_BLUETOOTH_STACK_NBLE=y" >> $PRJFILE
    echo "CONFIG_NBLE=y" >> $PRJFILE
    echo "CONFIG_BLUETOOTH_SMP=y" >> $PRJFILE
    echo "CONFIG_BLUETOOTH_PERIPHERAL=y" >> $PRJFILE
    echo "CONFIG_BLUETOOTH_GATT_DYNAMIC_DB=y" >> $PRJFILE
    echo "CONFIG_BLUETOOTH_MAX_CONN=4" >> $PRJFILE
    MODULES+=" -DBUILD_MODULE_EVENTS"
    echo "export ZJS_EVENTS=y" >> $CONFFILE
    echo "export ZJS_BLE=y" >> $CONFFILE
    buffer=true;
fi

if check_for_require aio || check_config_file ZJS_AIO; then
    >&2 echo Using module: AIO
    MODULES+=" -DBUILD_MODULE_AIO"
    echo "CONFIG_ADC=y" >> $ARCPRJFILE
    echo "export ZJS_AIO=y" >> $CONFFILE
fi

if check_for_require fs || check_config_file ZJS_FS; then
    >&2 echo Using module: FS
    OBJ_NAME=$(grep "require *( *['\"]fs['\"] *)*;" $SCRIPT | cut -d'=' -f1 | cut -d' ' -f2)

    if [ "$OBJ_NAME" != "" ]; then
        if grep "$OBJ_NAME\..*(" $SCRIPT | grep -q -v "Sync"; then
            MODULES+=" -DZJS_FS_ASYNC_APIS"
        fi
    fi
    MODULES+=" -DBUILD_MODULE_FS -DBUILD_MODULE_BUFFER"
    if [ $BOARD = "arduino_101" ]; then
        echo "CONFIG_FS_FAT_FLASH_DISK_W25QXXDV=y" >> $PRJFILE
        echo "CONFIG_DISK_ACCESS_FLASH=y" >> $PRJFILE
    else
        echo "CONFIG_DISK_ACCESS_RAM=y" >> $PRJFILE
    fi
    echo "CONFIG_FILE_SYSTEM=y" >> $PRJFILE
    echo "CONFIG_FILE_SYSTEM_FAT=y" >> $PRJFILE

    echo "CONFIG_FLASH=y" >> $PRJFILE
    echo "CONFIG_SPI=y" >> $PRJFILE
    echo "CONFIG_GPIO=y" >> $PRJFILE
    echo "export ZJS_FS=y" >> $CONFFILE
    echo "export ZJS_BUFFER=y" >> $CONFFILE
fi

if check_for_require i2c || check_config_file ZJS_I2C; then
    >&2 echo Using module: I2C
    MODULES+=" -DBUILD_MODULE_I2C"
    if [ $BOARD = "arduino_101" ]; then
        echo "CONFIG_I2C=y" >> $ARCPRJFILE
    else
        echo "CONFIG_I2C=y" >> $PRJFILE
    fi
    echo "export ZJS_I2C=y" >> $CONFFILE
    buffer=true;
fi

if check_for_require grove_lcd || check_config_file ZJS_GROVE_LCD; then
    >&2 echo Using module: Grove LCD
    MODULES+=" -DBUILD_MODULE_GROVE_LCD"
    if [ $BOARD = "arduino_101" ]; then
        echo "CONFIG_I2C=y" >> $ARCPRJFILE
        echo "CONFIG_GROVE=y" >> $ARCPRJFILE
        echo "CONFIG_GROVE_LCD_RGB=y" >> $ARCPRJFILE
        echo "CONFIG_GROVE_LCD_RGB_INIT_PRIORITY=90" >> $ARCPRJFILE
    else
        echo "CONFIG_I2C=y" >> $PRJFILE
        echo "CONFIG_GROVE=y" >> $PRJFILE
        echo "CONFIG_GROVE_LCD_RGB=y" >> $PRJFILE
        echo "CONFIG_GROVE_LCD_RGB_INIT_PRIORITY=90" >> $PRJFILE
    fi
    echo "export ZJS_GROVE_LCD=y" >> $CONFFILE
fi

if check_for_require board || check_config_file ZJS_BOARD; then
    >&2 echo Using module: Board
    MODULES+=" -DBUILD_MODULE_BOARD"
    echo "export ZJS_BOARD=y" >> $CONFFILE
fi

if check_for_require arduino101_pins || check_config_file ZJS_ARDUINO101_PINS; then
    >&2 echo Using module: A101 Pins
    MODULES+=" -DBUILD_MODULE_A101"
    echo "export ZJS_ARDUINO101_PINS=y" >> $CONFFILE
fi

if check_for_require test_promise; then
    >&2 echo Using module: test_promise
    MODULES+=" -DBUILD_MODULE_TEST_PROMISE -DBUILD_MODULE_PROMISE -DJERRY_PORT_ENABLE_JOBQUEUE"
    echo "export ZJS_TEST_PROMISE=y" >> $CONFFILE
    echo "export ZJS_PROMISE=y" >> $CONFFILE
    feature_on CONFIG_DISABLE_ES2015_PROMISE_BUILTIN
    feature_on CONFIG_DISABLE_ES2015_BUILTIN
    feature_on CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
fi

if check_for_require test_callbacks; then
    >&2 echo Using module: test_promise
    MODULES+=" -DBUILD_MODULE_TEST_CALLBACKS"
    echo "export ZJS_TEST_CALLBACKS=y" >> $CONFFILE
fi

interval=$(grep "setInterval\|setTimeout\|setImmediate" $SCRIPT)
if [ $? -eq 0 ] || check_config_file ZJS_TIMERS; then
    MODULES+=" -DBUILD_MODULE_TIMER"
    echo "export ZJS_TIMERS=y" >> $CONFFILE
fi

if $buffer && [[ $MODULE != *"BUILD_MODULE_BUFFER"* ]]; then
    >&2 echo Using module: Buffer
    MODULES+=" -DBUILD_MODULE_BUFFER"
    echo "export ZJS_BUFFER=y" >> $CONFFILE
fi
if check_for_feature "Accelerometer\|Gyroscope\|AmbientLightSensor\|TemperatureSensor" || check_config_file ZJS_SENSOR; then
    >&2 echo Using module: Sensor
    MODULES+=" -DBUILD_MODULE_SENSOR"
    echo "export ZJS_SENSOR=y" >> $CONFFILE
    if check_for_feature "Accelerometer" || check_config_file ZJS_SENSOR_ACCEL; then
        >&2 echo Using module: Sensor Accelerometer
        echo "export ZJS_SENSOR_ACCEL=y" >> $CONFFILE
        MODULES+=" -DBUILD_MODULE_SENSOR_ACCEL"
        sensor_accel=true
    fi
    if check_for_feature "Gyroscope" || check_config_file ZJS_SENSOR_GYRO; then
        >&2 echo Using module: Sensor Gyroscope
        echo "export ZJS_SENSOR_GYRO=y" >> $CONFFILE
        MODULES+=" -DBUILD_MODULE_SENSOR_GYRO"
        sensor_gyro=true
    fi
    if check_for_feature "AmbientLightSensor" || check_config_file ZJS_SENSOR_LIGHT; then
        >&2 echo Using module: Sensor Ambient Light
        echo "export ZJS_SENSOR_LIGHT=y" >> $CONFFILE
        MODULES+=" -DBUILD_MODULE_SENSOR_LIGHT"
        sensor_light=true
    fi
    if check_for_feature "TemperatureSensor" || check_config_file ZJS_SENSOR_TEMP; then
        >&2 echo Using module: Sensor Temperature
        echo "export ZJS_SENSOR_TEMP=y" >> $CONFFILE
        MODULES+=" -DBUILD_MODULE_SENSOR_TEMP"
        sensor_temp=true
    fi
    if [[ $BOARD = "arduino_101" ]]; then
        if $sensor_accel || $sensor_gyro || $sensor_temp; then
            echo "CONFIG_SENSOR=y" >> $ARCPRJFILE
            echo "CONFIG_SPI=y" >> $ARCPRJFILE
            echo "CONFIG_SPI_0=n" >> $ARCPRJFILE
            echo "CONFIG_SPI_1=n" >> $ARCPRJFILE
            echo "CONFIG_SPI_SS_1_NAME=\"SPI_SS_1\"" >> $ARCPRJFILE
            echo "CONFIG_BMI160=y" >> $ARCPRJFILE
            echo "CONFIG_BMI160_NAME=\"bmi160\"" >> $ARCPRJFILE
            echo "CONFIG_BMI160_SPI_PORT_NAME=\"SPI_SS_1\"" >> $ARCPRJFILE
            echo "CONFIG_BMI160_SLAVE=1" >> $ARCPRJFILE
            echo "CONFIG_BMI160_SPI_BUS_FREQ=88" >> $ARCPRJFILE
            if $gpio; then
                # Workaround for Zephyr bug ZEP-2024: conflict with BMI160 trigger
                # and GPIO, so trigger mode can only be enabled for non-ashell mode
                # and ashell will default to polling
                echo "CONFIG_BMI160_TRIGGER_NONE=y" >> $ARCPRJFILE
            else
                echo "CONFIG_GPIO=y" >> $ARCPRJFILE
                echo "CONFIG_BMI160_TRIGGER=y" >> $ARCPRJFILE
                echo "CONFIG_BMI160_TRIGGER_OWN_THREAD=y" >> $ARCPRJFILE
            fi
        fi
        if $sensor_light; then
            echo "CONFIG_ADC=y" >> $ARCPRJFILE
        fi
    fi
fi

if check_for_require pme || check_config_file ZJS_PME; then
    if [[ $BOARD = "arduino_101" ]]; then
        >&2 echo Using module: Curie PME
        MODULES+=" -DBUILD_MODULE_PME"
        echo "export ZJS_PME=y" >> $CONFFILE
    fi
fi

console=$(grep console $SCRIPT)
if [ $? -eq 0 ] || check_config_file ZJS_CONSOLE && [[ $MODULE != *"BUILD_MODULE_CONSOLE"* ]]; then
    >&2 echo Using module: Console
    MODULES+=" -DBUILD_MODULE_CONSOLE"
    echo "export ZJS_CONSOLE=y" >> $CONFFILE
fi

if check_for_feature "Math\."; then
    >&2 echo Using JrS module: Math
    feature_on CONFIG_DISABLE_MATH_BUILTIN
fi

if check_for_feature "Promise("; then
    >&2 echo Using JrS module: Promise
    MODULES+=" -DJERRY_PORT_ENABLE_JOBQUEUE"
    feature_on CONFIG_DISABLE_ES2015_PROMISE_BUILTIN
    feature_on CONFIG_DISABLE_ES2015_BUILTIN
    feature_on CONFIG_DISABLE_ES2015_TYPEDARRAY_BUILTIN
fi

if check_for_feature "JSON\."; then
    >&2 echo Using JrS module: JSON
    feature_on CONFIG_DISABLE_JSON_BUILTIN
fi

echo "# ---------------------------" >> $CONFFILE
echo $MODULES
