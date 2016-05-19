#!/bin/bash

# WARNING: This script must be started from this directory.

which jsrunner >& /dev/null
if [ "$?" == "1" ]; then
    echo "Error: missing 'jsrunner' command"
    exit 1
fi

curdir=$(pwd)

if [ ! -d ${curdir}/test_outdir ]; then
    mkdir ${curdir}/test_outdir
fi

topdir=$(pwd)/../../..
success=0
failures=0

tests=""
jerry_dir=${curdir}/../../../deps/jerryscript/tests/jerry
for i in `ls ${jerry_dir}`
do
    tests+=${jerry_dir}/$i
    tests+=" "
done

cp_dir=${curdir}/../../../deps/jerryscript/tests/jerry-test-suite
for i in `cat ${cp_dir}/compact-profile-list`
do
    tests+=${cp_dir}/$i
    tests+=" "
done

for test in $tests
do
    testname=`basename -s ".js" ${test}`
    # skip all js tests that use Math object
    fgrep Math ${test_dir}/$test >& /dev/null

    if [ "$?" == "1" ]; then

        # Copy the test to ../src.
        # Also add the assert function needed by many of the test cases.

        cat ${curdir}/assert.js $test >${curdir}/src/${testname}.js
        cd ${curdir}/src

        # generate the script.h
        jsrunner -f -s ${testname}.js >& /dev/null
        rm -f ${testname}.js
        cp ${topdir}/build/script.h .
        rm -rf ${topdir}/build

        # cleanup the previous build if any
        cd ${curdir}
        make pristine >& /dev/null
        rm -rf outdir

        # Start a child process to monitor the test run.
        # If test is completed, kill Qemu so next test can continue.
        outfile=${curdir}/test_outdir/${testname}.out
        ${curdir}/waitfortestcomplete.sh ${outfile} &

        # run the test
        echo -n "RUNNING TEST: $testname"
        make qemu >& ${outfile}

        fgrep "JERRYSCRIPT SUCCEEDED" ${outfile} >& /dev/null
        if [ "$?" == "0" ]; then
            echo " - Succeeded"
            (( success += 1 ))
        else
            echo " - Failed"
            (( failures += 1))
        fi
    fi

    # go back to the tests directory.
    cd ${curdir}

done

echo ""
echo "Number of tests passed: $success"
echo "Number of tests failed: $failures"
