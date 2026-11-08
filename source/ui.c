/* MAIZO Wii: full-width, gamepad-first catalog UI.
 * 4:3 CRT logical canvas 640x480. CPU/YUYV VI backend: no GX FIFO.
 * Three identical library/system layouts: grid, split list and carousel. */
#include "app.h"
#include "video_sw.h"
#include "carousel_layout.h"
#include "cover_download.h"
#include "font8x8.h"
#include <string.h>
#include <stdio.h>

static unsigned ink=0xfff7f8fau,muted=0xffabbac5u,base=0xff10151au;
static unsigned surface=0xff202b31u,card=0xff27333au,accent=0xff82afb2u;
static void quad(float x,float y,float w,float h,unsigned a,unsigned b){
 maizo_video_rect(x,y,w,h,a,b);
}
static void rounded(float x,float y,float w,float h,unsigned color){
 if(w<=12||h<=12)return;
 quad(x+6,y,w-12,h,color,color);
 quad(x,y+6,6,h-12,color,color);
 quad(x+w-6,y+6,6,h-12,color,color);
}
static unsigned char ascii(unsigned char first,unsigned char last){
 if(first!=0xc3)return '?';
 if(last>=0x80&&last<=0x85)return 'A';
 if(last>=0xa0&&last<=0xa5)return 'a';
 if(last==0x87)return 'C';if(last==0xa7)return 'c';
 if(last>=0x88&&last<=0x8b)return 'E';
 if(last>=0xa8&&last<=0xab)return 'e';
 if(last>=0x8c&&last<=0x8f)return 'I';
 if(last>=0xac&&last<=0xaf)return 'i';
 if(last==0x91)return 'N';if(last==0xb1)return 'n';
 if(last>=0x92&&last<=0x96)return 'O';
 if(last>=0xb2&&last<=0xb6)return 'o';
 if(last>=0x99&&last<=0x9c)return 'U';
 if(last>=0xb9&&last<=0xbc)return 'u';
 return '?';
}
static void txt(float x,float y,const char *s,float scale,unsigned col,int max){
 for(int n=0;s&&*s&&n<max;s++,n++){
  unsigned char ch=(unsigned char)*s;
  if(ch==0xc3&&s[1]){ch=ascii(ch,(unsigned char)s[1]);s++;}
  else if(ch>127)ch='?';
  for(int row=0;row<8;row++)
   for(int bit=0;bit<8;bit++)
    if(font8x8_basic[ch][row]&(1u<<bit))
     quad(x+bit*scale,y+row*scale,scale,scale,col,col);
  x+=8.0f*scale;
 }
}
static void label(float x,float y,const char*s,float z,unsigned col){txt(x,y,s,z,col,80);}
static void clipped(float x,float y,const char*s,float z,unsigned col,int max){
 txt(x,y,s,z,col,max);
}
static const char *sys_short[SYSTEM_COUNT]={
 "WII","GC","PS1","SNES","NES","GBA","GBC","VID","HB"
};
static const char *layout_name[3]={"GRADE","LISTA","CARROSSEL"};
static const char *premium_themes[6]={"VIOLETA","TURQUESA","CORAL","GRAFITE","OLED","CLASSICO"};
static const char *sys_names[SYSTEM_COUNT]={
 "NINTENDO WII","GAMECUBE","PLAYSTATION","SUPER NINTENDO",
 "NINTENDO NES","GAME BOY ADV","GAME BOY","VIDEOS E MUSICAS","HOMEBREWS"
};
static int layout_of(const Config*c){return c->layout<3?(int)c->layout:2;}
static int count_items(const State*u,const Library*l,int home){
 return home?SYSTEM_COUNT:library_total(l,u->system,u->search,0);
}
static const Game *game_at(const State*u,const Library*l,int home,int pos){
 int idx=home?library_visible(l,(System)pos,"",0,0):
  library_visible(l,u->system,u->search,0,pos);
 return idx>=0&&idx<l->count?&l->games[idx]:NULL;
}
static int system_at(const State*u,int home,int pos){
 return home?pos:(int)u->system;
}
static const char*item_name(const State*u,const Library*l,int home,int pos){
 if(home)return sys_names[pos];
 const Game*g=game_at(u,l,home,pos);
 return g?g->name:"SEM TITULO";
}
static int item_marked(const State*u,int home,int pos){
 return home?(int)u->system==pos:u->selected==pos;
}
static void art(const State*u,const Library*l,int home,int pos,
                float x,float y,float w,float h){
 int sys=system_at(u,home,pos);
 const Game*g=game_at(u,l,home,pos);
 /* Platform images are prebuilt from retro-game-console-icons; custom Wii fallback. */
 if(home){
  static const char *keys[SYSTEM_COUNT]={
   "IWII","IGC","IPS1","ISNES","INES","IGBA","IGB","IMEDIA","IHB"
  };
  Game icon={0};
  icon.system=(System)sys;
  snprintf(icon.id,sizeof(icon.id),"%s",keys[sys]);
  if(cover_draw(&icon,x,y,w,h))return;
 }
 rounded(x,y,w,h,0xff344550u);
 if(g&&cover_draw(g,x+2,y+2,w-4,h-4))return;
 /* Genre-color visual placeholder for unloaded artwork. */
 static const unsigned palette[SYSTEM_COUNT]={
 0xff386c78u,0xff5a617eu,0xff656183u,0xff6c7763u,
 0xff796a54u,0xff49796eu,0xff626a86u,0xff46707au,0xff424b4eu
 };
 quad(x+5,y+5,w-10,h-10,palette[sys],0xff26343cu);
 quad(x+9,y+9,w-18,3,accent,accent);
 float z=sys==SYSTEM_MEDIA?1.32f:sys==SYSTEM_SNES?1.46f:1.7f;
 clipped(x+14,y+h*.46f,sys_short[sys],z,ink,7);
}
/* Blend the accent into the OLED background: precomputed opaque pixels
 * resemble transparent tint, even on the YUYV/CRT software framebuffer. */
