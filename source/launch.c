#include "app.h"
#include "launch_args.h"
#include "ps1_cue.h"
#include <gccore.h>
#include <ogcsys.h>
#include <ogc/machine/processor.h>
#include <sys/stat.h>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <malloc.h>
#include <stdint.h>
/* Nintendont config layout/constants: fix94/nintendont common/include/CommonConfig.h,
 * GPL-2.0. WiiSXRX argv autoboot: niuus/WiiSXRX Gamecube/GamecubeMain.cpp. */
#define NIN_CFG_MAGIC 0x01070CF6u
#define NIN_CFG_VERSION 0x0000000Bu
#define NIN_CFG_AUTO_BOOT (1u<<7)
#define NIN_CFG_MEMCARDEMU (1u<<3)
#define NIN_CFG_FORCE_WIDE (1u<<5)
#define NIN_CFG_CHEATS (1u<<0)
#define NIN_CFG_SKIP_IPL (1u<<18)
#define NIN_CFG_USB (1u<<10)
#define NIN_CFG_MC_MULTI (1u<<13)
#define NIN_CFG_NATIVE_SI (1u<<14)
#define NIN_CFG_REMLIMIT (1u<<8)
typedef struct {
 u32 Magicbytes,Version,Config,VideoMode,Language;
 char GamePath[255],CheatPath[255];
 u32 MaxPads,GameID;
 u8 MemCardBlocks; s8 VideoScale,VideoOffset;u8 NetworkProfile;
 u32 WiiUGamepadSlot;
} NIN_CFG;
/* Compile-time layout check against fix94/Nintendont CommonConfig.h v0x0B. */
typedef char MaizoNinCfgSizeCheck[(sizeof(NIN_CFG)==548)?1:-1];
typedef struct {
 u32 text_pos[7],data_pos[11],text_start[7],data_start[11];
 u32 text_size[7],data_size[11],bss_start,bss_size,entry_point;
} DolHeader;
static int file_exists(const char*p){FILE*f=fopen(p,"rb");if(!f)return 0;fclose(f);return 1;}
/* Every system has its own executable; emulators are not merged into the frontend. */
static const char*paths[SYSTEM_COUNT][4]={
 {"usb:/apps/usbloader_gx/boot.dol","sd:/apps/usbloader_gx/boot.dol","usb:/maizo/modules/usbloader_gx/boot.dol","sd:/maizo/modules/usbloader_gx/boot.dol"},
 {"usb:/apps/Nintendont/boot.dol","sd:/apps/Nintendont/boot.dol","usb:/maizo/modules/nintendont/boot.dol","sd:/maizo/modules/nintendont/boot.dol"},
 {"usb:/apps/WiiSXRX/boot.dol","sd:/apps/WiiSXRX/boot.dol","usb:/maizo/modules/wiisxrx/boot.dol","sd:/maizo/modules/wiisxrx/boot.dol"},
 {"usb:/apps/snes9xgx/boot.dol","sd:/apps/snes9xgx/boot.dol","usb:/maizo/modules/snes9xgx/boot.dol","sd:/maizo/modules/snes9xgx/boot.dol"},
 {"usb:/apps/fceugx/boot.dol","sd:/apps/fceugx/boot.dol","usb:/maizo/modules/fceugx/boot.dol","sd:/maizo/modules/fceugx/boot.dol"},
 {"usb:/apps/vbagx/boot.dol","sd:/apps/vbagx/boot.dol","usb:/maizo/modules/vbagx/boot.dol","sd:/maizo/modules/vbagx/boot.dol"},
 {"usb:/apps/vbagx/boot.dol","sd:/apps/vbagx/boot.dol","usb:/maizo/modules/vbagx/boot.dol","sd:/maizo/modules/vbagx/boot.dol"},
 {"usb:/apps/WiiMC/boot.dol","sd:/apps/WiiMC/boot.dol","usb:/maizo/modules/wiimc/boot.dol","sd:/maizo/modules/wiimc/boot.dol"},
  {NULL,NULL,NULL,NULL} /* HB path = executable. */
};
static const char*ps1_wiisx_paths[]={
 "usb:/apps/WiiSX/boot.dol","sd:/apps/WiiSX/boot.dol",
 "usb:/apps/wiiSX/boot.dol","sd:/apps/wiiSX/boot.dol",
 "usb:/maizo/modules/wiisx/boot.dol","sd:/maizo/modules/wiisx/boot.dol"
};
static const char*resolve_media_official(void){
 static const char*paths[]={"usb:/apps/WiiMC-official/boot.dol","sd:/apps/WiiMC-official/boot.dol"};
 for(unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);i++)
  if(file_exists(paths[i]))return paths[i];
 return NULL;
}
static const char*resolve_ps1_wiisx(void){
 for(unsigned i=0;i<sizeof(ps1_wiisx_paths)/sizeof(ps1_wiisx_paths[0]);i++)
  if(file_exists(ps1_wiisx_paths[i]))return ps1_wiisx_paths[i];
 return NULL;
}
static const char*resolve(System sys){if(sys<0||sys>=SYSTEM_COUNT||sys==SYSTEM_HB)return NULL;
 for(int i=0;i<4;i++)if(paths[sys][i]&&file_exists(paths[sys][i]))return paths[sys][i];return NULL;}
