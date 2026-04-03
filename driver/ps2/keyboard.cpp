#include <proto.hpp>
#include <dlinker.h>
#include <syscall/syscall.h>
extern mouse_dec ms_dec;

struct keyboard_buf kb_fifo;
static volatile bool kb_fifo_ready = false;
static uint8_t kb_synth_buf[KB_BUF_SIZE];
static uint8_t *kb_synth_head = kb_synth_buf;
static uint8_t *kb_synth_tail = kb_synth_buf;
static volatile uint32_t kb_synth_lock = 0;

static void kb_synth_lock_acquire()
{
    while (__sync_lock_test_and_set(&kb_synth_lock, 1) != 0)
    {
        while (kb_synth_lock != 0) { __asm__ volatile("pause"); }
    }
}

static void kb_synth_lock_release()
{
    __sync_lock_release(&kb_synth_lock);
}

static void keyboard_prepare_fifo()
{
    if (kb_fifo_ready) return;

    kb_fifo.p_head = kb_fifo.buf;
    kb_fifo.p_tail = kb_fifo.buf;
    kb_fifo.count  = 0;
    kb_fifo.ctrl   = false;
    kb_fifo.shift  = false;
    kb_fifo.alt    = false;
    kb_fifo.caps   = false;
    memset(kb_fifo.buf, 0, KB_BUF_SIZE);
    kb_synth_head  = kb_synth_buf;
    kb_synth_tail  = kb_synth_buf;
    memset(kb_synth_buf, 0, sizeof(kb_synth_buf));
    kb_fifo_ready = true;
}

static void kb_synth_enqueue(uint8_t value)
{
    if (value == 0) return;

    kb_synth_lock_acquire();
    uint8_t *next = kb_synth_head + 1;
    if (next == kb_synth_buf + KB_BUF_SIZE) { next = kb_synth_buf; }

    if (next == kb_synth_tail)
    {
        kb_synth_lock_release();
        return;
    }

    *kb_synth_head = value;
    kb_synth_head  = next;
    kb_synth_lock_release();
}

static uint8_t kb_synth_dequeue()
{
    uint8_t value = 0;

    kb_synth_lock_acquire();
    if (kb_synth_tail != kb_synth_head)
    {
        value = *kb_synth_tail;
        kb_synth_tail++;
        if (kb_synth_tail == kb_synth_buf + KB_BUF_SIZE) {
            kb_synth_tail = kb_synth_buf;
        }
    }
    kb_synth_lock_release();
    return value;
}

uint8_t keyboard_code[256] = {
    0, KEY_ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    KEY_TAB, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    KEY_CTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    KEY_SHIFT, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', KEY_SHIFT,
    '*', KEY_ALT, ' ', KEY_CAPS, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    KEY_NUML, KEY_SCROLL, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.', 0, 0, 0, KEY_F11, KEY_F12}; // +0x80 = 释放状态
uint8_t keyboard_code1[256] = {                                                                                          // 按下Shift
    0, KEY_ESC, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    KEY_TAB, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    KEY_CTRL, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~',
    KEY_SHIFT, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', KEY_SHIFT,
    '*', KEY_ALT, ' ', KEY_CAPS, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    KEY_NUML, KEY_SCROLL, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.', 0, 0, 0, KEY_F11, KEY_F12};

extern "C" void c_keyboard_handler(void *regs_ptr, uint64_t error_code)
{
    keyboard_prepare_fifo();

    uint8_t x;
    x = inb(0x60);
    if (x == 0x2a || x == 0x36)
    { // Shift按下
        kb_fifo.shift = 1;
    }
    else if (x == 0x1d)
    { // Ctrl按下
        kb_fifo.ctrl = 1;
    }
    else if (x == 0x3a)
    { // Caps Lock按下
        kb_fifo.caps = kb_fifo.caps ^ 1;
    }
    else if (x == 0xaa || x == 0xb6)
    { // Shift松开
        kb_fifo.shift = 0;
    }
    else if (x == 0x9d)
    { // Ctrl按下
        kb_fifo.ctrl = 0;
    }
    if (x < 0x80)
    {
        // write_serial_string("scan code: ");
        // write_serial_hex(x);
        // write_serial_string("\n");
    }

    if (kb_fifo.p_head == kb_fifo.buf + KB_BUF_SIZE) { kb_fifo.p_head = kb_fifo.buf; }
    if (kb_fifo.count >= KB_BUF_SIZE) {
        kb_fifo.p_tail++;
        if (kb_fifo.p_tail == kb_fifo.buf + KB_BUF_SIZE) { kb_fifo.p_tail = kb_fifo.buf; }
        kb_fifo.count = KB_BUF_SIZE - 1;
    }
    *kb_fifo.p_head = x;

    kb_fifo.count++;
    kb_fifo.p_head++;
    send_eoi();
}

uint8_t get_keyboard_input()
{
    keyboard_prepare_fifo();

    uint8_t synthetic = kb_synth_dequeue();
    if (synthetic != 0) {
        return synthetic;
    }

    if (kb_fifo.p_tail != kb_fifo.p_head)
    {
        // 有没有读的
        uint8_t temp = keyboard_code[*kb_fifo.p_tail];
        if (kb_fifo.shift == 1 || kb_fifo.caps == 1) { temp = keyboard_code1[*kb_fifo.p_tail]; }

        if (kb_fifo.p_tail == kb_fifo.buf + KB_BUF_SIZE) { kb_fifo.p_tail = kb_fifo.buf; }
        kb_fifo.p_tail++;
        return temp;
    }
    return NULL;
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

void keyboard_init()
{
    keyboard_prepare_fifo();
    wait_ps2_write();
    outb(PORT_KB_CMD, KBCMD_WRITE_CMD);
    wait_ps2_read();
    outb(PORT_KB_DATA, KB_INIT_MODE);
}

void keyboard_push_input(uint8_t value)
{
    kb_synth_enqueue(value);
}

EXPORT_SYMBOL(keyboard_push_input);
