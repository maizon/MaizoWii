/* MAIZO Wii freestanding DOL bootstrap, GPL-3.0-or-later.
 * Called from MEM2 only after full DOL is validated by the parent frontend.
 * Never copy a DOL's sections over the currently executing frontend code. */
#include <gccore.h>
#include <stdint.h>
#define DOL_IMAGE ((const unsigned char *)0x92000000u)
#define ARG_BLOCK ((const struct __argv *)0x93200000u)
typedef struct {
 uint32_t off[18];
 uint32_t addr[18];
 uint32_t size[18];
 uint32_t bss, bss_size, entry;
} Image;

static void copy_bytes(void *to,const void *from,uint32_t n){
 unsigned char *d=(unsigned char *)to;const unsigned char *s=(const unsigned char *)from;
 for(uint32_t i=0;i<n;i++)d[i]=s[i];
}
static void clear_bytes(void *p,uint32_t n){
 unsigned char *d=(unsigned char *)p;
 for(uint32_t i=0;i<n;i++)d[i]=0;
}
static void flush_code(void *p,uint32_t n){
 uintptr_t a=((uintptr_t)p)&~31u;
 uintptr_t e=(((uintptr_t)p)+n+31u)&~31u;
 for(;a<e;a+=32){
  asm volatile("dcbst 0,%0"::"b"(a):"memory");
  asm volatile("icbi 0,%0"::"b"(a):"memory");
 }
 asm volatile("sync; isync":::"memory");
}
void maizo_boot_main(void){
 const Image *h=(const Image *)DOL_IMAGE;
 for(unsigned i=0;i<18;i++){
  if(!h->size[i])continue;
  void *target=(void *)(uintptr_t)h->addr[i];
  copy_bytes(target,DOL_IMAGE+h->off[i],h->size[i]);
  flush_code(target,h->size[i]);
 }
 /* The DOL header reserves BSS separately from file-backed sections.
  * Previous loader left stale frontend state in child global variables. */
 if(h->bss_size){
  clear_bytes((void *)(uintptr_t)h->bss,h->bss_size);
  /* Data-cache visibility matters when returning from libogc shutdown. */
  uintptr_t a=((uintptr_t)h->bss)&~31u;
  uintptr_t end=(((uintptr_t)h->bss)+h->bss_size+31u)&~31u;
  for(;a<end;a+=32)asm volatile("dcbst 0,%0"::"b"(a):"memory");
  asm volatile("sync":::"memory");
 }
 uintptr_t entry=h->entry;
 if(ARG_BLOCK->argvMagic==ARGV_MAGIC){
  void *argv_slot=(void *)(entry+8);
  copy_bytes(argv_slot,ARG_BLOCK,sizeof(struct __argv));
  flush_code(argv_slot,sizeof(struct __argv));
 }
 ((void(*)(void))entry)();
 for(;;)asm volatile("nop");
}
