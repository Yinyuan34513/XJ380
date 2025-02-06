.global _start
.type _start, @function
_start:
    callq xlibc_start

.size _start, . - _start
