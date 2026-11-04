#include "app.h"
#include <stdio.h>
#include <string.h>

/* Controls exposed here are backed by verified module arguments or NIN_CFG.
 * An unpatched emulator's internal menu is not a MAIZO-owned setting. */
int settings_count(System s){
 switch(s){case SYSTEM_WII:return 5;case SYSTEM_GC:return 11;
 case SYSTEM_PS1:return 6;case SYSTEM_MEDIA:return 4;default:return 4;}
}
const char*settings_label(System s,int row){
 if(row==0)return "TEMA";if(row==1)return "MODO DA BIBLIOTECA";
 if(row==settings_count(s)-1)return "VOLTAR";
 static const char*wii[]={"IOS INICIAL DO LOADER","PORTA USB"};
 static const char*gc[]={"MEMORY CARD EMULADA","WIDESCREEN","CHEATS","SKIP IPL","VIDEO GAMECUBE","MEMCARD MULTI","CONTROLE GC NATIVO","REMOVE LIMIT"};
 static const char*ps[]={"NUCLEO PS1","FRAME SKIP","AUDIO"};
 int n=row-2;
 if(s==SYSTEM_WII&&n>=0&&n<2)return wii[n];
 if(s==SYSTEM_GC&&n>=0&&n<8)return gc[n];
 if(s==SYSTEM_PS1&&n>=0&&n<3)return ps[n];
 if(s==SYSTEM_MEDIA&&row==2)return "VOLUME (MODULO PATCHED)";
 return "OPCOES NO MENU DO EMULADOR";
}
static unsigned get(const Config*c,System s,int row){
 if(row==0)return c->theme;if(row==1)return c->layout;
 if(s==SYSTEM_WII)return row==2?c->wii_ios:c->wii_usb_port;
 if(s==SYSTEM_GC)switch(row){
 case 2:return c->gc_memcard;case 3:return c->gc_wide;
 case 4:return c->gc_cheats;case 5:return c->gc_skip_ipl;
 case 6:return c->gc_video;case 7:return c->gc_mc_multi;
 case 8:return c->gc_native_si;case 9:return c->gc_rem_limit;default:return 0;
 }
 if(s==SYSTEM_PS1)switch(row){
 case 2:return c->ps1_interpreter;case 3:return c->ps1_frameskip;
 case 4:return c->ps1_audio;default:return 0;}
 if(s==SYSTEM_MEDIA&&row==2)return c->media_volume;
 return 0;
}
void settings_value(const Config*c,System s,int row,char*dest,size_t cap){
 if(row==settings_count(s)-1){snprintf(dest,cap,"A / B");return;}
 unsigned v=get(c,s,row);
 if(row==0){snprintf(dest,cap,"%s",v==0?"VIOLETA":v==1?"TURQUESA":"CORAL");return;}
 if(row==1){snprintf(dest,cap,"%s",v?"LISTA":"GRADE");return;}
 if(s==SYSTEM_WII){if(row==2)snprintf(dest,cap,"IOS %u",v);
  else snprintf(dest,cap,"USB %u",v);return;}
 if(s==SYSTEM_GC&&row==6){snprintf(dest,cap,"%s",v==0?"AUTO":v==1?"PAL-M":v==2?"NTSC":"PAL50");return;}
 if(s==SYSTEM_PS1&&row==2){snprintf(dest,cap,"%s",v?"INTERPRETADOR":"DYNAREC");return;}
 if(s==SYSTEM_MEDIA&&row==2){snprintf(dest,cap,"%u%%",v);return;}
 if(s!=SYSTEM_GC&&s!=SYSTEM_PS1){snprintf(dest,cap,"EXTERNO");return;}
 snprintf(dest,cap,"%s",v?"LIGADO":"DESLIGADO");
}
int settings_change(Config*c,System s,int row,int dir){
 if(row>=settings_count(s)-1||row<0)return 0;
 unsigned*v=NULL,lim=2;
 if(row==0){v=&c->theme;lim=3;}
 else if(row==1)v=&c->layout;
 else if(s==SYSTEM_WII&&row==2){
  const unsigned ios[]={58,249,250,251,252};int n=1;
  for(int i=0;i<5;i++)if(ios[i]==c->wii_ios){n=i;break;}
  c->wii_ios=ios[(n+5+(dir<0?-1:1))%5];return 1;
 }
 else if(s==SYSTEM_WII&&row==3)v=&c->wii_usb_port;
 else if(s==SYSTEM_GC)switch(row){
 case 2:v=&c->gc_memcard;break;case 3:v=&c->gc_wide;break;
 case 4:v=&c->gc_cheats;break;case 5:v=&c->gc_skip_ipl;break;
 case 6:v=&c->gc_video;lim=4;break;case 7:v=&c->gc_mc_multi;break;
 case 8:v=&c->gc_native_si;break;case 9:v=&c->gc_rem_limit;break;
 }
 else if(s==SYSTEM_PS1)switch(row){
 case 2:v=&c->ps1_interpreter;break;case 3:v=&c->ps1_frameskip;break;
 case 4:v=&c->ps1_audio;break;
 }
 else if(s==SYSTEM_MEDIA&&row==2){
  c->media_volume=dir<0?(c->media_volume>=5?c->media_volume-5:0):(c->media_volume<=95?c->media_volume+5:100);return 1;
 }
 if(!v)return 0;
 *v=(*v+lim+(dir<0?-1:1))%lim;return 1;
}