/* Availability is an explicit snapshot, NOT repeated fopen() operations
 * from every home card on every video frame (FAT/USB can block for ms). */
static unsigned char modules_present[SYSTEM_COUNT];
static int modules_scanned=0;
static int alternate_ps1=0,alternate_media=0;
void module_refresh_available(void){
 for(int i=0;i<SYSTEM_COUNT;i++)
  modules_present[i]=i==SYSTEM_HB?1:resolve((System)i)!=NULL;
 alternate_ps1=resolve_ps1_wiisx()!=NULL;
 alternate_media=resolve_media_official()!=NULL;
 modules_scanned=1;
}
/* Cache the actual selected engine, not just the default. The UI is drawn
 * every video frame: never fopen() modules from a detail panel or home tile. */
int module_variant_available(System sys,const Config*c){
 if(sys<0||sys>=SYSTEM_COUNT)return 0;
 if(!modules_scanned)module_refresh_available();
 if(sys==SYSTEM_PS1&&c&&c->ps1_engine)return alternate_ps1;
 if(sys==SYSTEM_MEDIA&&c&&c->media_player)return alternate_media;
 return modules_present[sys];
}
int module_available(System sys){
 if(sys<0||sys>=SYSTEM_COUNT)return 0;
 if(!modules_scanned)module_refresh_available();
 if(sys==SYSTEM_PS1)return modules_present[sys]||alternate_ps1;
 if(sys==SYSTEM_MEDIA)return modules_present[sys]||alternate_media;
 return modules_present[sys];
}
const char*system_title(System sys){
 switch(sys){
 case SYSTEM_WII:return "NINTENDO WII";case SYSTEM_GC:return "GAMECUBE";
 case SYSTEM_PS1:return "PLAYSTATION 1";case SYSTEM_SNES:return "SUPER NINTENDO";
 case SYSTEM_NES:return "NINTENDO NES";case SYSTEM_GBA:return "GAME BOY ADVANCE";
 case SYSTEM_GB:return "GAME BOY / COLOR";case SYSTEM_MEDIA:return "FILMES E MUSICAS";case SYSTEM_HB:return "HOMEBREWS";
 default:return "DESCONHECIDO";
 }
}
const char*module_name(System sys){
 switch(sys){
 case SYSTEM_WII:return "USB LOADER GX";case SYSTEM_GC:return "NINTENDONT";
 case SYSTEM_PS1:return "WIISXRX / WIISX 4.2.2";case SYSTEM_SNES:return "SNES9X GX";
 case SYSTEM_NES:return "FCE ULTRA GX";case SYSTEM_GBA:case SYSTEM_GB:return "VBA GX";
 case SYSTEM_MEDIA:return "WIIMC-SSLC";case SYSTEM_HB:return "HOMEBREW CHANNEL";default:return "SEM MODULO";
 }
}
static int save_nincfg(const Game*g,const Config*c,NIN_CFG *out) {
 NIN_CFG n;memset(&n,0,sizeof(n));n.Magicbytes=NIN_CFG_MAGIC;n.Version=NIN_CFG_VERSION;
 n.Config=NIN_CFG_AUTO_BOOT|(c->gc_memcard?NIN_CFG_MEMCARDEMU:0)|(c->gc_wide?NIN_CFG_FORCE_WIDE:0)|
 (c->gc_cheats?NIN_CFG_CHEATS:0)|(c->gc_skip_ipl?NIN_CFG_SKIP_IPL:0)|
 (c->gc_mc_multi?NIN_CFG_MC_MULTI:0)|(c->gc_native_si?NIN_CFG_NATIVE_SI:0)|
 (c->gc_rem_limit?NIN_CFG_REMLIMIT:0);
 if(!strncmp(g->path,"usb:",4))n.Config|=NIN_CFG_USB;
 if(c->gc_video==1)n.VideoMode=0x00010000u|8u; /* force MPAL */
 if(c->gc_video==2)n.VideoMode=0x00010000u|4u; /* force NTSC */
 if(c->gc_video==3)n.VideoMode=0x00010000u|1u; /* force PAL50 */
 n.Language=0xFFFFFFFFu;n.MaxPads=4;n.MemCardBlocks=2;
 /* Nintendont's kernel/Patch.c applies VideoScale as 600+value pixels:
  * native range 640..720; 0 preserves the game's automatic mode. The
  * calibrated frontend width is inherited when the user enlarges it.
  * Shrinking below 640 is NOT supported by this Nintendont mechanism. */
 unsigned width_pct=c->video_w*c->game_w480/100u;
 int scale=c->force_vi704?104:((c->gc_scale>=40&&c->gc_scale<=120)?(int)c->gc_scale:0);
 if(!scale&&width_pct>100u){
  scale=40+(int)(width_pct-100u)*4;
  if(scale>120)scale=120;
  if(scale&1)scale--;
 }
 n.VideoScale=(s8)scale;
 /* VideoOffset is HORIZONTAL in Nintendont, not vertical. Combine
  * frontend, individual game and Nintendont offsets without overflow. */
 int x=(int)c->video_x-50+(int)c->game_x480-50+
       (int)(c->gc_offset<=40?c->gc_offset:20)-20;
 if(x< -20)x=-20;if(x>20)x=20;
 n.VideoOffset=(s8)x;
 /* Nintendont expects a path relative to the selected root device. */
 if(!maizo_gc_relative_path(g->path,n.GamePath,sizeof(n.GamePath)))return 0;
 if(g->id[0])memcpy(&n.GameID,g->id,4);
 if(out)*out=n;
 char dest[32];snprintf(dest,sizeof(dest),"%s:/nincfg.bin",(n.Config&NIN_CFG_USB)?"usb":"sd");
 char tmp[48],backup[48];snprintf(tmp,sizeof(tmp),"%s.tmp",dest);
 snprintf(backup,sizeof(backup),"%s.bak",dest);
 FILE*f=fopen(tmp,"wb");if(!f)return 0;
 int ok=fwrite(&n,1,sizeof(n),f)==sizeof(n);if(fclose(f))ok=0;
 if(!ok){remove(tmp);return 0;}
 if(rename(tmp,dest)==0)return 1;
 remove(backup);
 if(rename(dest,backup)!=0){remove(tmp);return 0;}
 if(rename(tmp,dest)==0){remove(backup);return 1;}
 rename(backup,dest);remove(tmp);return 0;
}
static void save_launch_record(const Game*g,const Config*c) {
 char dest[96];snprintf(dest,sizeof(dest),"%s/maizo",(g->path[0]=='s')?"sd:":"usb:");
 mkdir(dest,0777);strncat(dest,"/maizo-launch.txt",sizeof(dest)-strlen(dest)-1);
 FILE*f=fopen(dest,"wb");if(!f)return;
 /* The frontend calibration is the baseline for games. Per-game mode
  * corrections are added, but frontend-only TV standard is never exported.
  * Clamp for malformed legacy settings before the game's renderer reads it. */
 /* Apply the global CRT calibration ONCE as the baseline for each game's
  * 240p/480i profile. The adapted module reads these values before drawing. */
 int x240=(int)c->video_x-50+(int)c->game_x240-50;
 int x480=(int)c->video_x-50+(int)c->game_x480-50;
 int y=(int)c->video_y-50;
 unsigned w240=c->video_w*c->game_w240/100u;
 unsigned w480=c->video_w*c->game_w480/100u;
 unsigned h=c->video_h;
 if(y< -50)y=-50;if(y>50)y=50;
 if(h<70)h=70;if(h>130)h=130;
 if(x240< -50)x240=-50;if(x240>50)x240=50;
 if(x480< -50)x480=-50;if(x480>50)x480=50;
 if(w240<70)w240=70;if(w240>130)w240=130;
 if(w480<70)w480=70;if(w480>130)w480=130;
 fprintf(f,"MAIZO-LAUNCH/3\nsystem=%d\nfile=%s\nmodule=%s\n"
 "media_volume=%u\nps1_interpreter=%u\nps1_frameskip=%u\n"
 "ps1_audio=%u\nps1_engine=%u\nps1_native=%u\n"
 "game_x240=%d\ngame_x480=%d\ngame_y240=%d\ngame_y480=%d\n"
 "game_w240=%u\ngame_w480=%u\ngame_h240=%u\ngame_h480=%u\nforce_vi704=%u\n",
 g->system,g->path,module_name(g->system),
 c->media_volume,c->ps1_interpreter,c->ps1_frameskip,c->ps1_audio,c->ps1_engine,c->ps1_native,
 x240,x480,y,y,w240,w480,h,h,(c->force_vi704&&g->system!=SYSTEM_MEDIA)?1u:0u);
 if(g->system==SYSTEM_MEDIA&&c->wiimc_apply){
  fprintf(f,"wiimc_apply=1\n");
  for(unsigned i=0;i<MAIZO_WIIMC_OPTION_COUNT;i++)
   fprintf(f,"wiimc_%s=%u\n",maizo_wiimc_options[i].key,c->wiimc_opts[i]);
 }
 fclose(f);
}
/* MEM2 bootstrap: do not copy target DOL sections over the running frontend.
 * Physical Wii testing and correct IOS selection remain necessary. */
