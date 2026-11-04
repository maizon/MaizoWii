#include "app.h"
#include <gccore.h>
#include <ogcsys.h>
#include <ogc/machine/processor.h>
#include <sys/stat.h>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
static const char*paths[3][4]={
 {"usb:/apps/Nintendont/boot.dol","sd:/apps/Nintendont/boot.dol","usb:/maizo/modules/nintendont/boot.dol","sd:/maizo/modules/nintendont/boot.dol"},
 {"usb:/apps/WiiSXRX/boot.dol","sd:/apps/WiiSXRX/boot.dol","usb:/maizo/modules/wiisxrx/boot.dol","sd:/maizo/modules/wiisxrx/boot.dol"},
 {"usb:/apps/WiiMC/boot.dol","sd:/apps/WiiMC/boot.dol","usb:/maizo/modules/wiimc/boot.dol","sd:/maizo/modules/wiimc/boot.dol"}
};
static const char*resolve(System sys){if(sys<0||sys>=SYSTEM_COUNT)return NULL;
 for(int i=0;i<4;i++)if(file_exists(paths[sys][i]))return paths[sys][i];return NULL;}
int module_available(System sys){return resolve(sys)!=NULL;}
const char*module_name(System sys){return sys==SYSTEM_GC?"NINTENDONT":sys==SYSTEM_PS1?"WIISXRX":"WIIMC-SSLC";}
static int save_nincfg(const Game*g,const Config*c) {
 NIN_CFG n;memset(&n,0,sizeof(n));n.Magicbytes=NIN_CFG_MAGIC;n.Version=NIN_CFG_VERSION;
 n.Config=NIN_CFG_AUTO_BOOT|(c->gc_memcard?NIN_CFG_MEMCARDEMU:0)|(c->gc_wide?NIN_CFG_FORCE_WIDE:0)|
 (c->gc_cheats?NIN_CFG_CHEATS:0)|(c->gc_skip_ipl?NIN_CFG_SKIP_IPL:0);
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
 fprintf(f,"MAIZO-LAUNCH/1\nsystem=%d\nfile=%s\nmedia_volume=%u\nps1_interpreter=%u\nps1_frameskip=%u\nps1_audio=%u\n",
 g->system,g->path,c->media_volume,c->ps1_interpreter,c->ps1_frameskip,c->ps1_audio);
 fclose(f);
}
/* Classical in-memory Wii DOL forwarder. DOL code and system cleanup are not
 * identical for all IOS setups. Requires validation on a physical console. */
