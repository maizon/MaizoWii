/*
 * MAIZO Wii minimal SD/USB forwarder, GPL-3.0-or-later.
 * This DOL is for HBC/forwarder testing, not itself a NAND-installable WAD.
 * No title or IOS installation; only launches apps/maizo/boot.dol.
 * Reuses the tested MAIZO MEM2 bootstub. Hardware verification pending.
 */
#include <gccore.h>
#include <ogcsys.h>
#include <ogc/machine/processor.h>
#include <fat.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define DOL_IMAGE 0x92000000u
#define BOOT_ADDR 0x93000000u
#define ARG_ADDR 0x93200000u
#define MAX_DOL (12u*1024u*1024u)
#define MIN_DOL_ADDR 0x80003F00u
static char dol_reason[112];
typedef struct {
 u32 text_pos[7], data_pos[11], text_start[7], data_start[11];
 u32 text_size[7], data_size[11], bss_start, bss_size, entry_point;
} DolHeader;
extern const unsigned char maizo_bootstub[];
extern const unsigned maizo_bootstub_len;
static int overlap(u32 a,u32 n,u32 b,u32 l){
 return n&&l&&(uint64_t)a < (uint64_t)b+l && (uint64_t)b < (uint64_t)a+n;
}
static int validate(const u8 *data, size_t size) {
 if(size<sizeof(DolHeader) || size>MAX_DOL){
  snprintf(dol_reason,sizeof(dol_reason),"tamanho do DOL invalido");
  return 0;
 }
 const DolHeader*h=(const DolHeader*)data;
 if(h->entry_point<MIN_DOL_ADDR || h->entry_point>=0x94000000u){
  snprintf(dol_reason,sizeof(dol_reason),"entry 0x%08lX fora da RAM",
           (unsigned long)h->entry_point);
  return 0;
 }
 int sections=0, entry_in_text=0;
 for(int i=0;i<18;i++){
  u32 pos=i<7?h->text_pos[i]:h->data_pos[i-7];
  u32 addr=i<7?h->text_start[i]:h->data_start[i-7];
  u32 n=i<7?h->text_size[i]:h->data_size[i-7];
  if(!n)continue;
  sections++;
  if(pos<0x100 || pos>=size || n>size-pos ||
     addr<MIN_DOL_ADDR || addr>=0x94000000u || n>0x94000000u-addr ||
     overlap(addr,n,DOL_IMAGE,MAX_DOL) ||
     overlap(addr,n,BOOT_ADDR,0x20000) ||
     overlap(addr,n,ARG_ADDR,0x10000)){
   snprintf(dol_reason,sizeof(dol_reason),"secao %d invalida: offset=%08lX addr=%08lX len=%08lX",
            i,(unsigned long)pos,(unsigned long)addr,(unsigned long)n);
   return 0;
  }
  if(i<7 && h->entry_point>=addr && h->entry_point<addr+n)
   entry_in_text=1;
 }
 if(h->bss_size && (h->bss_start<MIN_DOL_ADDR ||
      h->bss_start>=0x94000000u ||
      h->bss_size>0x94000000u-h->bss_start ||
      overlap(h->bss_start,h->bss_size,DOL_IMAGE,MAX_DOL) ||
      overlap(h->bss_start,h->bss_size,BOOT_ADDR,0x20000u) ||
      overlap(h->bss_start,h->bss_size,ARG_ADDR,0x10000u))){
  snprintf(dol_reason,sizeof(dol_reason),"BSS invadindo regiao reservada/invalida");
  return 0;
 }
 if(!sections || !entry_in_text){
  snprintf(dol_reason,sizeof(dol_reason),"sem secoes de codigo ou entry fora do texto");
  return 0;
 }
 return 1;
}
static int try_file(const char*path) {
 printf("Abrindo %s ...\n",path);
 FILE *f=fopen(path,"rb");
 if(!f){
  printf("  ausente/inacessivel (errno=%d)\n",errno);
  return 0;
 }
 if(fseek(f,0,SEEK_END)){printf("  seek falhou\n");fclose(f);return 0;}
 long len=ftell(f);
 rewind(f);
 if(len<256 || len>(long)MAX_DOL){
  printf("  tamanho invalido: %ld bytes\n",len);
  fclose(f);return 0;
 }
 printf("  encontrado: %ld bytes\n",len);
 u8*image=(u8*)(uintptr_t)DOL_IMAGE;
 size_t got=fread(image,1,(size_t)len,f);
 fclose(f);
 if(got!=(size_t)len){printf("  leitura incompleta: %lu bytes\n",(unsigned long)got);return 0;}
 if(!validate(image,(size_t)len)){
  printf("  DOL rejeitado: %s\n",dol_reason);return 0;
 }
 if(maizo_bootstub_len<16 || maizo_bootstub_len>65536){
  printf("  bootstub ausente ou invalido\n");return 0;
 }
 printf("  DOL validado. Executando frontend...\n");
 memset((void*)(uintptr_t)ARG_ADDR,0,0x10000u);
 DCFlushRange((void*)(uintptr_t)ARG_ADDR,0x10000u);
 memcpy((void*)(uintptr_t)BOOT_ADDR,maizo_bootstub,maizo_bootstub_len);
 DCFlushRange((void*)(uintptr_t)BOOT_ADDR,(maizo_bootstub_len+31u)&~31u);
 ICInvalidateRange((void*)(uintptr_t)BOOT_ADDR,(maizo_bootstub_len+31u)&~31u);
 DCFlushRange(image,(u32)((len+31L)&~31L));
 VIDEO_WaitVSync();
 fatUnmount("sd");fatUnmount("usb");
 SYS_ResetSystem(SYS_SHUTDOWN,0,0);
 u32 level;_CPU_ISR_Disable(level);
 ((void(*)(void))(uintptr_t)BOOT_ADDR)();
 _CPU_ISR_Restore(level);
 return -1;
}
int main(void) {
 VIDEO_Init();
 GXRModeObj *mode=VIDEO_GetPreferredMode(NULL);
 if(!mode)return 1;
 void*fb=MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
 if(!fb)return 1;
 VIDEO_Configure(mode);
 VIDEO_SetNextFramebuffer(fb);
 VIDEO_SetBlack(FALSE);
 VIDEO_Flush();
 VIDEO_WaitVSync();
 CON_Init(fb,20,20,mode->fbWidth,mode->xfbHeight,mode->fbWidth*2);
 printf("\nMAIZO WII FORWARDER (sem instalacao NAND)\n");
 printf("Buscando apps/maizo/boot.dol em SD e USB...\n");
 int mounted=fatInitDefault();
 printf("FAT inicial: %s\n",mounted?"OK":"nenhum volume automatico");
 /* A default FAT failure is not proof that no usable USB volume exists. */
 const char *paths[]={"sd:/apps/maizo/boot.dol","usb:/apps/maizo/boot.dol"};
 for(unsigned i=0;i<2;i++){
  int result=try_file(paths[i]);
  if(result<0)break;
 }
 /* Fallback only: explicit libfat block-device mounts for older IOS/HBC
  * combinations where the automatic mount skipped one medium. */
 printf("Tentando montagem explicita SD/USB...\n");
 if(fatMountSimple("sd",&__io_wiisd))printf("SD montado.\n");
 if(fatMountSimple("usb",&__io_usbstorage))printf("USB montado.\n");
 for(unsigned i=0;i<2;i++){
  int result=try_file(paths[i]);
  if(result<0)break;
 }
 printf("\nNAO FOI POSSIVEL INICIAR O FRONTEND.\n");
 printf("Confirme apps/maizo/boot.dol (DOL valido) no FAT32.\n");
 printf("O forwarder NAO instala um canal nem grava na NAND.\n");
 for(unsigned i=0;i<180;i++)VIDEO_WaitVSync();
 return 1;
}