static unsigned mix_rgb(unsigned a,unsigned b,unsigned strength){
 unsigned color=0xff000000u;
 for(unsigned shift=0;shift<24;shift+=8){
  unsigned aa=(a>>shift)&255u,bb=(b>>shift)&255u;
  color|=((aa*(256u-strength)+bb*strength)>>8)<<shift;
 }
 return color;
}
/* Only the focused tab renders a two-tone circular halo. No textures,
 * framebuffers, antialiasing or shader allocation are needed. */
static void tab_halo(int center_x,int center_y){
 const unsigned edge=mix_rgb(0xff10151au,accent,146);
 const unsigned fill=mix_rgb(0xff10151au,accent,46);
 for(int radius=21;radius>=19;radius-=2){
  unsigned color=radius==21?edge:fill;
  for(int dy=-radius;dy<=radius;dy++){
   int span=radius;
   while(span>0 && span*span+dy*dy>radius*radius)span--;
   quad(center_x-span,center_y+dy,span*2+1,1,color,color);
  }
 }
}
/* One icon for every platform, with a soft accent halo on selection.
 * Use the SAME pinned MZG1 console artwork as the home carousel.
 * Falls back to a short label only if icon assets are missing. */
static int topbar_phase=0;
static void topbar(const State*u,const Library*l,int hid){
 (void)l;(void)hid;
 static const char*codes[SYSTEM_COUNT]={
  "TWII","TGC","TPS1","TSNES","TNES","TGBA","TGB","TMEDIA","THB"
 };
 const int dy=(8-topbar_phase)*-6;
 quad(0,0,640,(float)(62*topbar_phase)/8,0xff07090bu,0xff141b20u);
 /* Brand is NOT part of the animated console strip. */
 label(145,28+dy,"L",1.1f,muted);
 for(int k=0;k<SYSTEM_COUNT;k++){
  float x=164+k*46;
  int selected=(int)u->system==k;
  if(selected)tab_halo((int)x+22,31+dy);
  Game icon={0};icon.system=(System)k;
  snprintf(icon.id,sizeof(icon.id),"%s",codes[k]);
  if(!cover_draw_icon(&icon,x+10,17+dy,24,30)){
   const char *name=sys_short[k];int n=(int)strlen(name);
   float z=n>3?0.78f:0.88f;
   label(x+(44.0f-n*8.0f*z)*0.5f,28+dy,name,z,selected?accent:ink);
  }
 }
 label(592,28+dy,"R",1.1f,muted);
}
/* The always-on MAIZO brand stays in its original position (25,23).
 * Draw this AFTER pages, tabs and dialogs so nothing can obscure it.
 * A quiet backdrop protects legibility on 4:3 CRTs without rearranging UI. */