static int valid_dol(const unsigned char*image,size_t len){
 if(len<sizeof(DolHeader))return 0;const DolHeader*h=(const DolHeader*)image;
 if(h->entry_point<0x80004000u||h->entry_point>=0x94000000u)return 0;
 for(int i=0;i<18;i++){
  u32 off=i<7?h->text_pos[i]:h->data_pos[i-7];
  u32 sz=i<7?h->text_size[i]:h->data_size[i-7];
  u32 addr=i<7?h->text_start[i]:h->data_start[i-7];
  if(!sz)continue;
  if(off>=len||sz>len-off||addr<0x80004000u||addr>=0x94000000u||sz>0x94000000u-addr)return 0;
 }
 return 1;
}
static u32 install_dol(const unsigned char*image){
 const DolHeader*h=(const DolHeader*)image;
 for(int i=0;i<7;i++)if(h->text_size[i]){
  void*dest=(void*)(uintptr_t)h->text_start[i];
  memmove(dest,image+h->text_pos[i],h->text_size[i]);DCFlushRange(dest,h->text_size[i]);ICInvalidateRange(dest,h->text_size[i]);
 }
 for(int i=0;i<11;i++)if(h->data_size[i]){
  void*dest=(void*)(uintptr_t)h->data_start[i];
  memmove(dest,image+h->data_pos[i],h->data_size[i]);DCFlushRange(dest,h->data_size[i]);
 }
 return h->entry_point;
}
int module_launch(const Game*g,const Config*c,char*error,size_t cap){
 const char*exe=resolve(g->system);if(!exe){snprintf(error,cap,"DOL do %s nao encontrado",module_name(g->system));return 0;}
 if(g->system==SYSTEM_GC&&!save_nincfg(g,c)){snprintf(error,cap,"Falha ao gravar nincfg.bin");return 0;}
 save_launch_record(g,c);
 /* WiiSXRX accepts argv[1]=directory argv[2]=filename.
  * WiiMC-SSLC accepts argv[1]=media path and sets AutobootExit. */
 char command[2048];size_t used=0;
 #define APPEND_TEXT(s) do{size_t n=strlen(s)+1;if(n>sizeof(command)-used){snprintf(error,cap,"Parametros excedem limite");return 0;}memcpy(command+used,s,n);used+=n;}while(0)
 APPEND_TEXT(exe);
 if(g->system==SYSTEM_PS1){
  char dir[MAIZO_PATH],game[MAIZO_PATH];snprintf(dir,sizeof(dir),"%s",g->path);
  char*slash=strrchr(dir,'/');if(!slash){snprintf(error,cap,"Caminho invalido");return 0;}
  snprintf(game,sizeof(game),"%s",slash+1);slash[1]=0;APPEND_TEXT(dir);APPEND_TEXT(game);
  char option[64];snprintf(option,sizeof(option),"Core=%u",c->ps1_interpreter);APPEND_TEXT(option);
  snprintf(option,sizeof(option),"Audio=%u",c->ps1_audio);APPEND_TEXT(option);
  snprintf(option,sizeof(option),"SkipFrames=%u",c->ps1_frameskip);APPEND_TEXT(option);
 } else if(g->system==SYSTEM_MEDIA) {
  /* Upstream WiiMC source/wiimc.cpp uses argv[1] as printf format. */
  if(strchr(g->path,'%')){snprintf(error,cap,"Nome com %% inseguro no WiiMC original");return 0;}
  /* WiiMC enumerates FAT mounts as usb1:/ or sd1:/, never usb:/ sd:/.
   * Preserve the media path and insert default partition index 1. */
  char media_path[MAIZO_PATH+2];
  if(!strncmp(g->path,"usb:/",5))
    snprintf(media_path,sizeof(media_path),"usb1:/%s",g->path+5);
  else if(!strncmp(g->path,"sd:/",4))
    snprintf(media_path,sizeof(media_path),"sd1:/%s",g->path+4);
  else {snprintf(error,cap,"Particao de midia nao suportada");return 0;}
  APPEND_TEXT(media_path); /* WiiMC argv[1] */
 }
 #undef APPEND_TEXT
 FILE*f=fopen(exe,"rb");if(!f){snprintf(error,cap,"Nao abre o modulo");return 0;}
 if(fseek(f,0,SEEK_END)!=0){fclose(f);snprintf(error,cap,"Seek falhou");return 0;}
 long size=ftell(f);rewind(f);
 if(size<256||size>12*1024*1024){fclose(f);snprintf(error,cap,"DOL muito grande/invalido");return 0;}
 unsigned char*image=(unsigned char*)0x92000000;
 if(fread(image,1,(size_t)size,f)!=(size_t)size){fclose(f);snprintf(error,cap,"Falha ao ler DOL");return 0;}fclose(f);
 if(!valid_dol(image,(size_t)size)){snprintf(error,cap,"DOL invalido: cabecalho/secoes");return 0;}
 /* Forward argv block after the DOL is loaded. */
 struct __argv *args=(struct __argv*)memalign(32,sizeof(struct __argv));
 char*line=(char*)memalign(32,used+32);
 if(!args||!line){free(args);free(line);snprintf(error,cap,"Sem RAM para parametros");return 0;}
 memset(args,0,sizeof(*args));memcpy(line,command,used);args->argvMagic=ARGV_MAGIC;
 args->commandLine=line;args->length=used;args->argc=g->system==SYSTEM_PS1?6:(g->system==SYSTEM_MEDIA?2:1);
 args->argv=&args->commandLine;args->endARGV=args->argv+1;
 u32 entry=0;
 /* Close libfat/IOS peripherals before new executable takes over. */
 input_shutdown();fatUnmount("sd");fatUnmount("usb");
 entry=install_dol(image);
 if(entry){
  void*dest=(void*)(uintptr_t)(entry+8);memcpy(dest,args,sizeof(*args));DCFlushRange(dest,sizeof(*args));
  VIDEO_WaitVSync();SYS_ResetSystem(SYS_SHUTDOWN,0,0);
  u32 level;_CPU_ISR_Disable(level);
  ((void(*)(void))(uintptr_t)entry)();_CPU_ISR_Restore(level);
 }
 snprintf(error,cap,"Falha ao iniciar motor");
 return 0;
}
