/*
 * Copyright 2009 The Lemon Man and thanks to luccax, Wiipower, Aurelio and crediar
 * Copyright 2010 Dimok
 * Copyright 2011 FIX94, oggzee
 *
 * Original forwarder source by
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 *
 * Based on OpenShopChannel/osc-forwarder (ramblecube, 2021, MIT):
 * https://github.com/OpenShopChannel/osc-forwarder
 *
 * MAIZO adaptation 2026:
 * - replaced the original SD-only path with explicit SD and USB FAT mounting;
 * - changed the executable target to apps/maizo/boot.dol;
 * - added bounded USB retries, DOL limits, diagnostics and no-NAND logging;
 * - retained the proven Open Shop Channel / FIX94 872-byte app_booter.
 * This is NOT a claim of successful real-Wii boot testing.
 */
#include <gccore.h>
#include <ogcsys.h>
#include <ogc/machine/processor.h>
#include <fat.h>
#include <ogc/disc_io.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#define EXECUTE_ADDR ((u8 *)0x92000000u)
#define BOOTER_ADDR ((u8 *)0x93000000u)
#define ARGS_ADDR   ((u8 *)0x93200000u)
#define MAX_DOL (12u*1024u*1024u)
#define MIN_ADDRESS (0x80003f00u)
#define VERSION "MAIZO FIX94-COMPAT FORWARDER v6"

extern const unsigned char osc_app_booter[];
extern const unsigned int osc_app_booter_len;
extern void __exception_setreload(int t);
/* Modern libogc removed __exception_closeall; SYS_SHUTDOWN is retained. */

typedef struct {
 u32 off[18];
 u32 addr[18];
 u32 length[18];
 u32 bss;
 u32 bss_size;
 u32 entry;
} DolHeader;

static char last_error[180];
static int sd_ready = 0, usb_ready = 0;
static char booted_path[96];
/* FIX94/nintendont-autoboot-forwarder calls this after SYS_SHUTDOWN.
 * Modern libogc may no longer export the LWP legacy symbol, so use
 * a weak runtime check. Never invoke the old LWP entry if absent. */
extern void __lwp_thread_stopmultitasking(void (*entry)(void))
 __attribute__((weak));

static void error_path(const char *path, const char *reason) {
 snprintf(last_error,sizeof(last_error),"%s: %s",path,reason);
 printf("  %s\n",last_error);
}
static int overlap(u32 a,u32 n,u32 b,u32 l){
 return n && l && (uint64_t)a < (uint64_t)b+l &&
                    (uint64_t)b < (uint64_t)a+n;
}
static int valid_dol(const u8 *data, unsigned size) {
 if(size<0x100 || size>MAX_DOL)return 0;
 const DolHeader*h=(const DolHeader *)data;
 if(h->entry<MIN_ADDRESS || h->entry>=0x94000000u)return 0;
 int text=0,sections=0;
 for(int i=0;i<18;i++){
  u32 off=h->off[i],addr=h->addr[i],n=h->length[i];
  if(!n)continue;
  if(off<0x100 || off>=size || n>size-off ||
     addr<MIN_ADDRESS || addr>=0x94000000u || n>0x94000000u-addr ||
     overlap(addr,n,0x92000000u,MAX_DOL) ||
     overlap(addr,n,0x93000000u,0x20000u) ||
     overlap(addr,n,0x93200000u,0x10000u))return 0;
  sections++;
  if(i<7 && h->entry>=addr && h->entry<addr+n)text=1;
 }
 if(!sections || !text)return 0;
 if(h->bss_size && (h->bss<MIN_ADDRESS ||
     h->bss>=0x94000000u || h->bss_size>0x94000000u-h->bss ||
     overlap(h->bss,h->bss_size,0x92000000u,MAX_DOL) ||
     overlap(h->bss,h->bss_size,0x93000000u,0x20000u) ||
     overlap(h->bss,h->bss_size,0x93200000u,0x10000u)))return 0;
 return 1;
}

/* Unlike fatInitDefault(), device mounts are explicit and independent.
 * This avoids aborting a USB-only boot when the SD slot is empty. */
