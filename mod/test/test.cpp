extern "C"
int write_serial_fmt(const char *fmt, ...);

extern "C"
__attribute__((used)) __attribute__((visibility("default"))) int dlstart(void) {
    return 0;
}

extern "C"
__attribute__((used)) __attribute__((visibility("default"))) int dlmain(void) {
    write_serial_fmt("SEND BY MOD\n");
    return 0;
}