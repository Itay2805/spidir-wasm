#pragma once

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(*x))

#define ALIGN_UP(x, align) __builtin_align_up(x, align)

#define SIZE_4GB 0x0000000100000000ULL

#define POKE(type, addr) ((struct __attribute__((packed)) { type value; }*)addr)->value

#define CALLOC(type, count) (type*)wasm_host_calloc(sizeof(type), (count))

#define MAX(a, b) \
    ({ \
        typeof(a) __a = a; \
        typeof(b) __b = b; \
        __a > __b ? __a : __b; \
    })
