
if [ "X$(basename -z -- "$0")" "==" "Xzjs-env.sh" ]; then
    echo "Source this file (do NOT execute it!) to set the Zephyr JS development environment."
    exit
fi

# identify ZJS source tree root directory
export ZJS_BASE=$( builtin cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
echo ZJS_BASE: $ZJS_BASE

# identify Soletta source tree root directory
export SOLETTA_BASE_DIR=$ZJS_BASE/deps/soletta
echo SOLETTA_BASE_DIR: $SOLETTA_BASE_DIR

scripts_path=${ZJS_BASE}/scripts
echo "${PATH}" | grep -q "${scripts_path}"
[ $? != 0 ] && export PATH=${scripts_path}:${PATH}
unset scripts_path
