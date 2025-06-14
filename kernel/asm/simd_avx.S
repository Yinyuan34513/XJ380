.intel_syntax noprefix
.global set_bitmap_avx2
set_bitmap_avx2:
    vpcmpeqd ymm0, ymm0, ymm0
    
    mov     rax, rsi        
    shr     rax, 5          
    jz      .tail           

.block_loop:
    vmovdqu [rdi], ymm0     
    add     rdi, 32         
    dec     rax             
    jnz     .block_loop     

.tail:
    vzeroupper              
    mov     rax, rsi        
    and     rax, 31         
    jz      .done          
    
    pcmpeqd xmm0, xmm0      
    cmp     rax, 16
    jb      .less_than_16
    movdqu  [rdi], xmm0
    add     rdi, 16
    sub     rax, 16

.less_than_16:
    test    rax, rax
    jz      .done
    mov     rcx, rax
    mov     al, 0xFF
    rep stosb

.done:
    ret