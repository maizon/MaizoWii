#include "app.h"
#include <stdio.h>
#include <string.h>

/* Values here correspond to NIN_CFG or WiiSX 4.2.2 OPTIONS argv.
 * The separate calibration screen modifies only the MAIZO frontend UI.
 * Do not imply that arbitrary DOLs consume frontend-only geometry. */
int settings_count(System s){
 switch(s){case SYSTEM_WII:return 6;case SYSTEM_GC:return 14;
 case SYSTEM_PS1:return 15;case SYSTEM_MEDIA:return 7;default:return 4;}
}
const char*settings_label(System s,int row){
 if(row==0)return "TEMA";
 if(row==1)return "MODO DA BIBLIOTECA";
 if(row==settings_count(s)-1)return "VOLTAR";
 if(row==settings_count(s)-2)return "CALIBRAR TELA MAIZO";
 static const char*wii[]={"IOS INICIAL DO LOADER","PORTA USB"};
 static const char*gc[]={"MEMORY CARD EMULADA","WIDESCREEN","CHEATS","SKIP IPL","VIDEO GAMECUBE","MEMCARD MULTI","CONTROLE GC NATIVO","REMOVE LIMIT","LARGURA GC (NINTENDONT)","POSICAO VERTICAL GC"};
 static const char*ps[]={"NUCLEO PS1","CPU (INTERP/DYNAREC)","FRAME SKIP","AUDIO","SAIDA NATIVA 240P","MODO DE VIDEO","PROPORCAO","DEFLICKER","DITHERING","MOSTRAR FPS","LIMITAR FPS"};
 int n=row-2;
 if(s==SYSTEM_WII&&n>=0&&n<2)return wii[n];
 if(s==SYSTEM_GC&&n>=0&&n<10)return gc[n];
 if(s==SYSTEM_PS1&&n>=0&&n<11)return ps[n];
 if(s==SYSTEM_MEDIA&&row==2)return "VOLUME (MODULO PATCHED)";
 if(s==SYSTEM_MEDIA&&row==3)return "WIIMC: VERSAO";
 if(s==SYSTEM_MEDIA&&row==4)return "WIIMC: ABRIR VIDEO";
 return "OPCOES DO MODULO ORIGINAL";
}
static unsigned get(const Config*c,System s,int row){
 if(row==0)return c->theme;if(row==1)return c->layout;
 if(s==SYSTEM_WII)return row==2?c->wii_ios:c->wii_usb_port;
 if(s==SYSTEM_GC)switch(row){
 case 2:return c->gc_memcard;case 3:return c->gc_wide;
 case 4:return c->gc_cheats;case 5:return c->gc_skip_ipl;
 case 6:return c->gc_video;case 7:return c->gc_mc_multi;
 case 8:return c->gc_native_si;case 9:return c->gc_rem_limit;
 case 10:return c->gc_scale;case 11:return c->gc_offset;default:return 0;
 }
 if(s==SYSTEM_PS1)switch(row){
 case 2:return c->ps1_engine;case 3:return c->ps1_interpreter;
 case 4:return c->ps1_frameskip;case 5:return c->ps1_audio;
 case 6:return c->ps1_native;case 7:return c->ps1_video;
 case 8:return c->ps1_aspect;case 9:return c->ps1_deflicker;
 case 10:return c->ps1_dither;case 11:return c->ps1_fps;
 case 12:return c->ps1_limit;default:return 0;
 }
 if(s==SYSTEM_MEDIA&&row==2)return c->media_volume;
 if(s==SYSTEM_MEDIA&&row==3)return c->media_player;
 if(s==SYSTEM_MEDIA&&row==4)return c->media_autoboot;
 return 0;
}
void settings_value(const Config*c,System s,int row,char*dest,size_t cap){
 if(row==settings_count(s)-1){snprintf(dest,cap,"A / B");return;}
 if(row==settings_count(s)-2){snprintf(dest,cap,"ABRIR");return;}
 unsigned v=get(c,s,row);
 if(row==0){const char*themes[]={"VIOLETA","TURQUESA","CORAL","GRAFITE","OLED","CLASSICO"};snprintf(dest,cap,"%s",themes[v%6]);return;}
 if(row==1){const char *layouts[]={"GRADE","LISTA","CARROSSEL"};snprintf(dest,cap,"%s",layouts[v%3]);return;}
 if(s==SYSTEM_WII){if(row==2)snprintf(dest,cap,"IOS %u",v);
  else snprintf(dest,cap,"USB %u",v);return;}
 if(s==SYSTEM_GC){
  if(row==6){snprintf(dest,cap,"%s",v==0?"AUTO":v==1?"PAL-M":v==2?"NTSC":"PAL50");return;}
  if(row==10){if(!v)snprintf(dest,cap,"AUTO");else snprintf(dest,cap,"%u",v);return;}
  if(row==11){snprintf(dest,cap,"%+d",(int)v-20);return;}
 }
 if(s==SYSTEM_PS1){
  if(row==2){snprintf(dest,cap,"%s",v?"WIISX 4.2.2":"WIISXRX");return;}
  if(row==3){snprintf(dest,cap,"%s",v?"INTERPRETADOR":"DYNAREC");return;}
  if(row==7){const char*names[]={"AUTO","NTSC","PAL","480P"};snprintf(dest,cap,"%s",names[v%4]);return;}
  if(row==8){const char*names[]={"4:3","16:9","16:9 BORDAS"};snprintf(dest,cap,"%s",names[v%3]);return;}
  if(row==10){const char*names[]={"NENHUM","AUTOMATICO","SEMPRE"};snprintf(dest,cap,"%s",names[v%3]);return;}
 }
 if(s==SYSTEM_MEDIA&&row==2){snprintf(dest,cap,"%u%%",v);return;}
 if(s==SYSTEM_MEDIA&&row==3){snprintf(dest,cap,"%s",v?"OFICIAL ESTAVEL":"HID EXPERIMENTAL");return;}
 if(s==SYSTEM_MEDIA&&row==4){snprintf(dest,cap,"%s",v?"DIRETO":"NAVEGADOR");return;}
 if(s!=SYSTEM_GC&&s!=SYSTEM_PS1){snprintf(dest,cap,"EXTERNO");return;}
 snprintf(dest,cap,"%s",v?"LIGADO":"DESLIGADO");
}
int settings_change(Config*c,System s,int row,int dir){
 if(row>=settings_count(s)-2||row<0)return 0;
 unsigned*v=NULL,lim=2;
 if(row==0){v=&c->theme;lim=6;}
 else if(row==1){v=&c->layout;lim=3;}
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
 case 10:
  if(dir<0)c->gc_scale=c->gc_scale==0?120:(c->gc_scale<=40?0:c->gc_scale-2);
  else c->gc_scale=c->gc_scale==0?40:(c->gc_scale>=120?0:c->gc_scale+2);
  return 1;
 case 11:c->gc_offset=(c->gc_offset+(dir<0?40:1))%41;return 1;
 }
 else if(s==SYSTEM_PS1)switch(row){
 case 2:v=&c->ps1_engine;break;case 3:v=&c->ps1_interpreter;break;
 case 4:v=&c->ps1_frameskip;break;case 5:v=&c->ps1_audio;break;
 case 6:v=&c->ps1_native;break;case 7:v=&c->ps1_video;lim=4;break;
 case 8:v=&c->ps1_aspect;lim=3;break;case 9:v=&c->ps1_deflicker;break;
 case 10:v=&c->ps1_dither;lim=3;break;case 11:v=&c->ps1_fps;break;
 case 12:v=&c->ps1_limit;break;
 }
 else if(s==SYSTEM_MEDIA&&row==2){
  c->media_volume=dir<0?(c->media_volume>=5?c->media_volume-5:0):(c->media_volume<=95?c->media_volume+5:100);return 1;
 }
 else if(s==SYSTEM_MEDIA&&row==3)v=&c->media_player;
 else if(s==SYSTEM_MEDIA&&row==4)v=&c->media_autoboot;
 if(!v)return 0;
 *v=(*v+lim+(dir<0?-1:1))%lim;return 1;
}

/* Category headers reflect actual supported parameters, not fake sliders. */
const char*settings_category(System s,int row){
 if(row==0||row==1)return "APARENCIA";
 if(row>=settings_count(s)-2)return "GERAL";
 if(s==SYSTEM_WII)return row==2?"EMULACAO / IOS":"ARMAZENAMENTO";
 if(s==SYSTEM_GC){
  if(row==6||row>=10)return "VIDEO";
  if(row==8)return "CONTROLES";
  if(row==2||row==7)return "SAVES";
  if(row==4)return "CHEATS";
  return "EMULACAO";
 }
 if(s==SYSTEM_PS1){
  if(row==5)return "AUDIO";
  if(row>=6)return "VIDEO / QUALIDADE";
  return "EMULACAO";
 }
 if(s==SYSTEM_MEDIA)return row==2?"AUDIO":"REPRODUCAO";
 return "EMULACAO";
}