static void brand_overlay(void){
 rounded(16,13,102,38,0xff10171eu);
 quad(19,23,2,17,accent,accent);
 label(25,23,"MAIZO",2.0f,0xfff7f8fau);
 quad(25,46,74,2,mix_rgb(0xff10171eu,accent,110),
      mix_rgb(0xff10171eu,accent,110));
}
static void footer(const State*u){
 rounded(19,446,602,26,surface);
 const char *msg="A ABRIR  B VOLTAR  L/R SISTEMA  START GERAIS";
 if(u->page==PAGE_LIBRARY)
  msg="A ABRIR  Y DETALHES  B INICIO  START SISTEMA";
 else if(u->page==PAGE_HOME)
  msg="A ENTRAR  SETAS SISTEMAS  START GERAIS  Z REINDEXAR";
 else if(u->page==PAGE_DETAIL)
  msg="A INICIAR  Y OPCOES DO JOGO  B VOLTAR  START SISTEMA";
 else if(u->page==PAGE_SETTINGS)
  msg="SETAS ALTERAM  A CONFIRMA  B SALVA  Z DIAGNOSTICO";
 else if(u->page==PAGE_COVER_PICK)
  msg="A MARCAR  B VOLTAR  SELECIONE INICIAR NO FIM";
 else if(u->page==PAGE_COVERS)
  msg="B CANCELAR   A VOLTAR AO FINAL";
 else if(u->page==PAGE_SEARCH)
  msg="A ESCREVER  B APAGAR  START RESULTADOS";
 else if(u->page==PAGE_INFO)msg="A/B VOLTAR";
 else if(u->page==PAGE_CALIBRATION)
  msg="SETAS AJUSTAR  A APLICAR  B SALVAR";
 clipped(31,455,msg,0.88f,0xffcad4dcu,82);
}
static void game_metadata(const Game *g,float y){
 if(!g)return;
 char region[28]="REGIAO: --";
 if((g->system==SYSTEM_WII||g->system==SYSTEM_GC)&&strlen(g->id)==6){
  char code=g->id[3];
  const char *value=code=='E'?"NTSC-U":code=='J'?"NTSC-J":
    (code=='P'||code=='D'||code=='F'||code=='I'||code=='S')?"PAL":NULL;
  if(value)snprintf(region,sizeof(region),"REGIAO: %s",value);
 }
 if(g->region[0])snprintf(region,sizeof(region),"REGIAO: %s",g->region);
 char year[28]="ANO: --";if(g->year)snprintf(year,sizeof(year),"ANO: %u",(unsigned)g->year);
 const char *values[]={sys_short[g->system],year,"GENERO: --",region};
 const float widths[]={96.0f,145.0f,141.0f,164.0f};
 float x=27;
 for(int k=0;k<4;k++){
  rounded(x,y,widths[k],25,surface);
  clipped(x+6,y+9,values[k],0.84f,k==0?accent:muted,26);
  x+=widths[k]+6;
 }
}
static void section_title(const State*u,const Library*l,const Config*c,int home){
 char s[98];
 int n=count_items(u,l,home);
 label(27,83,home?"ESCOLHA UMA PLATAFORMA":system_title(u->system),
       1.56f,ink);
 snprintf(s,sizeof(s),"%s  /  %d %s  /  %s",layout_name[layout_of(c)],n,
    home?"SISTEMAS":"ITENS",l->scanning?"INDEXANDO...":"PRONTO");
 clipped(29,111,s,0.94f,muted,77);
 /* Storage snapshot: zero FAT I/O in this frame. Particularly useful on
  * Priiloader cold autoboot when the USB device arrives late. */
 rounded(463,81,151,30,surface);
 label(474,91,"USB",0.88f,muted);
 label(500,91,u->storage_usb?"OK":"--",0.88f,
       u->storage_usb?0xff77d5a6u:0xffe2a68au);
 label(536,91,"SD",0.88f,muted);
 label(554,91,u->storage_sd?"OK":"--",0.88f,
       u->storage_sd?0xff77d5a6u:0xffe2a68au);
}
static void grid_tile(const State*u,const Library*l,int home,int pos,
                      float x,float y){
 int selected=item_marked(u,home,pos);
 if(selected)rounded(x-3,y-3,130,151,accent);
 rounded(x,y,124,145,selected?0xff3b4d55u:card);
 art(u,l,home,pos,x+21,y+7,82,114);
 clipped(x+8,y+127,item_name(u,l,home,pos),0.87f,ink,15);
 if(home){
  int ready=module_available((System)pos);
  quad(x+112,y+8,6,6,ready?0xff6fdab0u:0xffd88876u,
       ready?0xff6fdab0u:0xffd88876u);
 }else{
  const Game*g=game_at(u,l,0,pos);
  (void)g; /* No favorites badges. */
 }
}
static void grid(const State*u,const Library*l,const Config*c,int home){
 int n=count_items(u,l,home);
 if(!n)return;
 int start=home?0:u->scroll;
 for(int i=0;i<8;i++){
  int pos=start+i;
  if(pos>=n)break;
  float x=30+(i%4)*151.0f;
  float y=133+(i/4)*153.0f;
  grid_tile(u,l,home,pos,x,y);
 }
}
static void split_list(const State*u,const Library*l,int home){
 int n=count_items(u,l,home);
 int selected=home?(int)u->system:u->selected;
 int start=home?0:u->scroll;
 rounded(27,132,341,301,surface);
 rounded(377,132,235,301,surface);
 for(int i=0;i<8;i++){
  int pos=start+i;
  if(pos>=n)break;
  float y=142+i*35.0f;
  int focused=selected==pos;
  rounded(35,y,325,32,focused?0xff4e747au:(i&1?0xff253239u:0xff202b32u));
  if(focused)quad(35,y+4,3,24,accent,accent);
  clipped(47,y+9,item_name(u,l,home,pos),0.95f,ink,38);
 }
 if(n){
  art(u,l,home,selected,430,148,130,191);
  clipped(393,354,item_name(u,l,home,selected),1.07f,ink,27);
  if(home){
   clipped(394,388,module_available((System)selected)?
       "PRONTO PARA ABRIR":"MODULO NAO ENCONTRADO",0.87f,muted,29);
  }else{
   const Game*g=game_at(u,l,home,selected);
   if(g)clipped(394,388,g->path,0.74f,muted,32);
  }
 }
}
/* Premium carousel: no counters or hints on the home and game library. */
static void carousel(const State*u,const Library*l,int home){
 int n=count_items(u,l,home);
 if(n<=0)return;
 int focused=home?(int)u->system:u->selected;
 static const int order[5]={-2,2,-1,1,0};
 /* Pixel-stable slots: NEVER translate the whole row on a selection event.
  * The previous global 'shift' moved every cover ~9 pixels and then
  * stepped back, producing the reported wobble/flicker on CRT and Dolphin.
  * Animate only the focus frame; card geometry is exact integers each frame. */
 static int previous_focus=-1,previous_page=-1,focus_frames=8;
 if(previous_page!=(int)u->page){
  previous_focus=focused;previous_page=(int)u->page;focus_frames=8;
 }else if(previous_focus!=focused){
  previous_focus=focused;focus_frames=0;
 }
 if(focus_frames<8)focus_frames++;
 unsigned focus_color=accent;
 if(focus_frames<8){
  const unsigned dark=0xff37454eu;
  unsigned rgb=0;
  for(unsigned shift=0;shift<24;shift+=8){
   int from=(int)((dark>>shift)&255u),to=(int)((accent>>shift)&255u);
   int value=from+(to-from)*focus_frames/8;
   rgb|=((unsigned)value&255u)<<shift;
  }
  focus_color=0xff000000u|rgb;
 }
 for(int i=0;i<5;i++){
  int delta=order[i],pos=focused+delta;
  if(pos<0||pos>=n)continue;
  MaizoCardSlot slot;
  if(!maizo_card_slot(delta,&slot))continue;
  const float x=(float)slot.x,y=(float)slot.y;
  const float w=(float)slot.w,h=(float)slot.h;
  /* Shadow and background must not move when another card is selected. */
  quad(x+4,y+7,w,h,0xff080c12u,0xff080c12u);
  if(delta==0)quad(x-5,y-5,w+10,h+10,focus_color,focus_color);
  quad(x-3,y-3,w+6,h+6,0xff29343du,0xff29343du);
  art(u,l,home,pos,x,y,w,h);
 }
 const char*name=item_name(u,l,home,focused);
 int length=(int)strlen(name);if(length>39)length=39;
 float scale=length>27?0.95f:length>19?1.20f:1.48f;
 clipped(320.0f-length*4.0f*scale,372,name,scale,ink,length);
 /* Focus micro-accent: fixed position, no moving/flashing cover geometry. */
 quad(295,394,50,2,focus_color,focus_color);
 if(!home)game_metadata(game_at(u,l,home,focused),406);
 else{
  char games[72];snprintf(games,sizeof(games),"Jogos Disponíveis: %d",library_total(l,(System)focused,"",0));
  int chars=(int)strlen(games);
  clipped(320.0f-chars*4.0f,412,games,1.0f,muted,64);
 }
}
static void no_items(const State*u,const Library*l){
 rounded(44,140,552,270,surface);
 label(75,188,l->scanning?"PROCURANDO JOGOS...":"NENHUM ARQUIVO ENCONTRADO",1.47f,ink);
 clipped(76,232,l->scanning?l->status:"CONFIRA USB/SD, PASTAS E FORMATOS",1.08f,muted,51);
 label(75,281,"B VOLTA PARA ESCOLHER OUTRO SISTEMA",1.01f,muted);
 if(!u->storage_usb)label(75,324,"USB NAO MONTADO: TENTANDO NOVAMENTE",0.95f,0xffe7ab89u);
}
static void catalog(const State*u,const Library*l,const Config*c,int home){
 /* Shared title and live storage status on BOTH home and game libraries. */
 section_title(u,l,c,home);
 if(!count_items(u,l,home)){no_items(u,l);return;}
 switch(layout_of(c)){
  case 1:split_list(u,l,home);break;
  case 2:carousel(u,l,home);break;
  default:grid(u,l,c,home);break;
 }
}
static void detail(const State*u,const Library*l,const Config*c){
 if(u->detail<0||u->detail>=l->count)return;
 const Game*g=&l->games[u->detail];
 label(35,89,"DETALHES",1.52f,ink);
 rounded(35,130,193,278,surface);
 rounded(45,140,172,251,0xff38525cu);
 clipped(70,256,sys_short[g->system],1.66f,ink,8);
 /* Cover must always belong to the actual selected detail game. */
 cover_draw(g,48,143,168,235);
 clipped(255,141,g->name,1.54f,ink,25);
 clipped(255,184,module_name(g->system),1.04f,muted,38);
 if(g->id[0]){
  char id[24];snprintf(id,sizeof(id),"ID: %.6s",g->id);
  label(255,208,id,1.05f,muted);
 }
 clipped(255,244,g->path,0.87f,muted,48);
 if(g->system==SYSTEM_HB && g->description[0])
  clipped(255,280,g->description,0.87f,muted,45);
 else if(g->system==SYSTEM_MEDIA)
  label(255,280,u->game_config.media_player?"WIIMC ORIGINAL":"WIIMC HID",0.93f,muted);
 else label(255,280,"OPCOES AVANCADAS NO MODULO",0.81f,muted);
 int can=module_variant_available(g->system,&u->game_config);
 rounded(255,317,322,52,can?0xff40675bu:0xff754d4du);
 label(279,334,can?"A   INICIAR":"MODULO NAO INSTALADO",1.26f,ink);
 game_metadata(g,393);
}
static void search_ui(const State*u,const Library*l){
 const char*alphabet=" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";
 label(36,84,"BUSCA",1.75f,ink);
 char count[45];snprintf(count,sizeof(count),"%d RESULTADOS",count_items(u,l,0));
 label(450,93,count,0.91f,muted);
 rounded(34,127,571,40,surface);
 clipped(50,139,u->search[0]?u->search:"DIGITE UM NOME",1.16f,ink,44);
 int n=(int)strlen(alphabet);
 for(int i=0;i<n;i++){
  int col=i%8,row=i/8;
  float x=48+col*70.0f,y=184+row*43.0f;
  rounded(x,y,56,35,i==u->search_index?accent:surface);
  char t[2]={alphabet[i]==' '?'_':alphabet[i],0};
  txt(x+21,y+11,t,1.3f,ink,1);
 }
}
static void welcome(const Config*c){
 label(44,112,"BEM-VINDO AO MAIZO",2.16f,ink);
 clipped(47,164,"SEU WII, SUAS CAPAS, SEUS JOGOS E VIDEOS.",1.19f,muted,56);
 rounded(42,220,559,163,surface);
 label(69,250,"100% GAMEPAD",1.36f,ink);
 label(69,287,"GRADE  |  LISTA  |  CARROSSEL",1.16f,ink);
 char s[65];snprintf(s,sizeof(s),"TEMA: %s  < >",premium_themes[c->theme%6]);
 label(69,346,s,1.11f,accent);
}
static void hid_page(const State*u){
 HidDiagnostics d;input_hid_diagnostics(&d);
 label(42,92,"DIAGNOSTICO USB HID",1.58f,ink);
 char s[110];
 snprintf(s,sizeof(s),"CONTROLE: %04X:%04X  %s",d.vid,d.pid,
   d.connected?"CONECTADO":d.vid?"DESCONECTADO":"NAO DETECTADO");
 label(43,139,s,1.02f,ink);
 snprintf(s,sizeof(s),"RELATORIOS %u   FALHAS %u   PERFIL %d",
   d.reports,d.read_errors,d.profile_loaded);
 label(43,176,s,1.00f,muted);
 snprintf(s,sizeof(s),"ENDPOINT %02X   TAMANHO %u",d.endpoint,d.packet_size);
 label(43,212,s,1.00f,muted);
 snprintf(s,sizeof(s),"ROTEAMENTO MULTIIN: %u",d.multiin);
 label(43,237,s,0.95f,muted);
 for(int i=0;i<4;i++){
  snprintf(s,sizeof(s),"PORTA %d  BOT: %04X   X %+4d   Y %+4d",
   i+1,d.held[i],d.stick_x[i],d.stick_y[i]);
  label(43,260+i*36,s,0.99f,ink);
 }
 snprintf(s,sizeof(s),"DISPOSITIVOS: USB %s  /  SD %s",
  u->storage_usb?"MONTADO":"AUSENTE",u->storage_sd?"MONTADO":"AUSENTE");
 label(43,391,s,0.91f,u->storage_usb?0xff77d5a6u:0xffe2a68au);
 label(43,423,"B OU START PARA VOLTAR",0.90f,muted);
}
static void cover_pick_ui(const State*u,const Config*c){
 label(38,80,"CENTRAL DE CAPAS",1.55f,ink);
 static const char*systems[]={"WII","GAMECUBE","PLAYSTATION 1","SNES","NES","GBA","GB / GBC"};
 static const char*names[]={
  "WII","GAMECUBE","PLAYSTATION 1","SNES","NES","GBA","GB / GBC",
  "SELECIONAR TODOS","LIMPAR SELECAO","ATUALIZAR EXISTENTES",
  "FONTE PREFERIDA","TIPO DE IMAGEM","REGIAO PRIORITARIA",
  "MODO DOWNLOAD","RECONSTRUIR CACHE RAM","INICIAR DOWNLOAD"
 };
 int sys=u->covers_row<7?u->covers_row:(int)u->system;
 char stats[104];
 if(sys>=0&&sys<7)
  snprintf(stats,sizeof(stats),"%s: %d JOGOS  /  %d CAPAS  /  %d FALTANDO",
    systems[sys],u->covers_total[sys],u->covers_ready[sys],
    u->covers_total[sys]-u->covers_ready[sys]);
 else snprintf(stats,sizeof(stats),"WII / GC / PS1 / SNES / NES / GBA / GB");
 clipped(38,109,stats,0.93f,muted,78);
 int top=u->covers_row>9?u->covers_row-9:0;
 for(int row=top;row<16&&row<top+10;row++){
  int y=137+(row-top)*27;
  rounded(39,y,552,25,u->covers_row==row?0xff425d61u:surface);
  char mark[52]="";
  if(row<7)snprintf(mark,sizeof(mark),"[%c]",(u->covers_mask&(1u<<row))?'X':' ');
  else if(row==9)snprintf(mark,sizeof(mark),"[%c]",u->covers_update?'X':' ');
  else if(row==10){
   const char*v[]={"AUTO","GAMETDB","LIBRETRO"};
   snprintf(mark,sizeof(mark),"%s",v[c->cover_source%3]);
  }else if(row==11){
   const char*v[]={"FRONTAL","COMPLETA / SNAP","3D / TITULO"};
   snprintf(mark,sizeof(mark),"%s",v[c->cover_art%3]);
  }else if(row==12){
   const char*v[]={"AUTO","BR/PT","USA","EUROPA","JAPAO"};
   snprintf(mark,sizeof(mark),"%s",v[c->cover_region%5]);
  }else if(row==13){
   const char*v[]={"TODOS/FEITOS","JOGO SELECIONADO","REPETIR FALHAS"};
   snprintf(mark,sizeof(mark),"%s",v[u->covers_mode%3]);
  }
  clipped(52,y+8,names[row],0.91f,ink,30);
  clipped(389,y+8,mark,0.81f,accent,25);
 }
}
static void downloads_ui(void){
 MaizoCoverDownloadStatus d=maizo_cover_download_status();
 label(38,84,"DOWNLOAD DE CAPAS",1.55f,ink);
 clipped(38,124,"WII GC PS1 SNES NES GBA GB/GBC  -  HTTPS",0.94f,muted,72);
 char line[120];
 snprintf(line,sizeof(line),"VERIFICADOS %d / %d",d.processed,d.total);
 label(38,163,line,1.25f,ink);
 snprintf(line,sizeof(line),"NOVAS: %d   JA EXISTIAM: %d",d.downloaded,d.existing);
 label(38,202,line,1.05f,accent);
 snprintf(line,sizeof(line),"NAO ENCONTRADAS: %d   SEM PROVEDOR: %d",d.failed,d.unavailable);
 label(38,239,line,0.98f,ink);
 rounded(38,286,562,24,surface);
 if(d.total>0){
  int bar=d.processed*555/d.total;
  if(bar>555)bar=555;
  if(bar>0)quad(41,290,bar,16,accent,accent);
 }
 clipped(38,331,d.current,1.0f,ink,48);
 clipped(38,364,d.message,0.96f,muted,70);
 label(38,417,d.active?"B INTERROMPER DOWNLOAD":"A OU B VOLTAR",1.0f,ink);
}
static void calibration_ui(const State*u,const Config*c){
 label(38,83,"CALIBRACAO DE VIDEO CRT",1.56f,ink);
 clipped(38,111,"VI 704 EM GERAIS: MODULOS ADAPTADOS; NAO QUALQUER HOMEBREW",0.94f,muted,77);
 const char*names[]={"CENTRO HORIZONTAL","CENTRO VERTICAL","LARGURA / VI","ALTURA DO CONTEUDO","MODO VIDEO SO FRONTEND","RESTAURAR PADRAO","SALVAR E VOLTAR"};
 const int vals[]={((int)c->video_x-50),((int)c->video_y-50),
                   (int)c->video_w,(int)c->video_h};
 for(int i=0;i<7;i++){
  int y=141+i*36;
  rounded(39,y,557,35,i==u->calibration_row?0xff4a6e73u:surface);
  label(53,y+11,names[i],1.05f,ink);
  char value[32]="";
  if(i<2)snprintf(value,sizeof(value),"%+d",vals[i]);
  if(i>=2&&i<4)snprintf(value,sizeof(value),"%d%%",vals[i]);
   if(i==4){
    static const char *modes[]={"AUTO","480I/576I","240P/288P"};
    snprintf(value,sizeof(value),"%s",modes[c->frontend_video_mode%3]);
   }
  if(i<4)label(521,y+11,value,1.04f,ink);
   if(i==4)label(457,y+11,value,0.88f,ink);
 }
 if(u->video_preview_frames>0)
  label(40,414,"A CONFIRMA / B REVERTE MODO DE VIDEO",0.91f,0xffefbf88u);
}
static void hid_note(void){
 HidDiagnostics d;input_hid_diagnostics(&d);
 char s[128];snprintf(s,sizeof(s),"HID %04X:%04X   REL %u   ERROS %u",
   d.vid,d.pid,d.reports,d.read_errors);
 clipped(38,414,s,0.82f,muted,80);
}
static void settings_ui(const State*u,const Config*c){
 label(39,83,u->settings_for_game?"OPCOES DESTE JOGO":u->settings_global?"CONFIGURACOES GERAIS":"CONFIGURACOES DO SISTEMA",1.44f,ink);
 char group[88];
 System scope=u->settings_global?SYSTEM_COUNT:u->system;
 snprintf(group,sizeof(group),"%s / %s",u->settings_global?"MAIZO":system_title(u->system),settings_category(scope,u->settings_row));
 clipped(39,111,group,0.95f,muted,65);
 int n=settings_count(scope);
 /* Work in VISIBLE rows: hiding theme/layout on per-system screens
  * must not make the selected row disappear near the end of a long list. */
 int limited=!u->settings_global&&!u->settings_for_game;
 int first=limited?2:0;
 int count=n-first-(limited?1:0);
 int focus=u->settings_row-first;
 if(limited&&u->settings_row==n-1)focus--;
 if(focus<0)focus=0;
 int top=focus-7;if(top<0)top=0;
 if(top>count-9)top=count>9?count-9:0;
 int visible=0;
 for(int i=first;i<n;i++){
  if(limited&&i==n-2)continue;
  if(visible<top){visible++;continue;}
  if(visible>=top+9)break;
  int y=139+(visible++-top)*29;
  int selected=u->settings_row==i;
  rounded(39,y,555,25,selected?0xff4b7479u:surface);
  if(selected)quad(42,y+5,3,15,accent,accent);
  clipped(52,y+8,settings_label(scope,i),0.99f,ink,35);
  char val[56];settings_value(c,scope,i,val,sizeof(val));
  clipped(454,y+8,val,0.92f,selected?0xfff7f8fau:muted,18);
 }
 hid_note();
}
void ui_boot_message(const char*stage){
 maizo_video_clear();
 /* Boot is also a frontend screen: keep the branding at the same
  * top-left location while the loading message changes. */
 quad(0,0,640,480,0xff000000u,0xff000000u);
 /* Loading is intentionally static: no fake progress percentage or extra
  * FAT calls on the video thread. The brand remains at its fixed origin. */
 rounded(101,183,438,122,0xff171e26u);
 quad(105,187,430,3,0xff41676eu,0xff41676eu);
 label(129,210,"PREPARANDO SEU WII",1.53f,0xfff7f8fau);
 if(stage)clipped(130,258,stage,0.89f,0xffb3bed5u,48);
 brand_overlay();
 if(stage){
  int count=(int)strlen(stage);if(count>57)count=57;
  float scale=0.85f;
  float px=608.0f-count*8.0f*scale;
  if(px<20.0f)px=20.0f;
  clipped(px,447,stage,scale,0xffa9b5bdu,count);
 }
 maizo_video_present();
}
int ui_init(void){return maizo_video_init();}
void ui_draw(const State*u,const Library*l,const Config*c,int hid){
 static const unsigned accents[]={0xff7772dbu,0xff37b9a4u,0xffe07c97u,0xff82afb2u,0xffb1b7c6u,0xffbad0d3u};
 accent=accents[c->theme%6];
 int graphite=c->theme==3,oled=c->theme==4,classic=c->theme==5;
 base=oled?0xff08090cu:classic?0xff263039u:graphite?0xff10191eu:0xff141b26u;
 surface=oled?0xff15181cu:classic?0xff344049u:graphite?0xff202a30u:0xff232b3bu;
 card=oled?0xff202328u:classic?0xff40525au:graphite?0xff29363bu:0xff293449u;
 muted=classic?0xffd3e2e5u:graphite?0xffb8c9cbu:0xffb3bed5u;
 maizo_video_clear();
 quad(0,0,640,480,base,0xff1e2b31u);
 /* Restrained native color fields: system identity without heavyweight GPU
  * shaders, animation buffers, or icons fighting for screen space. */
 static const unsigned ambience[SYSTEM_COUNT]={
  0xff1c323eu,0xff30294fu,0xff263753u,0xff393044u,
  0xff49352eu,0xff213d4au,0xff2b3445u,0xff2d3b46u,0xff26383eu
 };
 unsigned target=ambience[(unsigned)u->system%SYSTEM_COUNT];
 if(u->page==PAGE_LIBRARY||u->page==PAGE_DETAIL){
  const Game*g=u->page==PAGE_DETAIL?(u->detail>=0&&u->detail<l->count?&l->games[u->detail]:NULL):game_at(u,l,0,u->selected);
  if(g){unsigned tint=cover_tint(g);if(tint)target=tint;}
 }
 if(oled)target=0xff17191eu;
 static unsigned previous=0;
 if(!previous)previous=target;
 if(previous!=target){
  unsigned a=previous,b=target,channels=0;
  for(int i=0;i<3;i++){
   unsigned shift=i*8,va=(a>>shift)&255,vb=(b>>shift)&255;
   int d=(int)vb-(int)va;int step=d>0?(d+7)/8:(d-7)/8;
   int v=(int)va+step;if((d>0&&v>(int)vb)||(d<0&&v<(int)vb))v=vb;
   channels|=((unsigned)v&255u)<<shift;
  }
  previous=0xff000000u|channels;
 }
 quad(0,104,640,304,base,previous);
 quad(0,408,640,72,previous,base);
 /* Paint the page first, including any overlays. */
 switch(u->page){
 case PAGE_HOME:catalog(u,l,c,1);break;
 case PAGE_LIBRARY:catalog(u,l,c,0);break;
 case PAGE_DETAIL:detail(u,l,c);break;
 case PAGE_SEARCH:search_ui(u,l);break;
 case PAGE_SETTINGS:settings_ui(u,u->settings_for_game?&u->game_config:c);break;
 case PAGE_HID:hid_page(u);break;
  case PAGE_COVER_PICK:cover_pick_ui(u,c);break;
  case PAGE_COVERS:downloads_ui();break;
 case PAGE_CALIBRATION:calibration_ui(u,c);break;
 case PAGE_WELCOME:welcome(c);break;
 case PAGE_INFO:
  label(43,126,"AVISO MAIZO",1.83f,ink);
  clipped(43,197,u->info,1.05f,muted,70);
  label(43,259,"APERTE A OU B PARA VOLTAR",1.13f,accent);
  break;
 }
 footer(u);
 if(u->in_system){if(topbar_phase<8)topbar_phase++;topbar(u,l,hid);}
 else topbar_phase=0;
 /* LAST overlay pass: independent of page, modal and tab animation. */
 brand_overlay();
 maizo_video_present();
}
void ui_shutdown(void){maizo_video_shutdown();}
