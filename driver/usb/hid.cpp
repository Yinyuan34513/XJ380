#include "../../include/include.h"
#include "../../kernel/alloc/page.h"

#define MAX_KBD_EVENT 16

#define HID_REQ_GET_REPORT   0x01
#define HID_REQ_GET_IDLE     0x02
#define HID_REQ_GET_PROTOCOL 0x03
#define HID_REQ_SET_REPORT   0x09
#define HID_REQ_SET_IDLE     0x0A
#define HID_REQ_SET_PROTOCOL 0x0B

#define HID_SET_PROTOCOL_BOOT   0
#define HID_SET_PROTOCOL_REPORT 1

typedef struct _HID_STANDARD_KEYEVENT {
    uint8_t MOD;
    uint8_t RSV;
    uint8_t KEY[6];
} HID_STANDARD_KEYEVENT;

USB_PIPE              *KEYBOARD_PIPE = 0;
HID_STANDARD_KEYEVENT *KEYEVENT0;
HID_STANDARD_KEYEVENT *KEYEVENT1;
uint16_t               KEYPRESS[16];

KEY_EVENT_RING KEY_RING;

uint64_t usb_keyboard_kernel_thread(uint64_t arg) {
    while (true) {
        if (KEYBOARD_PIPE && KEYEVENT0 && KEYEVENT1) {
            // OUTPUTTEXT("USB KEYBOARD EVENT\n");
            // Assume to xHCI pipe
            if (!KEYEVENT1->RSV) {
                KEYEVENT1->RSV = 1;
                for (uint32_t i = 0; i < 6; i++) {
                    if (KEYEVENT0->KEY[i]) {
                        uint8_t  cod = KEYEVENT0->KEY[i];
                        uint8_t  idx = cod >> 4;
                        uint16_t msk = ~(1 << (cod & 0xF));
                        int      up  = 1;
                        for (uint32_t j = 0; (j < 6) && up; j++) {
                            if (cod == KEYEVENT1->KEY[j]) { up = 0; }
                        }
                        if (up) { KEYPRESS[idx] &= msk; }
                    }
                }
                for (uint32_t i = 0; i < 6; i++) {
                    if (KEYEVENT1->KEY[i]) {
                        uint8_t  cod = KEYEVENT1->KEY[i];
                        uint8_t  idx = cod >> 4;
                        uint16_t msk = (1 << (cod & 0xF));
                        if (!(KEYPRESS[idx] & msk)) {
                            // KeyEvent(&KEY_RING, cod);
                            write_serial_string("HID KEYBOARD EVENT: ");
                            write_serial_hex(cod);
                            write_serial_string("\n");
                        }
                        KEYPRESS[idx] |= msk;
                    }
                }
                xmemcpy(KEYEVENT0, KEYEVENT1, sizeof(HID_STANDARD_KEYEVENT));
            }

            XHCI_PIPE *xpipe = (XHCI_PIPE *)KEYBOARD_PIPE;
            if (xpipe->RING.NID == xpipe->RING.EID) {
                int cc = KEYBOARD_PIPE->CTRL->XFER(KEYBOARD_PIPE, 0, KEYEVENT1,
                                                   sizeof(HID_STANDARD_KEYEVENT), 0);
                if (cc != 0) {
                    write_serial_string("HID KEYBOARD XFER ERROR: ");
                    write_serial_hex(cc);
                    write_serial_string("\n");
                    return cc;
                }
            }
        }

        __asm__ __volatile__("sti\n\tpause\n\t");
    }

    return 0;
}

void CreateKeyEventRing(KEY_EVENT_RING *ring) {
    ring->CNT = 4096;
    ring->NID = ring->EID = 0;
}

uint32_t ConfigureHIDKeyboard(USB_COMMON *common, USB_ENDPOINT *epdesc) {
    if (KEYBOARD_PIPE) { return -1; }

    if (epdesc->MPS < sizeof(HID_STANDARD_KEYEVENT) || epdesc->MPS > MAX_KBD_EVENT) {
        write_serial_string("HID: Invalid endpoint max packet size\n");
        return -1;
    }

    USB_DEVICE_REQUEST req;
    req.T  = USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE;
    req.C  = HID_REQ_SET_PROTOCOL;
    req.V  = HID_SET_PROTOCOL_BOOT;
    req.I  = common->IFC->IN;
    req.L  = 0;
    int cc = common->CTRL->XFER(common->PIPE, &req, 0, 0, 0);
    if (cc) {
        write_serial_string("CANNOT SET PROTOCOL ");
        write_serial_hex(cc);
        write_serial_string("\n");
        return cc;
    }

    // SET IDLE 4ms
    req.T = USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE;
    req.C = HID_REQ_SET_IDLE;
    req.V = 1 << 8;
    req.I = 0;
    req.L = 0;
    cc    = common->CTRL->XFER(common->PIPE, &req, 0, 0, 0);
    if (cc) {
        write_serial_string("CANNOT SET IDLE ");
        write_serial_hex(cc);
        write_serial_string("\n");
        return cc;
    }

    KERNEL_MAPPER mapper((uint64_t *)convert_physical_to_virtual((uint64_t)current_task->mm->pgd));

    uint64_t keyevent0_phys = (uint64_t)PAGE_ALLOC.alloc_page();
    uint64_t keyevent1_phys = (uint64_t)PAGE_ALLOC.alloc_page();
    uint64_t keyevent0_virt = (uint64_t)convert_physical_to_virtual(keyevent0_phys);
    uint64_t keyevent1_virt = (uint64_t)convert_physical_to_virtual(keyevent1_phys);

    mapper.map_page(keyevent0_virt, keyevent0_phys, 0x3);
    mapper.map_page(keyevent1_virt, keyevent1_phys, 0x3);
    flush_tlb();

    KEYEVENT0 = (HID_STANDARD_KEYEVENT *)keyevent0_virt;
    KEYEVENT1 = (HID_STANDARD_KEYEVENT *)keyevent1_virt;

    xmemset(KEYEVENT0, 0, sizeof(HID_STANDARD_KEYEVENT));
    xmemset(KEYEVENT1, 0, sizeof(HID_STANDARD_KEYEVENT));
    KEYEVENT1->RSV = 1;
    CreateKeyEventRing(&KEY_RING);

    KEYBOARD_PIPE = common->CTRL->CPIP(common, 0, epdesc);
    if (!KEYBOARD_PIPE) {
        write_serial_string("CANNOT CREATE PIPE\n");
        return -1;
    }

    kernel_thread(usb_keyboard_kernel_thread, 0, CLONE_FILES | CLONE_FS | CLONE_SIGNAL);

    return 0;
}

uint32_t ConfigureHID(USB_COMMON *common) {
    USB_INTERFACE *iface = common->IFC;
    if (iface->IS != USB_INTERFACE_SUBCLASS_BOOT) { return -1; }

    USB_ENDPOINT *epdesc = USBSearchEndpoint(common, USB_ENDPOINT_XFER_INT, USB_DIR_IN);
    if (!epdesc) {
        write_serial_string("HID: No interrupt endpoint found\n");
        return -1;
    }

    if (iface->IP == USB_INTERFACE_PROTOCOL_KEYBOARD) {
        write_serial_string("CONFIGURING HID KEYBOARD\n");
        return ConfigureHIDKeyboard(common, epdesc);
    } else if (iface->IP == USB_INTERFACE_PROTOCOL_MOUSE) {
        return -1;
    }

    return -1;
}