extern const unsigned char maizo_bootstub[];
extern const unsigned maizo_bootstub_len;
#define MAIZO_DOL_IMAGE 0x92000000u
#define MAIZO_BOOT_ADDR 0x93000000u
#define MAIZO_ARG_ADDR 0x93200000u
static int overlap(u32 a,u32 n,u32 b,u32 len){
 if(!n||!len)return 0;
 return (uint64_t)a<(uint64_t)b+len && (uint64_t)b<(uint64_t)a+n;
}

static int valid_dol(const unsigned char*image,size_t len){
 if(len<sizeof(DolHeader))return 0;const DolHeader*h=(const DolHeader*)image;
 /* Modern devkitPro DOLs (including our frontend and adapted SNES9x GX)
  * start at 0x80003F00, not 0x80004000. Verified against build #166. */
 if(h->entry_point<0x80003F00u||h->entry_point>=0x94000000u)return 0;
 int sections=0,entry_in_text=0;
 for(int i=0;i<18;i++){
  u32 off=i<7?h->text_pos[i]:h->data_pos[i-7];
  u32 sz=i<7?h->text_size[i]:h->data_size[i-7];
  u32 addr=i<7?h->text_start[i]:h->data_start[i-7];
  if(!sz)continue;
  sections++;
  if(off<0x100u||off>=len||sz>len-off||addr<0x80003F00u||addr>=0x94000000u||sz>0x94000000u-addr)return 0;
  if(i<7 && h->entry_point>=addr && h->entry_point<addr+sz)entry_in_text=1;
  if(overlap(addr,sz,MAIZO_DOL_IMAGE,12*1024*1024u)||
     overlap(addr,sz,MAIZO_BOOT_ADDR,0x20000u)||
     overlap(addr,sz,MAIZO_ARG_ADDR,0x10000u))return 0;
 }
 if(h->bss_size &&
   (h->bss_start<0x80003F00u||h->bss_start>=0x94000000u||
    h->bss_size>0x94000000u-h->bss_start||
    overlap(h->bss_start,h->bss_size,MAIZO_DOL_IMAGE,12*1024*1024u)||
    overlap(h->bss_start,h->bss_size,MAIZO_BOOT_ADDR,0x20000u)||
    overlap(h->bss_start,h->bss_size,MAIZO_ARG_ADDR,0x10000u)))return 0;
 return sections>0 && entry_in_text;
}
int module_launch(const Game*g,const Config*c,char*error,size_t cap){
 if(!g||!c||!error||!cap)return 0;
 error[0]=0;
 const char*exe=g->system==SYSTEM_HB?g->path:
                (g->system==SYSTEM_PS1&&c->ps1_engine)?resolve_ps1_wiisx():
                (g->system==SYSTEM_MEDIA&&c->media_player)?resolve_media_official():resolve(g->system);if(!exe){snprintf(error,cap,"DOL do %s nao encontrado",module_name(g->system));return 0;}
 if(g->system==SYSTEM_PS1){
  const char*e=strrchr(g->path,'.');
  if(e&&(!strcasecmp(e,".chd")||!strcasecmp(e,".m3u"))){
   snprintf(error,cap,"Formato PS1 nao validado para autoboot. Use BIN/CUE ou ISO");return 0;
  }
 }
 /* Fail before reboot when CUE tracks/PS1 image are missing or malformed. */
 if(g->system==SYSTEM_PS1 && !maizo_probe_ps1_image(g->path,error,cap))return 0;
 /* Construct arguments with the exact same portable code exercised in CI.
  * Nintendont additionally consumes the raw binary NIN_CFG as argv[1]. */
 MaizoLaunchSpec spec={0};
 spec.system=(int)g->system;spec.executable=exe;spec.path=g->path;spec.id=g->id;
 spec.wii_ios=c->wii_ios;spec.wii_usb_port=c->wii_usb_port;
 spec.ps1_interpreter=c->ps1_interpreter;spec.ps1_frameskip=c->ps1_frameskip;
 spec.ps1_audio=c->ps1_audio;
 spec.ps1_engine=c->ps1_engine;spec.ps1_native=c->ps1_native;
 spec.ps1_video=c->ps1_video;spec.ps1_aspect=c->ps1_aspect;
 spec.ps1_deflicker=c->ps1_deflicker;spec.ps1_dither=c->ps1_dither;
 spec.ps1_fps=c->ps1_fps;spec.ps1_limit=c->ps1_limit;
 spec.media_autoboot=c->media_autoboot;
 MaizoLaunchArgs command;
 if(!maizo_build_args(&spec,&command,error,cap))return 0;
 FILE*f=fopen(exe,"rb");if(!f){snprintf(error,cap,"Nao abre o modulo");return 0;}
 if(fseek(f,0,SEEK_END)!=0){fclose(f);snprintf(error,cap,"Seek falhou");return 0;}
 long size=ftell(f);rewind(f);
 if(size<256||size>12*1024*1024){fclose(f);snprintf(error,cap,"DOL muito grande/invalido");return 0;}
 unsigned char*image=(unsigned char*)0x92000000;
 if(fread(image,1,(size_t)size,f)!=(size_t)size){fclose(f);snprintf(error,cap,"Falha ao ler DOL");return 0;}fclose(f);
 if(!valid_dol(image,(size_t)size)){snprintf(error,cap,"DOL invalido: cabecalho/secoes");return 0;}
 if(g->system==SYSTEM_GC){
  NIN_CFG nin;
  /* argv[1] carries the complete NIN_CFG directly (Nintendont main.c).
   * The root nincfg.bin is a compatibility cache, not a prerequisite:
   * previous code aborted direct boot on USB write/rename failures. */
  (void)save_nincfg(g,c,&nin);
  /* Nintendont loader/source/main.c reads argv[1] as the NIN_CFG
   * binary. nincfg.bin alone does not provide reliable direct boot. */
  const size_t count=sizeof(nin)+1;
  if(command.length+count>sizeof(command.data)){
   snprintf(error,cap,"Autoboot excede argv");return 0;
  }
  memcpy(command.data+command.length,&nin,sizeof(nin));
  command.data[command.length+sizeof(nin)]=0;
  command.length+=count;command.argc++;
 }
 save_launch_record(g,c);
 /* The stand-alone bootstub executes from MEM2 so its memory copy cannot
  * overwrite the frontend while the child DOL is being installed. */
 if(maizo_bootstub_len==0||maizo_bootstub_len>65536u){
  snprintf(error,cap,"Bootstub ausente/invalido");return 0;
 }
 struct __argv *args=(struct __argv*)(uintptr_t)MAIZO_ARG_ADDR;
 const u32 argv_base=(MAIZO_ARG_ADDR+sizeof(struct __argv)+31u)&~31u;
 char*line=(char*)(uintptr_t)argv_base;
 if(command.length>0x10000u-(argv_base-MAIZO_ARG_ADDR)){
  snprintf(error,cap,"Argumentos excedem regiao reservada");return 0;
 }
 memset(args,0,sizeof(*args));memcpy(line,command.data,command.length);
 args->argvMagic=ARGV_MAGIC;
 args->commandLine=line;args->length=command.length;
 args->argc=command.argc;
 args->argv=&args->commandLine;args->endARGV=args->argv+1;
 DCFlushRange(args,(u32)(argv_base-MAIZO_ARG_ADDR+command.length+31u)&~31u);
 memcpy((void*)(uintptr_t)MAIZO_BOOT_ADDR,maizo_bootstub,maizo_bootstub_len);
 DCFlushRange((void*)(uintptr_t)MAIZO_BOOT_ADDR,(maizo_bootstub_len+31u)&~31u);
 ICInvalidateRange((void*)(uintptr_t)MAIZO_BOOT_ADDR,(maizo_bootstub_len+31u)&~31u);
 DCFlushRange(image,(u32)((size+31L)&~31L));
 /* Preserve the IOS/HBC privileges as far as the standard libogc handoff
  * allows. USB and FAT must be shut down before the child reinitializes. */
 VIDEO_WaitVSync();
 input_shutdown();
 fatUnmount("sd");fatUnmount("usb");
 SYS_ResetSystem(SYS_SHUTDOWN,0,0);
 u32 level;_CPU_ISR_Disable(level);
 ((void(*)(void))(uintptr_t)MAIZO_BOOT_ADDR)();
 _CPU_ISR_Restore(level);
 snprintf(error,cap,"Bootstub retornou inesperadamente");return 0;
}
