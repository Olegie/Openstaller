# C / Assembly ABI

The Assembly layer is exposed through one narrow function:

```c
uint64_t os_asm_fnv1a64(const unsigned char *data, size_t size, uint64_t seed);
```

The C core calls it through:

```c
uint64_t os_hash_bytes(const void *data, size_t size, uint64_t seed);
```

## x86_64 Calling Conventions

Openstaller supports both major x86_64 conventions:

- Windows x64: `RCX`, `RDX`, `R8`
- System V AMD64: `RDI`, `RSI`, `RDX`

The assembly file adapts register intake at compile time and then runs the same
inner byte loop. The public C function remains unchanged across platforms.

## Why This Layer Exists

The identity path is a good Assembly boundary because it is:

- byte-oriented
- deterministic
- isolated from allocation and file-system ownership
- easy to regression-test against the C fallback
- hot enough to matter for large payload trees

The rest of Openstaller intentionally stays in C because directory traversal,
script generation, registry integration, and GUI controls are clearer and safer
there.
