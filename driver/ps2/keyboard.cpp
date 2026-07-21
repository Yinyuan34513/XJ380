#include <proto.hpp>
#include <dlinker.h>
#include <input/evdev.h>
#include <syscall/syscall.h>

extern mouse_dec ms_dec;

struct keyboard_buf kb_fifo;
static volatile bool kb_fifo_ready = false;
static uint8_t kb_synth_buf[KB_BUF_SIZE];
static uint8_t *kb_synth_head = kb_synth_buf;
static uint8_t *kb_synth_tail = kb_synth_buf;
static volatile uint32_t kb_synth_lock = 0;
static bool kb_e0_prefix = false;

#define KB_USB_REPEAT_SLOTS 6
#define KB_USB_REPEAT_DELAY_NS 500000000ULL
#define KB_USB_REPEAT_INTERVAL_NS 25000000ULL

struct keyboard_usb_repeat_slot
{
    bool active;
    uint8_t usage;
    uint8_t value;
    uint64_t next_repeat_ns;
};

static keyboard_usb_repeat_slot kb_usb_repeat[KB_USB_REPEAT_SLOTS];

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

static bool kb_value_repeatable(uint8_t value)
{
    return value == '\b' || value == '\n' || (value >= 32 && value < 127);
}

static void kb_synth_enqueue_locked(uint8_t value)
{
    if (value == 0) return;

    uint8_t *next = kb_synth_head + 1;
    if (next == kb_synth_buf + KB_BUF_SIZE) { next = kb_synth_buf; }

    if (next == kb_synth_tail) { return; }

    *kb_synth_head = value;
    kb_synth_head  = next;
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
    memset(kb_usb_repeat, 0, sizeof(kb_usb_repeat));
    kb_fifo_ready = true;
}

static void kb_synth_enqueue(uint8_t value)
{
    kb_synth_lock_acquire();
    kb_synth_enqueue_locked(value);
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

static void kb_usb_repeat_service()
{
    uint64_t now = nanoTime();
    if (now == 0) return;

    kb_synth_lock_acquire();
    for (size_t i = 0; i < KB_USB_REPEAT_SLOTS; ++i)
    {
        keyboard_usb_repeat_slot *slot = &kb_usb_repeat[i];
        if (!slot->active || now < slot->next_repeat_ns) { continue; }

        kb_synth_enqueue_locked(slot->value);
        slot->next_repeat_ns = now + KB_USB_REPEAT_INTERVAL_NS;
    }
    kb_synth_lock_release();
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
    if (x == 0xe0)
    {
        kb_e0_prefix = true;
        send_eoi();
        return;
    }

    evdev_push_ps2_keyboard_event(x, kb_e0_prefix ? 1 : 0);

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
    if (kb_e0_prefix && x < 0x80)
    {
        switch (x)
        {
        case 0x47: x = KEY_HOME; break;
        case 0x48: x = KEY_UP; break;
        case 0x49: x = KEY_PAGE_UP; break;
        case 0x4b: x = KEY_LEFT; break;
        case 0x4d: x = KEY_RIGHT; break;
        case 0x4f: x = KEY_END; break;
        case 0x50: x = KEY_DOWN; break;
        case 0x51: x = KEY_PAGE_DOWN; break;
        case 0x52: x = KEY_INSERT; break;
        case 0x53: x = KEY_DELETE; break;
        default: x = 0; break;
        }
    }
    kb_e0_prefix = false;
    if (x == 0)
    {
        send_eoi();
        return;
    }
    *kb_fifo.p_head = x;

    kb_fifo.count++;
    kb_fifo.p_head++;
    send_eoi();
}

uint8_t get_keyboard_input()
{
    keyboard_prepare_fifo();
    kb_usb_repeat_service();

    uint8_t synthetic = kb_synth_dequeue();
    if (synthetic != 0) {
        return synthetic;
    }

    if (kb_fifo.p_tail != kb_fifo.p_head)
    {
        // 有没有读的
        uint8_t raw = *kb_fifo.p_tail;
        uint8_t temp = raw >= 128 ? raw : keyboard_code[raw];
        if (raw < 128 && (kb_fifo.shift == 1 || kb_fifo.caps == 1)) { temp = keyboard_code1[raw]; }

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

extern "C" void keyboard_push_input(uint8_t value)
{
    kb_synth_enqueue(value);
}

extern "C" void keyboard_usb_key_event(uint8_t usage, uint8_t value, uint8_t pressed)
{
    keyboard_prepare_fifo();
    evdev_push_usb_keyboard_event(usage, pressed);

    kb_synth_lock_acquire();
    keyboard_usb_repeat_slot *free_slot = NULL;
    keyboard_usb_repeat_slot *match = NULL;

    for (size_t i = 0; i < KB_USB_REPEAT_SLOTS; ++i)
    {
        keyboard_usb_repeat_slot *slot = &kb_usb_repeat[i];
        if (slot->active && slot->usage == usage) { match = slot; }
        else if (!slot->active && free_slot == NULL) { free_slot = slot; }
    }

    if (pressed)
    {
        kb_synth_enqueue_locked(value);
        if (kb_value_repeatable(value))
        {
            keyboard_usb_repeat_slot *slot = match ? match : free_slot;
            if (slot != NULL)
            {
                uint64_t now = nanoTime();
                slot->active = true;
                slot->usage = usage;
                slot->value = value;
                slot->next_repeat_ns = now ? now + KB_USB_REPEAT_DELAY_NS : 0;
            }
        }
    }
    else if (match != NULL)
    {
        match->active = false;
    }

    kb_synth_lock_release();
}

EXPORT_SYMBOL(keyboard_push_input);
EXPORT_SYMBOL(keyboard_usb_key_event);
