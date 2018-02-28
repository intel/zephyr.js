#!/usr/bin/env python3

#
# This script converts a file's ASCII data into a C char array:
#
# const char script_gen[] = { ........ }
#

# This script was written to be compatible with Python 3.4

import argparse
import collections
import os
import pathlib
import subprocess
import sys

Uglify = collections.namedtuple("Uglify", ['content', 'used_uglifyjs'])


def main():
    args = parse_args()
    minified_result = uglifyjs(args.input)
    write_minified(args.output, minified_result)


def write_minified(output_path, minified_result):
    """Write the minified output, escaping things as necessary

    :param output_path: A pathlib.Path() object of the output file.
    :param minified_result: An Uglify() object containing the content to write.
    """
    with output_path.open('w') as out_file:
        last_char = ""
        print('"', end='', file=out_file)
        for char in minified_result.content:
            # Escape quote symbols
            if char in ('"', ):
                print(r'\"', end='', file=out_file)
            elif last_char == "\\":
                # Special case for new line and carriage return that can appear
                # in a string e.g. print("some string\n");
                # This is needed because BASH reads the "\n" as two characters,
                # but in a C string it is just one character.
                if char in ('n', 'r'):
                    # NOTE(jlvillal): If not 'n' or 'r' we print nothing. This
                    # is how 'convert.sh' did it.
                    print('\\{}'.format(char), end='', file=out_file)
            elif char == "\n":
                if not minified_result.used_uglifyjs:
                    print(r'\n"', end='\n"', file=out_file)
            else:
                print(char, end='', file=out_file)
            last_char = char
        print('"', end='', file=out_file)


def uglifyjs(input_path):
    """Run 'uglifyjs' on the input file

    :param input_path: A pathlib.Path() object of the input file.
    :returns: An Uglify() object
    """
    filename = str(input_path)
    # NOTE(jlvillal): The docs say that '-nc' is the same as '--no-copyright'
    # but in testing it is not.
    # https://github.com/mishoo/UglifyJS/blob/2bc1d02363db3798d5df41fb5059a19edca9b7eb/README.html#L481-L483
    ugl_args = ["-nc", "-mt"]

    # uglifyjs seems to have vastly different options between versions, this
    # should work with both
    cmd_line = ['uglifyjs', '--version']
    try:
        result = subprocess.call(cmd_line, stdout=subprocess.DEVNULL)
    except FileNotFoundError:
        # We don't have uglifyjs, so return the contents of the file
        return Uglify(input_path.read_text(), False)

    if result == 0:
        # We have newer uglifyjs
        cmd_line = ['uglifyjs', filename] + ugl_args
    else:
        # Older uglifyjs
        cmd_line = ['uglifyjs'] + ugl_args + [filename]
    try:
        output = subprocess.check_output(cmd_line)
    except subprocess.CalledProcessError as exc:
        print("ERROR: Minification failed!")
        sys.exit(exc.returncode)
    return Uglify(output.decode('utf-8'), True)


def parse_args():
    parser = argparse.ArgumentParser(
        description=("Runs minifier on a JavaScript file and writes it out in "
                     "a C string format."))
    parser.add_argument("input", metavar='INPUT_FILE')
    parser.add_argument("output", metavar='OUTPUT_FILE')
    args = parser.parse_args()
    # Make all paths absolute and expand any "~/" usage.
    for arg_name in ('input', 'output'):
        setattr(args, arg_name, pathlib.Path(
            os.path.abspath(os.path.expanduser(getattr(args, arg_name)))))
    if not args.input.is_file():
        print("ERROR: The INPUT_FILE {!r} does not exist.".format(
            str(args.input)))
        parser.print_help()
        sys.exit(1)
    return args


if '__main__' == __name__:
    sys.exit(main())
