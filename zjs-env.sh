if [ "X$(basename -z -- "$0")" "==" "Xzjs-env.sh" ]; then
    echo "Source this file (do NOT execute it!) to set up the ZJS dev environment."
    exit
fi

# set the expected size of the X86 partition on the target device
# Note: This is specific to our current use of only Arduino 101
export ZJS_PARTITION=144
if [ "x$1" == "x256" ]; then
    export ZJS_PARTITION=256
elif [ "x$1" == "x216" ]; then
    export ZJS_PARTITION=216
elif [ -n "$1" -a "x$1" != "x144" ]; then
    echo Warning: Invalid partition size given: \'$1\'.
    echo Expected 256, 216, or 144 \(default\).
    echo
fi
echo ZJS_PARTITION: $ZJS_PARTITION

# identify ZJS source tree root directory
export ZJS_BASE=$( builtin cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
echo ZJS_BASE: $ZJS_BASE

# identify JerryScript source tree root directory
if [ "X$JERRY_BASE" != "X" ]; then
    echo JERRY_BASE: $JERRY_BASE
fi

# add scripts/ subdirectory to PATH
scripts_path=${ZJS_BASE}/scripts
echo "${PATH}" | grep -q "${scripts_path}"
[ $? != 0 ] && export PATH=${scripts_path}:${PATH}
unset scripts_path
