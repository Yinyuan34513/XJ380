.global _start
.type _start, @function
_start:
    endbr64
    jmp xlibc_start

.size _start, . - _start
