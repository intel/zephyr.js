// Copyright (c) 2017, Intel Corporation.

#ifndef __ashell_comms_h__
#define __ashell_comms_h__

#include <arch/cpu.h>
#include <string.h>

#define ASCII_SUB    0x1A  // substitute, Ctrl+Z
#define ASCII_FS     0x1C  // file separator
#define ASCII_GS     0x1D  // group separator
#define ASCII_RS     0x1E  // record separator
#define ASCII_US     0x1F  // unit separator

/**
 TODO: move this text to a README.

 IDE mode defines its own protocol over WebUSB that is also implemented by
 a WebIDE module on the client side. The client sends text command lines, and
 receives JSON replies.

 The content of the messages is not generic, but applied to the ashell/WebIDE
 use cases. Message serialization is experimental. CBOR is possible later.

 * Request packet serialization: text commands
 - preamble: '{'
 - separator: ' '
 - postamble: '}'
 - first token: command (init, save, run, stop, ls, cat, mv, rm, boot, reboot)
 - second token: arg1, string (required for: save, run, mv, rm, boot)
 - third token: arg2, string (move), or <0x1E><bytes><0x1A> (save)

[Binary] data stream:
- preamble: 0x1E
- postamble: 0x1A.
Inside the stream, the 0x1E, 0x1A, and \ characters are escaped with \.

 * Reply packet serialization: JSON
 - preamble: '{'
 - "cmd" : "string" (init, save, run, stop, list, move, remove, boot, reboot)
 - "status": "string" (ok | error)
 - "data": ["string"] (only needed for list, as array of strings)
 - postamble: '}'

Text data stream as reply (cat command):
 - a message as JSON reply with data: ["start"]
 - binary data
 - a message as JSON reply with data: ["end"]

**/

#ifndef TX_POOL_SIZE      // from WebUSB driver
#define TX_POOL_SIZE      64
#endif

#define P_SPOOL_SIZE      (2 * TX_POOL_SIZE)  // WebUSB driver tx buffer size

// Errors are used as negative numbers in the code, and sent as positive numbers.
typedef enum {
  NO_ERROR                    = 0,
  ERROR_GENERIC,            //  1
  ERROR_INVALID_MSG,        //  2
  ERROR_INVALID_CMD,        //  3
  ERROR_INVALID_ARG,        //  4
  ERROR_MISSING_ARG,        //  5
  ERROR_TOO_MANY_ARGS,      //  6
  ERROR_UNEXPECTED_CHAR,    //  7
  ERROR_INVALID_FILENAME,   //  8
  ERROR_INVALID_STREAM,     //  9
  ERROR_FILE,               // 10
  ERROR_FILE_OPEN,          // 11
  ERROR_FILE_WRITE,         // 12
  ERROR_DIR_OPEN,           // 13
  ERROR_DATA_TRUNCATED      // 14
} ashell_error_t;

// Process a buffer (part of a message) in WebUSB driver context.
void ide_receive(u8_t *buf, size_t len);

// Init WebUSB with a receiver callback.
void ide_init();

// For periodic jobs called from mainloop. Not used.
void ide_process();

// Send arbitrary data over WebUSB.
int ide_send_buffer(char *buffer, size_t size);

// Spool formatted data, but don't send it yet.
int ide_spool(char *format, ...);

// Send all spooled data.
int ide_spool_flush();

// Return remaning bytes in spool.
int ide_spool_space();

// Return a pointer to the first free byte in the spool.
char *ide_spool_ptr();

// Add the given number of bytes to the spool current length.
void ide_spool_adjust(size_t size);

#if ASHELL_IDE_DBG
#define IDE_DBG(...) do { \
    ide_spool( __VA_ARGS__ ); \
    ide_spool_flush(); \
} while(0)
#else
#define IDE_DBG(...)  do { ; } while (0)
#endif

#endif  // __ashell_comms_h__
