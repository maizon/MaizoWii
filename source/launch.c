#include "app.h"
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
 {"usb:/apps/WiiMC/boot.dol","sd:/apps/WiiMC/boot.dol","usb:/maizo/modules/wiimc/boot.dol","sd:/maizo/modules/wiimc/boot.dol"}
};
static const char*resolve(System sys){if(sys<0||sys>=SYSTEM_COUNT)return NULL;
 for(int i=0;i<4;i++)if(file_exists(paths[sys][i]))return paths[sys][i];return NULL;}
int module_available(System sys){return resolve(sys)!=NULL;}
const char*system_title(System sys){
 switch(sys){
 case SYSTEM_WII:return "NINTENDO WII";case SYSTEM_GC:return "GAMECUBE";
 case SYSTEM_PS1:return "PLAYSTATION 1";case SYSTEM_SNES:return "SUPER NINTENDO";
 case SYSTEM_NES:return "NINTENDO NES";case SYSTEM_GBA:return "GAME BOY ADVANCE";
 case SYSTEM_GB:return "GAME BOY / COLOR";case SYSTEM_MEDIA:return "FILMES E MUSICAS";
 default:return "DESCONHECIDO";
 }
}
const char*module_name(System sys){
 switch(sys){
 case SYSTEM_WII:return "USB LOADER GX";case SYSTEM_GC:return "NINTENDONT";
 case SYSTEM_PS1:return "WIISXRX";case SYSTEM_SNES:return "SNES9X GX";
 case SYSTEM_NES:return "FCE ULTRA GX";case SYSTEM_GBA:case SYSTEM_GB:return "VBA GX";
 case SYSTEM_MEDIA:return "WIIMC-SSLC";default:return "SEM MODULO";
 }
}
static int save_nincfg(const Game*g,const Config*c) {
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
 /* Nintendont expects a path relative to the selected root device. */
 const char *relative=strchr(g->path,':');relative=relative?relative+1:g->path;
 snprintf(n.GamePath,sizeof(n.GamePath),"%s",relative);
 if(g->id[0])memcpy(&n.GameID,g->id,4);
 char dest[32];snprintf(dest,sizeof(dest),"%s:/nincfg.bin",(n.Config&NIN_CFG_USB)?"usb":"sd");
 FILE*f=fopen(dest,"wb");if(!f)return 0;
 int ok=fwrite(&n,1,sizeof(n),f)==sizeof(n);if(fclose(f))ok=0;return ok;
}
static void save_launch_record(const Game*g,const Config*c) {
 char dest[96];snprintf(dest,sizeof(dest),"%s/maizo",(g->path[0]=='s')?"sd:":"usb:");
 mkdir(dest,0777);strncat(dest,"/maizo-launch.txt",sizeof(dest)-strlen(dest)-1);
 FILE*f=fopen(dest,"wb");if(!f)return;
 fprintf(f,"MAIZO-LAUNCH/2\nsystem=%d\nfile=%s\nmodule=%s\nmedia_volume=%u\nps1_interpreter=%u\nps1_frameskip=%u\nps1_audio=%u\n",
 g->system,g->path,module_name(g->system),c->media_volume,c->ps1_interpreter,c->ps1_frameskip,c->ps1_audio);
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
 if(h->entry_point<0x80004000u||h->entry_point>=0x94000000u)return 0;
 for(int i=0;i<18;i++){
  u32 off=i<7?h->text_pos[i]:h->data_pos[i-7];
  u32 sz=i<7?h->text_size[i]:h->data_size[i-7];
  u32 addr=i<7?h->text_start[i]:h->data_start[i-7];
  if(!sz)continue;
  if(off>=len||sz>len-off||addr<0x80004000u||addr>=0x94000000u||sz>0x94000000u-addr)return 0;
  if(overlap(addr,sz,MAIZO_DOL_IMAGE,12*1024*1024u)||
     overlap(addr,sz,MAIZO_BOOT_ADDR,0x20000u)||
     overlap(addr,sz,MAIZO_ARG_ADDR,0x10000u))return 0;
 }
 return 1;
}
int module_launch(const Game*g,const Config*c,char*error,size_t cap){
 const char*exe=resolve(g->system);if(!exe){snprintf(error,cap,"DOL do %s nao encontrado",module_name(g->system));return 0;}
 if(g->system==SYSTEM_PS1){
  const char*e=strrchr(g->path,'.');
  if(e&&(!strcasecmp(e,".chd")||!strcasecmp(e,".m3u"))){
   snprintf(error,cap,"WiiSXRX original nao suporta este formato diretamente");return 0;
  }
 }
 if(g->system==SYSTEM_GC&&!save_nincfg(g,c)){snprintf(error,cap,"Falha ao gravar nincfg.bin");return 0;}
 save_launch_record(g,c);
 /* Original WiiSXRX and WiiMC-SSLC accept argv autoboot; no module source changes here. */
 char command[2048];size_t used=0;
 #define APPEND_TEXT(s) do{size_t n=strlen(s)+1;if(n>sizeof(command)-used){snprintf(error,cap,"Parametros excedem limite");return 0;}memcpy(command+used,s,n);used+=n;}while(0)
 APPEND_TEXT(exe);
 if(g->system==SYSTEM_WII){
  /* USB Loader GX Enhanced recognizes six-character game ID for quick boot.
   * If the ID cannot be identified, open its menu instead. */
  if(strlen(g->id)==6)APPEND_TEXT(g->id);
  char option[64];
  snprintf(option,sizeof(option),"-bootios=%u",c->wii_ios);APPEND_TEXT(option);
  snprintf(option,sizeof(option),"-usbport=%u",c->wii_usb_port);APPEND_TEXT(option);
 }
 if(g->system==SYSTEM_PS1){
  char dir[MAIZO_PATH],game[MAIZO_PATH];snprintf(dir,sizeof(dir),"%s",g->path);
  char*slash=strrchr(dir,'/');if(!slash){snprintf(error,cap,"Caminho invalido");return 0;}
  snprintf(game,sizeof(game),"%s",slash+1);slash[1]=0;APPEND_TEXT(dir);APPEND_TEXT(game);
  char option[64];snprintf(option,sizeof(option),"Core=%u",c->ps1_interpreter);APPEND_TEXT(option);
  snprintf(option,sizeof(option),"Audio=%u",c->ps1_audio);APPEND_TEXT(option);
  snprintf(option,sizeof(option),"SkipFrames=%u",c->ps1_frameskip);APPEND_TEXT(option);
 } else if(g->system==SYSTEM_SNES||g->system==SYSTEM_NES||
           g->system==SYSTEM_GBA||g->system==SYSTEM_GB){
  /* GX autoboot: these upstream mains already call AutoloadGame(argv[1],argv[2]).
   * Passing only argv[0] would silently open their menus. */
  char dir[MAIZO_PATH],name[MAIZO_PATH];
  snprintf(dir,sizeof(dir),"%s",g->path);
  char*slash=strrchr(dir,'/');
  if(!slash||!slash[1]){snprintf(error,cap,"Caminho do jogo invalido");return 0;}
  snprintf(name,sizeof(name),"%s",slash+1);slash[1]=0;
  APPEND_TEXT(dir);APPEND_TEXT(name);
 } else if(g->system==SYSTEM_MEDIA) {
  /* Upstream WiiMC-SSLC uses argv[1] as an unsafe printf format string. */
  if(strchr(g->path,'%')){snprintf(error,cap,"Nome com %% inseguro no WiiMC original");return 0;}
  /* WiiMC enumerates FAT partitions as usb1:/ or sd1:/.
   * The default partition index (1) requires hardware confirmation. */
  char media_path[MAIZO_PATH+2];
  if(!strncmp(g->path,"usb:/",5))
    snprintf(media_path,sizeof(media_path),"usb1:/%s",g->path+5);
  else if(!strncmp(g->path,"sd:/",4))
    snprintf(media_path,sizeof(media_path),"sd1:/%s",g->path+4);
  else {snprintf(error,cap,"Particao de midia nao suportada");return 0;}
  APPEND_TEXT(media_path);
 }
 #undef APPEND_TEXT
 FILE*f=fopen(exe,"rb");if(!f){snprintf(error,cap,"Nao abre o modulo");return 0;}
 if(fseek(f,0,SEEK_END)!=0){fclose(f);snprintf(error,cap,"Seek falhou");return 0;}
 long size=ftell(f);rewind(f);
 if(size<256||size>12*1024*1024){fclose(f);snprintf(error,cap,"DOL muito grande/invalido");return 0;}
 unsigned char*image=(unsigned char*)0x92000000;
 if(fread(image,1,(size_t)size,f)!=(size_t)size){fclose(f);snprintf(error,cap,"Falha ao ler DOL");return 0;}fclose(f);
 if(!valid_dol(image,(size_t)size)){snprintf(error,cap,"DOL invalido: cabecalho/secoes");return 0;}
 /* The stand-alone bootstub executes from MEM2 so its memory copy cannot
  * overwrite the frontend while the child DOL is being installed. */
 if(maizo_bootstub_len==0||maizo_bootstub_len>65536u){
  snprintf(error,cap,"Bootstub ausente/invalido");return 0;
 }
 struct __argv *args=(struct __argv*)(uintptr_t)MAIZO_ARG_ADDR;
 const u32 argv_base=(MAIZO_ARG_ADDR+sizeof(struct __argv)+31u)&~31u;
 char*line=(char*)(uintptr_t)argv_base;
 if(used>0x10000u-(argv_base-MAIZO_ARG_ADDR)){
  snprintf(error,cap,"Argumentos excedem regiao reservada");return 0;
 }
 memset(args,0,sizeof(*args));memcpy(line,command,used);
 args->argvMagic=ARGV_MAGIC;
 args->commandLine=line;args->length=used;
 args->argc=g->system==SYSTEM_PS1?6:(g->system==SYSTEM_WII?(strlen(g->id)==6?4:3):
 (g->system==SYSTEM_MEDIA?2:(g->system==SYSTEM_GC?1:3)));
 args->argv=&args->commandLine;args->endARGV=args->argv+1;
 DCFlushRange(args,(u32)(argv_base-MAIZO_ARG_ADDR+used+31u)&~31u);
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
