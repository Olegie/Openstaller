OPTION CASEMAP:NONE

.code

PUBLIC os_asm_fnv1a64

; uint64_t os_asm_fnv1a64(const unsigned char *data, size_t size, uint64_t seed)
;
; Windows x64 calling convention:
;   RCX = data
;   RDX = size
;   R8  = seed
os_asm_fnv1a64 PROC
    mov rax, r8
    mov r9, rcx
    mov rcx, rdx
    mov r10, 100000001B3h
    test rcx, rcx
    je done

hash_loop:
    xor al, BYTE PTR [r9]
    imul rax, r10
    inc r9
    dec rcx
    jne hash_loop

done:
    ret
os_asm_fnv1a64 ENDP

END
