#include "app.h"
#include <stdio.h>
#include <string.h>
const MaizoWiiMCOption maizo_wiimc_options[MAIZO_WIIMC_OPTION_COUNT]={
 {"videoZoomHor","ZOOM HORIZONTAL",80,120,2,100,MAIZO_WIIMC_PERCENT},
 {"videoZoomVert","ZOOM VERTICAL",80,120,2,100,MAIZO_WIIMC_PERCENT},
 {"videoXshift","POSICAO HORIZONTAL",0,80,1,40,MAIZO_WIIMC_SIGNED},
 {"videoYshift","POSICAO VERTICAL",0,80,1,40,MAIZO_WIIMC_SIGNED},
 {"frameDropping","QUEDA DE QUADROS",0,2,1,1,MAIZO_WIIMC_NUMBER},
 {"cacheFill","CACHE DE VIDEO",5,90,5,30,MAIZO_WIIMC_PERCENT},
 {"audioDelay","ATRASO DO AUDIO",0,80,1,40,MAIZO_WIIMC_TENTHS},
 {"autoResume","RETOMAR VIDEO",0,1,1,1,MAIZO_WIIMC_BOOL},
 {"autoPlayNextVideo","REPRODUCAO SEGUINTE",0,4,1,0,MAIZO_WIIMC_NUMBER},
 {"skipBackward","VOLTAR SEGUNDOS",5,60,5,10,MAIZO_WIIMC_NUMBER},
 {"skipForward","AVANCAR SEGUNDOS",5,120,5,30,MAIZO_WIIMC_NUMBER},
 {"videoFull","VIDEO TELA CHEIA",0,1,1,0,MAIZO_WIIMC_BOOL},
 {"audioNorm","NORMALIZAR AUDIO",0,1,1,0,MAIZO_WIIMC_BOOL},
 {"videoDf","DEFLICKER VIDEO",0,1,1,0,MAIZO_WIIMC_BOOL},
 {"viWidth","VI LARGURA MAXIMA",0,1,1,0,MAIZO_WIIMC_BOOL},
 {"skipLoop","PULAR DEBLOCK",0,1,1,0,MAIZO_WIIMC_BOOL},
 {"interlaceHandle","ENTRELACAMENTO",0,2,1,1,MAIZO_WIIMC_NUMBER},
 {"dvdMenu","MENU DVD",0,1,1,1,MAIZO_WIIMC_BOOL},
 {"dvdDisabled","DESABILITAR DVD",0,1,1,0,MAIZO_WIIMC_BOOL},
 {"subtitleVisibility","MOSTRAR LEGENDAS",0,1,1,1,MAIZO_WIIMC_BOOL},
 {"subtitleDelay","ATRASO LEGENDAS",0,80,1,40,MAIZO_WIIMC_TENTHS},
 {"subtitleSize","TAMANHO LEGENDAS",10,50,1,25,MAIZO_WIIMC_TENTHS_POS},
 {"bold","LEGENDAS EM NEGRITO",0,1,1,0,MAIZO_WIIMC_BOOL},
 {"monofont","FONTE MONOESPACADA",0,1,1,0,MAIZO_WIIMC_BOOL},
 {"doubleStrike","MODO DOUBLE STRIKE",0,1,1,0,MAIZO_WIIMC_BOOL},
 {"tiledRender","RENDERIZACAO TILED",0,2,1,0,MAIZO_WIIMC_NUMBER},
 {"saveExit","SALVAR AO SAIR",0,1,1,1,MAIZO_WIIMC_BOOL},
 {"hideExtensions","OCULTAR EXTENSOES",0,1,1,1,MAIZO_WIIMC_BOOL},
 {"libass","USAR RENDER ASS",0,1,1,1,MAIZO_WIIMC_BOOL},
 {"force576p","FORCAR 576P",0,1,1,0,MAIZO_WIIMC_BOOL},
 {"playOrder","ORDEM DAS MUSICAS",0,4,1,0,MAIZO_WIIMC_NUMBER},
 {"nativeLoops","LOOPS NATIVOS",0,8,1,1,MAIZO_WIIMC_NUMBER},
 {"onlineCacheFill","CACHE ONLINE",5,90,5,20,MAIZO_WIIMC_PERCENT},
 {"screensaverArt","CAPAS NA PROTECAO",0,4,1,0,MAIZO_WIIMC_NUMBER},
 {"rumble","VIBRACAO WIIMOTE",0,1,1,1,MAIZO_WIIMC_BOOL},
};


