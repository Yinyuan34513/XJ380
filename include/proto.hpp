#ifndef _PROTO_H_
#define _PROTO_H_

#define KERNEL_STACK_SIZE 32768  // 任务内核栈地址
#define BIG_USER_STACK    999424 // 用户栈大小，要对齐到页

#include <stdint.h>
#include <graphics/sheet.h>
#include <graphics/window/window.h>
#include <graphics/GOP.hpp>

#define close_interrupt __asm__ volatile("cli" ::: "memory")
#define open_interrupt  __asm__ volatile("sti" ::: "memory")

/*
 *   graphics
 */


 extern const FrameBufferConfig *fbc_addr;

// graphics/draw.cpp
auto operator new(size_t size, void *ptr) -> void *;
auto operator new[](size_t size, void *ptr) -> void *;
auto operator delete(void *ptr, size_t size) -> void;
auto operator delete[](void *ptr, size_t size) -> void;
void rect(const FrameBufferConfig &fbc, int x1, int y1, int x2, int y2, const PixelColor &c);
void dot(const FrameBufferConfig &fbc, int x, int y, const PixelColor &c);

// graphics/draw_sheet.cpp
void draw_point(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, const SHEET_BUFFER &color);
void draw_rect(SHEET_INFO *sht, UINT16 *sheet_id, int x1, int y1, int x2, int y2, const SHEET_BUFFER &color);
void draw_line(SHEET_INFO *sht, UINT16 *sheet_id, int x0, int y0, int x1, int y1, const SHEET_BUFFER &color);

// graphics/desktop.cpp
void init_desktop(SHEET_INFO *sht, UINT16 *sheet_id, UINT16 *dock_sheet_id);
void init_dock(SHEET_INFO *sht, UINT16 *sheet_id);
void init_shortcut_dock(SHEET_INFO *sht, UINT16 *sheet_id);
void draw_logo(SHEET_INFO *sht, UINT16 *sheet_id, int xi, int yi);
void draw_studio_logo(SHEET_INFO *sht, UINT16 *sheet_id, int xi, int yi);

// graphics/sheet.cpp
void init_sheet(const FrameBufferConfig &fbc, SHEET_INFO *sht);
void create_sheet(SHEET_INFO *sht, UINT32 bx, UINT32 by, UINT32 x, UINT32 y, UINT32 type, INT16 index, UINT16 *sheet_id);
UINT32 getBX(SHEET_INFO *sht, UINT16 *sheet_id);
UINT32 getBY(SHEET_INFO *sht, UINT16 *sheet_id);
UINT32 getXsize(SHEET_INFO *sht, UINT16 *sheet_id);
UINT32 getYsize(SHEET_INFO *sht, UINT16 *sheet_id);
UINT16 *found_sheet(SHEET_INFO *sht, UINT32 x, UINT32 y);
UINT16 *found_sheetmb(SHEET_INFO *sht, UINT32 x, UINT32 y);
UINT16 *found_sheet_movable(SHEET_INFO *sht, UINT32 x, UINT32 y);
SHEET *get_sheet(SHEET_INFO *sht, UINT16 *sheet_id);
void setBX(SHEET_INFO *sht, UINT16 *sheet_id, UINT32 num);
void setBY(SHEET_INFO *sht, UINT16 *sheet_id, UINT32 num);
void lift_sheet(SHEET_INFO *sht, UINT16 *sheet_id);
void delete_sheet(SHEET_INFO *sht, UINT16 *sheet_id);
void refresh_sheet(SHEET_INFO *sht);
void refresh_part_sheet(SHEET_INFO *sht, int bex1, int bey1, int bex2, int bey2);
UINT32 get_hor();
UINT32 get_ver();

// graphics/window/windowm.cpp
void init_xwm(XWM_INFO *xwmi);
void create_window(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS **windowls, CHAR8 *title, UINT32 width, UINT32 height);
void change_title(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls, CHAR8 *title);
void draw_text(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls, CHAR8 *title,int x,int y,int size,SHEET_BUFFER color);
void delete_window(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls);
WINDOWLS* sht_found_win(XWM_INFO *xwmi, SHEET_INFO *sht, SHEET *csheet);
/*
 *   font
 */

// font/ttf/ttf.cpp
void print_box_ttf(SHEET_INFO *sht, UINT16 *sheet_id, char *buf, SHEET_BUFFER fc, unsigned x, unsigned y, int size);
void init_ttf();

// font/font.cpp
void WriteAscii(const FrameBufferConfig &fbc, int x, int y, char ch, const PixelColor &c);
void WriteDec(const FrameBufferConfig &fbc, int x, int y, unsigned long long dec, const PixelColor &c);
void WriteString(const FrameBufferConfig &fbc, int x, int y, const char *s, const PixelColor &c);
void WriteHex(const FrameBufferConfig &fbc, int x, int y, unsigned long long hex, const PixelColor &c);
void PrintFont(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, char ch, const SHEET_BUFFER &color);
void PrintString(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, const char *s, const SHEET_BUFFER &color);
void PrintHex(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, unsigned long long hex, const SHEET_BUFFER &color);
void PrintDec(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, unsigned long long dec, const SHEET_BUFFER &color);


