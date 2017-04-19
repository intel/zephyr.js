# This tool can test many JS test files at once using ashell. To use, flash a
# ZJS device with ashell and start it. Then run this script, passing in the
# files you want to test, as well as the serial port the board is connected to:

#    python scripts/ashell-monkey.py <port> [print] <list of files>

# The port parameter should be something like "/dev/ttyACM0"
# After the port, you can pass in an optional "print" string, which will print
# all the ashell output.
# After the port and print parameters pass in a list of files you would like
# to test. These should be ZJS test files which end in printing:

#    "TOTAL: X of Y tests passed"

# This is the string that ashell-monkey looks for to determine if the test is
# over, as well as if it passed or not. Here is an example of how you might
# run this script:

#    python scripts/ashell-monkey /dev/ttyACM0 print tests/test-buffer.js \
#        tests/test-fs.js tests/test-error.js tests/test-event.js

# This would run the buffer, fs, error, and event test scripts found under
# tests/. At the end you should get a message if any tests failed or if all
# succeeded.

import serial
import re
import sys
from time import sleep

start_idx = 2
do_print = False
if sys.argv[2] == "print":
    do_print = True
    start_idx = 3

print "TESTING FILES:"
for i in sys.argv[start_idx:]:
    print i

port = sys.argv[1]
ser = serial.Serial(port, 115200, timeout=0)
ser.write('help\r')
bytes = ''
state = "SETUP"
failures = []

for i in sys.argv[start_idx:]:
    while True:
        data = ser.read(9999)
        if len(data) > 0:
            bytes += data
            if do_print:
                print bytes

        if state == "SETUP":
            if "acm>" in bytes:
                print 'Ashell is ready'
                ser.write('load Assert.js\r')
                with open("modules/Assert.js", 'rb') as f:
                    ser.write(f.read())
                    ser.write('\r')
                ser.write('\x1a')
                bytes = ''
                state = "BEGIN"
        elif state == "BEGIN":
            if "acm>" in bytes:
                print 'Ashell is ready'
                ser.write('load test.js\r')
                with open(i, 'rb') as f:
                    ser.write(f.read())
                    ser.write('\r')
                ser.write('\x1a')
                ser.write('run test.js\r')
                bytes = ''
                state = "WAITING_EXECUTE"
        elif state == "WAITING_EXECUTE":
            if "run test.js" in bytes:
                state = "WAITING_RESULT"
                bytes = ''
        elif state == "WAITING_RESULT":
            match = re.search("TOTAL: [0-9]*[0-9] of [0-9]*[0-9] passed", bytes)
            if match:
                print "DONE with " + i
                arr = match.group(0).split(' ')
                if arr[1] != arr[3]:
                    print "test " + i + " failed"
                    failures += i
                state = "BEGIN"
                break

if len(failures) > 0:
    print "FALIURES:"
    for i in failures:
        print i
else:
    print "ALL TESTS PASSED"

ser.close()
