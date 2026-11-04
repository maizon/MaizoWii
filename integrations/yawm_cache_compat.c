/*
 * libwiidrc 2014 expects a public DCInvalidateRange symbol.
 * New libogc versions inline the operation and do not export the old ABI.
 * Supply the exact PowerPC cache-line invalidation on 32-byte boundaries.
 * Used only by the legacy Wii U DRC compatibility library in YAWM.
 * GPL-2.0-or-later.
 */
#include <stdint.h>

void DCInvalidateRange(void *p, unsigned int size) {
    if (!p || !size) return;
    uintptr_t a = (uintptr_t)p & ~(uintptr_t)31;
    uintptr_t end = ((uintptr_t)p + (uintptr_t)size + 31u) & ~(uintptr_t)31;
    for (; a < end; a += 32u)
        __asm__ volatile ("dcbi 0,%0" : : "r"(a) : "memory");
    __asm__ volatile ("sync" : : : "memory");
}