/* Values here correspond to NIN_CFG or WiiSX 4.2.2 OPTIONS argv.
 * The separate calibration screen modifies only the MAIZO frontend UI.
 * Do not imply that arbitrary DOLs consume frontend-only geometry. */
int settings_count(System s){
 switch(s){case SYSTEM_COUNT:return 6;case SYSTEM_WII:return 7;case SYSTEM_GC:return 15;
 case SYSTEM_PS1:return 20;case SYSTEM_SNES:return 9;
 case SYSTEM_MEDIA:return 8+MAIZO_WIIMC_OPTION_COUNT;default:return 5;}
}
const char*settings_label(System s,int row){
 if(row==0)return "TEMA";
 if(s==SYSTEM_MEDIA&&row>=5&&row<5+MAIZO_WIIMC_OPTION_COUNT)
  return maizo_wiimc_options[row-5].label;
 if(row==1)return "MODO DA BIBLIOTECA";
 if(s==SYSTEM_COUNT&&row==2)return "FORCAR VI 704 - MODULOS COMPATIVEIS";
 if(row==settings_count(s)-1)return "VOLTAR";
 if(row==settings_count(s)-2)return "CALIBRAR TELA MAIZO";
 if(row==settings_count(s)-3)return s==SYSTEM_MEDIA||s==SYSTEM_HB?"REINDEXAR BIBLIOTECA":"GERENCIAR CAPAS";
 if((s==SYSTEM_SNES||s==SYSTEM_PS1)&&row>=settings_count(s)-7&&row<settings_count(s)-3){
  static const char*labels[]={"JOGO 240P: HORIZONTAL","JOGO 480I: HORIZONTAL","JOGO 240P: LARGURA","JOGO 480I: LARGURA"};
  return labels[row-(settings_count(s)-7)];
 }
 static const char*wii[]={"IOS INICIAL DO LOADER","PORTA USB"};
 static const char*gc[]={"MEMORY CARD EMULADA","WIDESCREEN","CHEATS","SKIP IPL","VIDEO GAMECUBE","MEMCARD MULTI","CONTROLE GC NATIVO","REMOVE LIMIT","LARGURA GC (NINTENDONT)","POSICAO HORIZONTAL GC"};
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
 if(s==SYSTEM_COUNT&&row==2)return c->force_vi704;
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
 if(s==SYSTEM_MEDIA&&row>=5&&row<5+MAIZO_WIIMC_OPTION_COUNT)return c->wiimc_opts[row-5];
 return 0;
}
void settings_value(const Config*c,System s,int row,char*dest,size_t cap){
 if(row==settings_count(s)-1){snprintf(dest,cap,"A / B");return;}
 if(row==settings_count(s)-2||row==settings_count(s)-3){snprintf(dest,cap,"ABRIR");return;}
 if((s==SYSTEM_SNES||s==SYSTEM_PS1)&&row>=settings_count(s)-7&&row<settings_count(s)-3){
  unsigned v=row==settings_count(s)-7?c->game_x240:row==settings_count(s)-6?c->game_x480:
             row==settings_count(s)-5?c->game_w240:c->game_w480;
  if(row==settings_count(s)-7||row==settings_count(s)-6)snprintf(dest,cap,"%+d",(int)v-50);
  else snprintf(dest,cap,"%u%%",v);
  return;
 }
 unsigned v=get(c,s,row);
 if(s==SYSTEM_MEDIA&&row>=5&&row<5+MAIZO_WIIMC_OPTION_COUNT){
  const MaizoWiiMCOption *o=&maizo_wiimc_options[row-5];
  unsigned value=c->wiimc_opts[row-5];
  if(o->type==MAIZO_WIIMC_BOOL)snprintf(dest,cap,"%s",value?"SIM":"NAO");
  else if(o->type==MAIZO_WIIMC_PERCENT)snprintf(dest,cap,"%u%%",value);
  else if(o->type==MAIZO_WIIMC_SIGNED)snprintf(dest,cap,"%+d",(int)value-40);
  else if(o->type==MAIZO_WIIMC_TENTHS)snprintf(dest,cap,"%+.1f",((int)value-40)/10.0);
  else if(o->type==MAIZO_WIIMC_TENTHS_POS)snprintf(dest,cap,"%.1f",value/10.0);
  else snprintf(dest,cap,"%u",value);
  return;
 }
 if(row==0){const char*themes[]={"VIOLETA","TURQUESA","CORAL","GRAFITE","OLED","CLASSICO"};snprintf(dest,cap,"%s",themes[v%6]);return;}
 if(row==1){const char *layouts[]={"GRADE","LISTA","CARROSSEL"};snprintf(dest,cap,"%s",layouts[v%3]);return;}
 if(s==SYSTEM_COUNT&&row==2){snprintf(dest,cap,"%s",v?"LIGADO":"DESLIGADO");return;}
 if(s==SYSTEM_WII){if(row==2)snprintf(dest,cap,"IOS %u",v);
  else if(v==2)snprintf(dest,cap,"AMBAS / AUTO");
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
 if(row>=settings_count(s)-3||row<0)return 0;
 if(s==SYSTEM_COUNT&&row==2){c->force_vi704=!c->force_vi704;return 1;}
 if((s==SYSTEM_SNES||s==SYSTEM_PS1)&&row>=settings_count(s)-7&&row<settings_count(s)-3){
  unsigned *p=row==settings_count(s)-7?&c->game_x240:row==settings_count(s)-6?&c->game_x480:
             row==settings_count(s)-5?&c->game_w240:&c->game_w480;
  unsigned lo=(row>=settings_count(s)-5)?70:0,hi=(row>=settings_count(s)-5)?130:100;
  if(dir<0)*p=*p<=lo?lo:*p-1;
  else *p=*p>=hi?hi:*p+1;
  return 1;
 }
 if(s==SYSTEM_MEDIA&&row>=5&&row<5+MAIZO_WIIMC_OPTION_COUNT){
  const MaizoWiiMCOption *o=&maizo_wiimc_options[row-5];
  unsigned *v=&c->wiimc_opts[row-5];
  c->wiimc_apply=1;
  if(dir<0)*v=*v<=o->min+o->step-1?o->min:*v-o->step;
  else *v=*v>=o->max-o->step+1?o->max:*v+o->step;
  return 1;
 }
 unsigned*v=NULL,lim=2;
 if(row==0){v=&c->theme;lim=6;}
 else if(row==1){v=&c->layout;lim=3;}
 else if(s==SYSTEM_WII&&row==2){
  const unsigned ios[]={58,249,250,251,252};int n=1;
  for(int i=0;i<5;i++)if(ios[i]==c->wii_ios){n=i;break;}
  c->wii_ios=ios[(n+5+(dir<0?-1:1))%5];return 1;
 }
 else if(s==SYSTEM_WII&&row==3){v=&c->wii_usb_port;lim=3;}
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
  c->media_volume=dir<0?(c->media_volume>=5?c->media_volume-5:0):(c->media_volume<=95?c->media_volume+5:100);c->wiimc_apply=1;return 1;
 }
 else if(s==SYSTEM_MEDIA&&row==3)v=&c->media_player;
 else if(s==SYSTEM_MEDIA&&row==4)v=&c->media_autoboot;
 if(!v)return 0;
 *v=(*v+lim+(dir<0?-1:1))%lim;return 1;
}

/* Category headers reflect actual supported parameters, not fake sliders. */
const char*settings_category(System s,int row){
 if(s==SYSTEM_COUNT&&row==2)return "VIDEO CRT GLOBAL";
 if(row==0||row==1)return "APARENCIA";
 if(row>=settings_count(s)-3)return "GERAL";
 if((s==SYSTEM_SNES||s==SYSTEM_PS1)&&row>=settings_count(s)-7&&row<settings_count(s)-3)return "VIDEO JOGO";
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
 if(s==SYSTEM_MEDIA){
  if(row==2)return "AUDIO";
  if(row>=5&&row<5+MAIZO_WIIMC_OPTION_COUNT){
   int i=row-5;
   if(i<=6)return "VIDEO E AUDIO";
   if(i<=16)return "REPRODUCAO";
   if(i<=18)return "DVD";
   if(i<=23)return "LEGENDAS";
   return "AVANCADO WIIMC";
  }
  return "REPRODUCAO";
 }
 return "EMULACAO";
}
