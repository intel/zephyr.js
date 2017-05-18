
## AShell introduction
Ashell provides a command line interface to script management on a device running Zephyr.js (the host) that can be connected to from clients such as web pages, e.g. via WebUSB on a serial line.

The minimum functionality on the host side is implementing the script management interface. Command line editing and parsing may be implemented either on the client side, or in ashell on the host side. Typically,
- an IDE client would implement local line editing functions and use the JSON command interface with Ashell script management.
- A CLI client would implement local line editing functions and use either the textual or JSON command interface.
- A simple serial connection to `screen` or other terminal would use host side line editing and the character based interface.

## Use cases
The following client use cases are supported:
- Connect to ashell running on a device.
- Run a script on the device.
- Save a script to the device with a given file name.
- Check (eval) a script file on the device.
- Start a script file on the device.
- Stop a running script on the device.
- Configure a script file to be automatically started at next boot.
- Delete a script file from the device.
- Rename a script file on the device.
- List script files saved on the device.
- List available space for scripts on the device.

## How does it work
<a name="context"></a>
- A client connects to ashell and configures ashell with a `context` that contains the following settings:
  * `verbose` mode: `on` or `off`
    - When `verbose` is `off`, the host will send only application output and shell prompt.
    - When `verbose` is `on`, the host will also send shell prints (errors, help etc).
  * `echo` mode: `on` or `off`.
    - When echo is `on`, line editing happens on the ashell (host) side. Transfer mode is set to `char`.
    - When echo is `off`, line editing is done on client side and full command lines are sent to the host. Transfer mode can be `line` or `json`, both are parsed by the host and provided to the script manager interface.
  * `transfer` mode: `char`, `line`, `json`
    - The `char` mode is used when line editing is done on the host side.
    - In `line` mode line editing is done on the client side, full command lines are sent to ashell that need to be parsed for command and arguments, and control characters are ignored.
    - In `json` mode data is exchanged in serialized [JSON format](#jsonschema), and the commands and arguments should be sent in parsed form, although the full command line may be included as well.
- When initialized, ashell will send the system prompt and if `verbose` is `on` and `echo` is `on`, also a help text.
- Then the client can send a simple command that completes, sends back data, and the system prompt when the command is finished.
- A client can send a command that needs input and spans over multiple lines. Each line sent to the host will contain a line number (in the context of the multi-line command). The host will send a command specific prompt after each received line, until the command completes, when the host sends the system prompt.

## Interfaces and modules
- `script-man.h`: script management interface that implements the use cases above. The main purpose of `ashell` is to connect to this interface through the configured transport, using either character based command line input with host side line editing (handled by `ashell-line.c`), or textual commands (client side line editing, handled by `ashell-cmd.c`), or JSON commands (handled by `ashell-cmd.c`).
- `ashell.h`: interface used from zephyr.js main (init and mainloop tick).
- `ashell.c`: implements the protocol (request-response), and uses `ashell-line` editor (if needed), `ashell-cmd` parser (if needed) and `script-man.h`.
- `comms.h`: interface for abstracting the transport, defines supported transport types, states, and callback interface. Provides buffer interface to `ashell.c`.
- `comms-uart.c`: implements `comms.h`, using the UART CDC ACM and WebUSB via UART transports, and defines UART specific mainloop integration and ashell integration.
- `ashell-line.h`: host side line editor on the host side.
- `ashell-cmd.h`: host side command parser (from text and JSON).

## Protocols
The client should implement the same protocol as the ashell server, based on this section.

### Textual command line
Each command line sent from the client to the ashell server contains a preamble, a text command and arguments, and a postamble.
- The preamble starts with TODO and ends with TODO.
  * if TODO is read, set `echo` mode to `on`.
  * if TODO is read, set `echo` mode to `off`.
  * if TODO is read, set `verbose` mode to `on`.
  * if TODO is read, set `verbose` mode to `off`.
- The postamble may be just a newline (`'\n'`) or `Ctrl+D` character, or otherwise starts with TODO and ends with TODO.
- The payload can contain any printable character.
- If a `Ctrl-C` character is encountered in the payload, the command line is discarded.
- If a `Ctrl-D` character is encountered, terminates the command context.
- Other control characters are ignored.

### Character based command line
It is similar to the textual command line, except that line editing functions are implemented, including maintaining a cursor, and more control characters are interpreted in order to change the command line. After a postamble is encountered, the command line is sent to the command parser (`ashell-cmd.c`).

### JSON commands
<a name="jsonschema"></a>
The JSON schema of the command structure is the following:
```json
{
  "title": "Ashell command",
  "type": "object",
  "properties": {
    "echo": {
      "type": { "enum": [ "on", "off" ] } },
    },
    "verbose": {
      "type": { "enum": [ "on", "off" ] } },
    },
    "command": {
      "description": "The parsed command part of the command line",
      "type": "string"
    },
    "arguments": {
      "description": "Array with the parsed string arguments",
      "type": "array",
      "minItems": 0,
      "items": { "type": "string" },
      "uniqueItems": false
    },
    "line": {
      "description": "The index of the current line in a multi-line command",
      "type": "number",
      "minimum": 0
    },
    "payload": {  // optional
      "description": "The command line (not parsed)",
      "type": "string",
      "minimum": 1
    },
},
  "required": ["echo", "verbose", "command", "arguments", "line"]
}
```
If in a command object `line` is positive, it means the command is the same as with the previous line, and a command specific prompt needs to be sent back.

## Notes
- WebUSB uses reliable bulk mode USB endpoints, therefore IHEX transfer was removed from this version.
- This version implements only the WebUSB over UART transport, and only the character and line
