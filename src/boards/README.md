# src/boards subdirectory

The files in this directory are not really header files but board configuration
files intended to be included in zjs_board.c source. They are split out here to
make it more obvious what needs to be defined to nicely support a new board,
and to reduce the confusion of nested #ifdefs.
