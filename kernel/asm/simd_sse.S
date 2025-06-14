.intel_syntax noprefix
.global set_bitmap_sse
set_bitmap_sse:
    mov     rax, rsi
    shr     rax, 4
    jz      .tail
    
    pcmpeqd xmm0, xmm0
    
.block_loop:
    movdqu  [rdi], xmm0     
    add     rdi, 16         
    dec     rax             
    jnz     .block_loop   

.tail:
    mov     rax, rsi        
    and     rax, 15         
    jz      .done           
    

    mov     rcx, rax
    mov     al, 0xFF        
    rep stosb               

.done:
    ret