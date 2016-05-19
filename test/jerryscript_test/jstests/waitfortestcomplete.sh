#!/bin/bash
# Usage: waitfortestcomplete.sh outputfile

testname=`basename $1 .out`
for i in {1..30}
do
    sleep 1
    if [ -f $outfile ]; then
        tmp=`mktemp`
        # remove the \r from output
        sed 's/\r//g' <$1 >$tmp
        while read -r line
        do
            if [ "$line" == "JERRYSCRIPT SUCCEEDED" ]  ||
               [ "$line" == "JERRYSCRIPT FAILED" ]; then
                killall make >& /dev/null
                rm -f $tmp
                exit
            fi
        done < "$tmp"
        rm -f $tmp
    fi
done
killall make >& /dev/null
