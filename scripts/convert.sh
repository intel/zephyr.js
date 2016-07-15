#!/bin/bash

#
# This script converts a file's ASCII data into a C char array:
#
# const char script_gen[] = { ........ }
#

progress='.'

# Draw a loading bar given count and size
percent()
{
	percent=$((200*$1/$2 % 2 + 100*$1/$2))
	printf "\r%i%% 0x%02x " $((percent)) $3
	len=$(($((80*$1)) / $2))
	# Add a . if the % has progressed
	if [ $((len)) -gt ${#progress} ]; then
		progress="$progress."
	fi
	printf "$progress"
}

INPUT=$1
OUTPUT=$2

# Check that yui-compressor exists
which yui-compressor &> /dev/null
if [ $? == 1 ]; then
    cat $INPUT > /tmp/gen.tmp
else 
    yui-compressor $INPUT > /tmp/gen.tmp
fi

COUNT=0

if [ "$(uname)" == "Darwin" ]; then
    SIZE=$(stat -f%z "$INPUT")        
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
    SIZE=$(stat -c%s "$INPUT")
fi

printf "/* This file was auto-generated */\n\n" > $OUTPUT
printf "const char script[] = \"" >> $OUTPUT

# No field separator, read whole file (IFS=),
# no backslash escape (-r),
# read 1 character at a time (-n1)
while IFS= read -r -n1 char
do
	if [ "$char" = "\"" ]; then
		printf "\\\\$char" >> $OUTPUT
	else
		printf "$char" >> $OUTPUT
	fi
	COUNT=$((COUNT+1))
	percent $COUNT $SIZE "'$char"
done < /tmp/gen.tmp

printf "\n"

# Terminate the string
printf "\";" >> $OUTPUT

rm -f /tmp/gen.tmp
