/* ABI compatibility for pinned wii-mbedTLS (built against legacy libogc).
 * Recent libogc 3.2 no longer exports gettime() as a public symbol.
 * Wii PowerPC TB counter runs at bus_clock/4; use coherent hi/lo reads.
 * Keep this in a standalone TU so contemporary libogc headers do not
 * collide with the legacy function name. */
#include <stdint.h>
uint64_t gettime(void){
 uint32_t high_a,high_b,low;
 do {
  __asm__ volatile("mftbu %0" : "=r"(high_a));
  __asm__ volatile("mftb %0" : "=r"(low));
  __asm__ volatile("mftbu %0" : "=r"(high_b));
 } while(high_a!=high_b);
 return ((uint64_t)high_a<<32) | (uint64_t)low;
}
