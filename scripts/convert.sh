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

# uglifyjs seems to have vastly different options between versions,
# this should work with both
if which uglifyjs &> /dev/null; then
    if uglifyjs --version &> /dev/null; then
        uglifyjs $INPUT -nc -mt > /tmp/gen.tmp
    else
        uglifyjs -nc -mt $INPUT > /tmp/gen.tmp
    fi
    ERR=$?
    if (($ERR > 0)); then
        echo Error: Minification failed!
        exit $ERR
    fi
    UGLIFY=1
else
    cat $INPUT > /tmp/gen.tmp
    UGLIFY=0
fi

COUNT=0

if [ "$(uname)" == "Darwin" ]; then
    SIZE=$(stat -f%z "$INPUT")
    FLAGS="-r -n 1"
else
    SIZE=$(stat -c%s "$INPUT")
    FLAGS="-r -N 1"
fi

printf "/* This file was auto-generated */\n\n" > $OUTPUT
printf "#include \"zjs_common.h\"\n\n" >> $OUTPUT
printf "const char *script_gen = \"" >> $OUTPUT

last_char=0

# No field separator, read whole file (IFS=),
# no backslash escape (-r),
# read 1 character at a time (-n1)
while IFS= read $FLAGS char
do
    # Case for escaping a quote (") character
    if [ "$char" = "\"" ]; then
        printf "\\\\$char" >> $OUTPUT
    elif [ "$last_char" = $'\\' ]; then
        # Special case for new line and carrage return that can appear in a
        # string e.g. print("some string\n");
        # This is needed because BASH reads the "\n" as two characters, but
        # in a C string it is just one character.
        if [ "$char" = $'r' ] || [ "$char" = $'n' ]; then
            printf "\\\\$char" >> $OUTPUT
        fi
    elif [ "$char" = $'\n' ] && [ $UGLIFY -eq 0 ]; then
        # Handle new lines if uglify is not installed
        printf "\\\n\"\n\"" >> $OUTPUT
    else
        printf "$char" >> $OUTPUT
    fi
    last_char=$char
    COUNT=$((COUNT+1))
    percent $COUNT $SIZE "'$char"
done < /tmp/gen.tmp

printf "\n"

# Terminate the string
printf "\";" >> $OUTPUT

rm -f /tmp/gen.tmp