// driver/ps2/keyboard.cpp
extern struct keyboard_buf kb_fifo;
extern UINT8 keyboard_code[256];
extern UINT8 keyboard_code1[256];
void keyboard_init();
UINT8 get_keyboard_input();

// driver/ps2/mouse.cpp
void mouse_init();
UINT8 get_Mouse_output();
bool mousedecode(UINT8 data);
int get_mouse_x();
int get_mouse_y();
void draw_mouse(SHEET_INFO *sht, UINT16 *sheet_id);
void move_mouse(SHEET_INFO *sht, UINT16 *sheet_id, int px, int py);

/*
 *
 *  driver/serial/
 * 
 */

// driver/serial/serial_port.cpp
int  init_serial();
void write_serial(char a);
void write_serial_string(const char *str);
void write_serial_dec(unsigned long long dec);
void write_serial_hex(unsigned long long hex);
void write_serial_fmt(const char *fmt, ...);
void write_serial_fmt_heapless(const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
/*
 *
 *  kernel/
 * 
 */

// kernel/wsod.cpp
#ifdef __cplusplus
extern "C" {
#endif
void wsod_unexpect_intrrupt(struct X64_REGS *regs, uint64_t error_code);
void c_device_not_avaliable(struct X64_REGS *regs, uint64_t error_code);
void wsod_divide_error(struct X64_REGS *regs, uint64_t error_code);
void wsod_undefined_opcode(struct X64_REGS *regs, uint64_t error_code);
void wsod_debug(struct X64_REGS *regs, uint64_t error_code);
void wsod_nmi(struct X64_REGS *regs, uint64_t error_code);
void wsod_double_fault(struct X64_REGS *regs, uint64_t error_code);
void wsod_system_kernel_error(struct X64_REGS *regs, uint64_t error_code);
void wsod_general_protection(struct X64_REGS *regs, uint64_t error_code);
void wsod_page_fault(struct X64_REGS *regs, uint64_t error_code);
void default_isr(struct X64_REGS *regs, uint64_t error_code);
#ifdef __cplusplus
}
#endif

/*
 *
 *  kernel/cpu/
 * 
 */

// kernel/cpu/common.cpp
void     outb(uint16_t port, uint8_t value);
uint8_t  inb(uint16_t port);
void     outw(uint16_t port, uint16_t value);
uint16_t inw(uint16_t port);
void     outl(uint16_t port, uint32_t value);
uint32_t inl(uint16_t port);
void     insw(uint16_t port, void *buf, unsigned long n);
void     outsw(uint16_t port, const void *buf, unsigned long n);
void     insl(uint32_t port, void *addr, int cnt);
void     outsl(uint32_t port, const void *addr, int cnt);
void     enable_intr();
void     disable_intr();
void     io_mfence();

// kernel/cpu/cpuid.cpp
void asm_cpuid(uint32_t mop, uint32_t sop, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);
void get_cpu_name(char *c);
bool mtrr_supported();

// kernel/cpu/mtrr.cpp
void mtrr_save();
void mtrr_restore();

// kernel/cpu/delay.cpp
// 读取TSC
static inline uint64_t rdtsc();
void                   delay(uint32_t count);
uint64_t               get_cpu_freq_mhz();
void                   delay_us(uint64_t us);
void                   delay_ms(uint64_t ms);
void                   delay_s(uint64_t s);

/*
 *
 *  kernel/intr/
 * 
 */

// kernel/intr/8259a.cpp
void init_8259A();
void disable_8259A();

// kernel/intr/apic.cpp
void     lapic_write(uint32_t reg, uint64_t value);
uint32_t lapic_read(uint32_t reg);
void     ioapic_mask_all();
void     init_lApic();
void     init_apic(uint64_t MADT0);
uint64_t lapic_id();

// kernel/intr/hpet.cpp
uint64_t nanoTime();
void     init_hpet(uint64_t hpet_ptr);

/*
 *
 *  kernel/pctable/
 * 
 */

// kernel/pctable/idt.cpp
void init_idt();

// kernel/pctable/gdt.cpp
void init_gdt();

/*
 *
 *  kernel/memory/
 * 
 */

// kernel/memory/heap/heap.cpp
#include <mm/alloc/alloc.h>
void init_heap();

#include <mm/memory.h>

// kernel/memory/bitmap.cpp
#include <mm/bitmap.h>

// kernel/memory/frame.cpp
void     init_frame(MEMORY_MAP map);
void     free_frame(uint64_t addr);
uint64_t alloc_frames(size_t count);
void free_frames(uint64_t addr, uint64_t size);

// kernel/memory/hhdm.cpp
void     init_hhdm();
uint64_t get_physical_memory_offset();
void    *phys_to_virt(uint64_t phys_addr);
void    *virt_to_phys(uint64_t virt_addr);
void    *driver_phys_to_virt(uint64_t phys_addr);
void    *driver_virt_to_phys(uint64_t virt_addr);
uint64_t page_virt_to_phys(uint64_t va);

// kernel/memory/page.cpp
#include <mm/page.h>

/*
 *
 *  kernel/smp/
 * 
 */

#include <smp/smp.h>

// kernel/smp/smp.cpp
void init_smp(uint64_t MADT0);
void set_kernel_stack(uint64_t rsp);

void                   send_eoi();
struct PROCESSOR_INFO *get_current_cpu();

#endif