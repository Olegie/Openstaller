.intel_syntax noprefix
.text

#if defined(__APPLE__)
.globl _os_asm_fnv1a64
_os_asm_fnv1a64:
#else
.globl os_asm_fnv1a64
os_asm_fnv1a64:
#endif
    /*
     * uint64_t os_asm_fnv1a64(const unsigned char *data, size_t size, uint64_t seed)
     *
     * This is a first-class Openstaller kernel. The C generator uses this
     * routine for package identity, manifest hashes, and generated uninstall
     * registration identifiers on x86_64 builds.
     */
#if defined(_WIN32)
    mov rax, r8
    mov r9, rcx
    mov rcx, rdx
#else
    mov rax, rdx
    mov r9, rdi
    mov rcx, rsi
#endif
    movabs r10, 1099511628211
    test rcx, rcx
    je .done
.loop:
    xor al, byte ptr [r9]
    imul rax, r10
    inc r9
    dec rcx
    jne .loop
.done:
    ret

#if defined(__linux__) || defined(__FreeBSD__)
.section .note.GNU-stack,"",@progbits
#endif
