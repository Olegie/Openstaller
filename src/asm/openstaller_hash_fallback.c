#include "openstaller/openstaller.h"

#if defined(OPENSTALLER_USE_ASM)
extern uint64_t os_asm_fnv1a64(const unsigned char *data, size_t size, uint64_t seed);
#endif

uint64_t os_c_fnv1a64(const unsigned char *data, size_t size, uint64_t seed)
{
    const uint64_t prime = 1099511628211ull;
    uint64_t hash = seed;
    size_t index;

    for (index = 0; index < size; ++index) {
        hash ^= (uint64_t)data[index];
        hash *= prime;
    }

    return hash;
}

uint64_t os_hash_bytes(const void *data, size_t size, uint64_t seed)
{
#if defined(OPENSTALLER_USE_ASM)
    return os_asm_fnv1a64((const unsigned char *)data, size, seed);
#else
    return os_c_fnv1a64((const unsigned char *)data, size, seed);
#endif
}

int os_hash_uses_assembly(void)
{
#if defined(OPENSTALLER_USE_ASM)
    return 1;
#else
    return 0;
#endif
}

const char *os_hash_backend_name(void)
{
#if defined(OPENSTALLER_USE_ASM)
    return "asm:x86_64:fnv1a64";
#else
    return "c:fallback:fnv1a64";
#endif
}
