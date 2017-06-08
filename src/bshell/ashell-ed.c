
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <atomic.h>
#include <malloc.h>
#include <misc/printk.h>
#include <fs.h>
#include <ctype.h>

#include "ashell-def.h"

#include "../zjs_util.h"

/* Tracking Escape sequences */
enum
{
    ESC_ESC,
    ESC_ANSI,
    ESC_ANSI_FIRST,
    ESC_ANSI_VAL,
    ESC_ANSI_VAL_2
};

static struct {
    char shell_line[ASHELL_MAX_LINE_LEN];
    uint32_t cur;         // cursor position
    uint32_t back;        // position after pressing arrows
    uint32_t ansi_val;
    uint32_t ansi_val_2;
    atomic_t esc_state;
    bool flush_line;
} ed = {
    .cur = 0,
    .back = 0,
    .flush_line = false,
};

static inline void cursor_forward(unsigned int count)
{
    while(count--) {
        PRINT("\x1b[1C");
    }
}

static inline void cursor_backward(unsigned int count)
{
    while(count--) {
        PRINT("\x1b[1D");
    }
}

static inline void cursor_save(void)
{
    PRINT("\x1b[s");
}

static inline void cursor_restore(void)
{
    PRINT("\x1b[u");
}

static void insert_char(char *pos, char c, uint8_t end)
{
    char tmp;

    if (ECHO_MODE()) {
        PRINTCH(c);  /* Echo back to console */
    }

    if (end == 0) {
        *pos = c;
        return;
    }

    tmp = *pos;
    *(pos++) = c;

    cursor_save();

    while (end-- > 0) {
        if (ECHO_MODE()) {
            PRINTCH(tmp);
        }
        c = *pos;
        *(pos++) = tmp;
        tmp = c;
    }

    /* Move cursor back to right place */
    cursor_restore();
}

static void handle_ansi(uint8_t byte)
{
    if (atomic_test_and_clear_bit(&ed.esc_state, ESC_ANSI_FIRST)) {
        if (!isdigit(byte)) {
            ed.ansi_val = 1;
            goto ansi_cmd;
        }

        atomic_set_bit(&ed.esc_state, ESC_ANSI_VAL);
        ed.ansi_val = byte - '0';
        ed.ansi_val_2 = 0;
        return;
    }

    if (atomic_test_bit(&ed.esc_state, ESC_ANSI_VAL)) {
        if (isdigit(byte)) {
            if (atomic_test_bit(&ed.esc_state, ESC_ANSI_VAL_2)) {
                ed.ansi_val_2 *= 10;
                ed.ansi_val_2 += byte - '0';
            } else {
                ed.ansi_val *= 10;
                ed.ansi_val += byte - '0';
            }
            return;
        }

        /* Multi value sequence, e.g. Esc[Line;ColumnH */
        if (byte == ';' &&
            !atomic_test_and_set_bit(&ed.esc_state, ESC_ANSI_VAL_2)) {
            return;
        }

        atomic_clear_bit(&ed.esc_state, ESC_ANSI_VAL);
        atomic_clear_bit(&ed.esc_state, ESC_ANSI_VAL_2);
    }

ansi_cmd:
    switch (byte) {
    case ANSI_BACKWARD:
        if (ed.ansi_val > ed.cur) {
            break;
        }

        ed.back += ed.ansi_val;
        ed.cur -= ed.ansi_val;
        cursor_backward(ed.ansi_val);
        break;
    case ANSI_FORWARD:
        if (ed.ansi_val > ed.back) {
            break;
        }

        ed.back -= ed.ansi_val;
        ed.cur += ed.ansi_val;
        cursor_forward(ed.ansi_val);
        break;
    default:
        break;
    }

    atomic_clear_bit(&ed.esc_state, ESC_ANSI);
}

bool handle_escape(uint8_t byte)
{
    // Handle ANSI escape mode */
    if (atomic_test_bit(&ed.esc_state, ESC_ANSI)) {
        handle_ansi(byte);
        return true;
    }

    // Handle escape mode */
    if (atomic_test_and_clear_bit(&ed.esc_state, ESC_ESC)) {
        switch (byte) {
        case ANSI_ESC:
            atomic_set_bit(&ed.esc_state, ESC_ANSI);
            atomic_set_bit(&ed.esc_state, ESC_ANSI_FIRST);
            break;
        default:
            break;
        }
        return true;
    }
    return false;
}

static void shift_left(char *pos, uint32_t n)
{
    if (n == 0) {
        PRINTCH(' ');
        PRINTCH('\b');
        return;
    }
    cursor_save();
    while(n--) {
        *pos = *(pos + 1);
        PRINTCH(*pos++);
    }
    PRINTCH(' ');  // overwrite the last char
    cursor_restore();
}

/**
 * Issue: a lot of special keys just return 27 (Escape): Del, PgUp, PgDown, arrows, etc
 * Left-Right Arrows work because handled by ANSI sequence
 */
void handle_control_chars(uint8_t byte)
{
    if (!isprint(byte)) {
        switch (byte) {
        case ASCII_DEL:
        case ASCII_BKSP:
            if (ed.cur > 0) {
                // delete one char before ed.cur and shift the rest left
                PRINTCH('\b');
                shift_left(ed.shell_line + --ed.cur, ed.back);
            }
            break;
                // delete one char after ed.cur and shift the rest left
            // case ASCII_DEL:
            //     shift_left(ed.shell_line + ed.cur, ed.back);
            //     break;
        case ASCII_ESC:
            atomic_set_bit(&ed.esc_state, ESC_ESC);
            break;
        case ASCII_CR:
        case ASCII_NL:
            ed.flush_line = true;
            break;
        case ASCII_GROUP_SEP:
            SET_ECHO_MODE(false);
            break;
        case ASCII_RECORD_SEP:
            SET_ECHO_MODE(true);
            break;
        case ASCII_TAB:
            // TODO: autocomplete
            break;
        case ASCII_END_OF_TEXT:
        case ASCII_END_OF_TRANS:
        case ASCII_CANCEL:
        case ASCII_SUBSTITUTE:
            ed.flush_line = true;
        default:
            ed.shell_line[ed.cur++] = byte;
            break;
        }
    }
}

/**
 * If char mode enabled, first process that, then call command line processing
 */
void ashell_process_char(const char *buf, size_t len)
{
    extern void ashell_process_line(char *line, size_t len);
    while (len-- > 0) {
        uint8_t byte = *buf++;

        if (ed.back == ASHELL_MAX_LINE_LEN) {
            ERROR("Line size exceeded \n");
            return;
        }

        if (handle_escape(byte))
            continue;

        handle_control_chars(byte);

        if (ed.flush_line) {
            ed.shell_line[ed.cur + ed.back] = '\0';
            if (ECHO_MODE()) {
                SEND("\r\n", 2);
            }
            uint32_t length = strnlen(ed.shell_line, ASHELL_MAX_LINE_LEN);
            ashell_process_line(ed.shell_line, length);
            ed.cur = ed.back = 0;
            ed.flush_line = false;
        } else
            if (isprint(byte)) {
                /* Ignore characters if there's no more buffer space */
                if (ed.cur < ASHELL_MAX_LINE_LEN - 1) {
                    insert_char(ed.shell_line + ed.cur++, byte, ed.back);
                }
            }
    }
}
