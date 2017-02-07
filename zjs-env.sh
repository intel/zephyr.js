if [ "X$(basename -z -- "$0")" "==" "Xzjs-env.sh" ]; then
    echo "Source this file (do NOT execute it!) to set up the ZJS dev environment."
    exit
fi

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