static int mount_one(const char*name, const DISC_INTERFACE *disk){
 int started=disk->startup();
 int mounted=started && fatMountSimple(name,disk);
 printf("%s: startup=%d FAT=%d\n",name,started,mounted);
 return mounted;
}
static void save_failure(void){
 const char *folder=sd_ready?"sd:/maizo":(usb_ready?"usb:/maizo":NULL);
 if(!folder)return;
 mkdir(folder,0777);
 char name[48];
 snprintf(name,sizeof(name),"%s/forwarder.log",folder);
 FILE*f=fopen(name,"ab");
 if(!f)return;
 fprintf(f,"%s IOS=%u; %s\n",VERSION,(unsigned)IOS_GetVersion(),
         last_error[0]?last_error:"DOL nao encontrado");
 fclose(f);
 printf("Log salvo: %s\n",name);
}
static int launch_from(const char*path){
 printf("Procurando %s\n",path);
 FILE*f=fopen(path,"rb");
 if(!f){
  char msg[60];snprintf(msg,sizeof(msg),"fopen errno=%d",errno);
  error_path(path,msg);return 0;
 }
 if(fseek(f,0,SEEK_END)){
  error_path(path,"fseek falhou");fclose(f);return 0;
 }
 long n=ftell(f);
 if(n<0x100 || n>(long)MAX_DOL){
  error_path(path,"tamanho DOL fora do limite");
  fclose(f);return 0;
 }
 rewind(f);
 unsigned size=(unsigned)n;
 size_t read=fread(EXECUTE_ADDR,1,size,f);
 fclose(f);
 if(read!=size){
  error_path(path,"leitura incompleta");return 0;
 }
 if(!valid_dol(EXECUTE_ADDR,size)){
  error_path(path,"DOL invalido/entrada sobreposta");return 0;
 }
 if(osc_app_booter_len<100 || osc_app_booter_len>8192){
  error_path(path,"OSC app_booter invalido");return 0;
 }

 printf("DOL validado: %u bytes. Transferindo para OSC booter.\n",size);
 fflush(stdout);
 VIDEO_WaitVSync();

 /* The working FIX94 Nintendont channel passes a valid argc/argv block
  * containing the target DOL path. OSC's original forwarder supplied ZERO
  * argv; preserve the standard HBC-like startup contract for child DOLs.
  * This memory region is outside the validated DOL sections. */
 const size_t path_bytes=strlen(path)+1;
 if(sizeof(struct __argv)+path_bytes>=0x10000u){
  error_path(path,"argv path too long");return 0;
 }
 struct __argv *a=(struct __argv *)ARGS_ADDR;
 memset(ARGS_ADDR,0,0x10000);
 a->argvMagic=ARGV_MAGIC;
 a->commandLine=(char *)ARGS_ADDR+sizeof(struct __argv);
 a->length=(int)path_bytes;
 a->argc=1;
 /* The NUL-separated string is consumed by CRT0 and may be copied by
  * the FIX94 app_booter into the child entrypoint's argv slot. */
 memcpy(a->commandLine,path,path_bytes);
 snprintf(booted_path,sizeof(booted_path),"%s",path);
 memcpy(BOOTER_ADDR,osc_app_booter,osc_app_booter_len);
 DCFlushRange(ARGS_ADDR,0x10000);
 DCFlushRange(BOOTER_ADDR,(osc_app_booter_len+31u)&~31u);
 ICInvalidateRange(BOOTER_ADDR,(osc_app_booter_len+31u)&~31u);
 DCFlushRange(EXECUTE_ADDR,(size+31u)&~31u);

 /* The FIX94 forwarder closes both filesystem AND disc interface before
  * switching execution. Omitting shutdown leaves an IOS USB handle open
  * across the exec; the next DOL then sees an apparently absent drive. */
 if(sd_ready){fatUnmount("sd");sd_ready=0;}
 if(usb_ready){fatUnmount("usb");usb_ready=0;}
 __io_wiisd.shutdown();
 __io_usbstorage.shutdown();

 VIDEO_SetBlack(TRUE);
 VIDEO_Flush();
 VIDEO_WaitVSync();
 SYS_ResetSystem(SYS_SHUTDOWN,0,0);
 /* FIX94's real forwarder uses the LWP stop/jump handoff.
  * Try its legacy entry only when actually linked; the modern fallback
  * uses the OSC code image and no longer touches storage after shutdown. */
 if(__lwp_thread_stopmultitasking){
  __lwp_thread_stopmultitasking((void (*)(void))BOOTER_ADDR);
 }
 u32 level;
 _CPU_ISR_Disable(level);
 ((void (*)(void))BOOTER_ADDR)();
 _CPU_ISR_Restore(level);
 return 1; /* Returning indicates a boot failure in the embedded loader. */
}

int main(void){
 __exception_setreload(0);
 VIDEO_Init();
 GXRModeObj*mode=VIDEO_GetPreferredMode(NULL);
 if(!mode)return 1;
 void*fb=MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
 if(!fb)return 1;
 VIDEO_Configure(mode);
 VIDEO_SetNextFramebuffer(fb);
 VIDEO_SetBlack(FALSE);
 VIDEO_Flush();
 VIDEO_WaitVSync();
 CON_Init(fb,18,18,mode->fbWidth,mode->xfbHeight,mode->fbWidth*2);
 printf("\n%s\n",VERSION);
 printf("Base: Open Shop Channel / FIX94 - IOS=%u\n",
        (unsigned)IOS_GetVersion());
 printf("Sem instalacao ou gravacao NAND.\n\n");
 /* Some channel WADs start under an IOS without USB2. Mount only after
  * attempting to switch to the same IOS58 used by Homebrew Channel. */
 if(IOS_GetVersion()!=58){
  s32 result=IOS_ReloadIOS(58);
  printf("IOS58 reload: %ld; ativo IOS %u\n",(long)result,(unsigned)IOS_GetVersion());
 }
 /* The reported working Nintendont forwarder boots this same Maizo from
  * USB, not a stale SD copy. Prefer the USB application when both exist.
  * Probe the USB device first; never silently execute outdated SD binaries. */
 for(int attempt=1;attempt<=8&&!usb_ready;attempt++){
  printf("USB tentativa %d/8\n",attempt);
  usb_ready=mount_one("usb",&__io_usbstorage);
  if(!usb_ready)usleep(300000);
 }
 sd_ready=mount_one("sd",&__io_wiisd);
 if(usb_ready){
  if(launch_from("usb:/apps/maizo/boot.dol"))return 0;
   if(launch_from("usb:/apps/Maizo/boot.dol"))return 0;
  if(launch_from("usb:/apps/MaizoWii/boot.dol"))return 0;
 }
 if(sd_ready){
  if(launch_from("sd:/apps/maizo/boot.dol"))return 0;
   if(launch_from("sd:/apps/Maizo/boot.dol"))return 0;
  if(launch_from("sd:/apps/MaizoWii/boot.dol"))return 0;
 }
 save_failure();
 printf("\nERRO: nenhum frontend valido em SD ou USB.\n");
 printf("Coloque apps/maizo/boot.dol na raiz do FAT32.\n");
 printf("Se a tela nao mostrar FIX94-COMPAT v6,\n");
 printf("o canal WAD instalado e antigo.\n");
 fflush(stdout);
 for(int i=0;i<300;i++)VIDEO_WaitVSync();
 return 1;
}
