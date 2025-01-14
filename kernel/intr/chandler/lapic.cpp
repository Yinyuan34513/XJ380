#include "../../../include/include.h"

void timer() {
    write_serial_string("Interrupt IRQ32");
    ApicEoi();
}
