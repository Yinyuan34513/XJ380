#include <proto.hpp>
#include <ps2/keyboard.h>
#include <task/pcb.h>
#include <atom_queue.h>

extern tcb_t        kernel_head_task;
extern lock_queue  *pgb_queue;
struct keyboard_buf kb_fifo;

bool ctrled     = false;
bool shifted    = false;
bool capsLocked = false;

char character_table[140] = {
    0, KEY_ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    KEY_TAB, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    KEY_CTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    KEY_SHIFT, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', KEY_SHIFT,
    '*', KEY_ALT, ' ', KEY_CAPS, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    KEY_NUML, KEY_SCROLL, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.', 0, 0, 0, KEY_F11, KEY_F12
};  // +0x80 = 释放状态

char shifted_character_table[140] = {
    0, KEY_ESC, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    KEY_TAB, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    KEY_CTRL, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~',
    KEY_SHIFT, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', KEY_SHIFT,
    '*', KEY_ALT, ' ', KEY_CAPS, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    KEY_NUML, KEY_SCROLL, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.', 0, 0, 0, KEY_F11, KEY_F12
};

char cap_character_table[140] = {
    0, KEY_ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    KEY_TAB, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n',
    KEY_CTRL, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`',
    KEY_SHIFT, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', KEY_SHIFT,
    '*', KEY_ALT, ' ', KEY_CAPS, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    KEY_NUML, KEY_SCROLL, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.', 0, 0, 0, KEY_F11, KEY_F12
};

char shifted_cap_character_table[140] = {
    0, KEY_ESC, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    KEY_TAB, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}', '\n',
    KEY_CTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '\"', '~',
    KEY_SHIFT, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', '<', '>', '?', KEY_SHIFT,
    '*', KEY_ALT, ' ', KEY_CAPS, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    KEY_NUML, KEY_SCROLL, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.', 0, 0, 0, KEY_F11, KEY_F12
};

char *character_array[2][2] = {
    {character_table,     shifted_character_table    },
    {cap_character_table, shifted_cap_character_table}
};

struct keyboard_cmd_state
{
    bool waiting_ack;
    bool got_ack;
    bool got_resend;
} key_cmd_state = {
    .waiting_ack = false,
    .got_ack     = false,
    .got_resend  = false,
};
atom_queue *temp_keyboard_buffer = nullptr;
void keyboard_tmp_writer(const uint8_t *data) {
    if (data[0] == 3) {
        return ;
    }
    while (*data != '\0') {
        atom_push(temp_keyboard_buffer, *data);
        data++;
    }
}

int kernel_getch() {
    char ch;
    if ((ch = atom_pop(temp_keyboard_buffer)) == -1) {
        return NULL;
    }
    return ch;
}

extern "C" void c_keyboard_handler(void *regs_ptr, uint64_t error_code)
{

    uint8_t scancode = inb(0x60);

    if (scancode == 0xfa)
    {
        key_cmd_state.got_ack = true;
        return;
    }

    if (scancode == 0xaa && key_cmd_state.got_ack && !key_cmd_state.got_resend)
    {
        key_cmd_state.got_resend = true;
        return;
    }

    char out = 0;

    if (scancode == 0xE0)
        out = keyboard_scancode(scancode, inb(0x60), 0);
    else if (scancode == 0xE1)
        out = keyboard_scancode(scancode, inb(0x60), inb(0x60));
    else
        out = keyboard_scancode(scancode, 0, 0);

    if (ctrled) out &= 0x1f;
    char c0[2];
    c0[0] = out;
    c0[1] = '\0';
    keyboard_tmp_writer((uint8_t *)c0);
    send_eoi();
}

uint8_t keyboard_scancode(uint8_t scancode, uint8_t scancode_1, uint8_t scancode_2)
{
    if (scancode == 0xE0)
    {
        switch (scancode_1)
        {
        case 0x48: return KEY_BUTTON_UP;
        case 0x50: return KEY_BUTTON_DOWN;
        case 0x4b: return KEY_BUTTON_LEFT;
        case 0x4d: return KEY_BUTTON_RIGHT;
        case 0x47: return KEY_BUTTON_HOME;
        case 0x49: return KEY_BUTTON_PDOWN;
        case 0x4f: return KEY_BUTTON_END;
        case 0x51: return KEY_BUTTON_PUP;
        case 0x52: return KEY_BUTTON_INSERT;
        case 0x53: return KEY_BUTTON_DEL;
        default: return 0;
        }
    }
    if (shifted == 1 && scancode & 0x80)
    {
        if ((scancode & 0x7F) == SCANCODE_SHIFT_L || (scancode & 0x7F) == SCANCODE_SHIFT_R)
        {
            shifted = 0;
            return 0;
        }
    }

    if (ctrled == 1 && scancode & 0x80)
    {
        if ((scancode & 0x7F) == 0x1d)
        {
            ctrled = false;
            return 0;
        }
    }

    if (scancode < sizeof(character_table) && !(scancode & 0x80))
    {
        uint8_t character = character_array[capsLocked][shifted][scancode];

        if (character < 128)
        { // Normal char
            return character;
        }

        switch (scancode)
        {
        case SCANCODE_ENTER: return CHARACTER_ENTER;
        case SCANCODE_BACK: return CHARACTER_BACK;
        case SCANCODE_SHIFT_L:
        case SCANCODE_SHIFT_R: shifted = true; break;
        case 0x1d: ctrled = true; break;
        case SCANCODE_CAPS: capsLocked = !capsLocked; break;
        }
    }

    return 0;
}

void wait_ps2_write()
{
    for (size_t i = 0; i < MAX_WAIT_INDEX; ++i)
    {
        if (!(inb(PS2_CMD_PORT) & KB_STATUS_IBF)) return;
    }
}

void wait_ps2_read()
{
    for (size_t i = 0; i < MAX_WAIT_INDEX; ++i)
    {
        if (!(inb(PS2_CMD_PORT) & KB_STATUS_OBF)) return;
    }
}

void keyboard_setup()
{
    open_interrupt;
    temp_keyboard_buffer = create_atom_queue(256);
    wait_KB_write();
    outb(PORT_KB_CMD, KBCMD_WRITE_CMD);
    wait_KB_read();
    outb(PORT_KB_DATA, KB_INIT_MODE);
    close_interrupt;
}