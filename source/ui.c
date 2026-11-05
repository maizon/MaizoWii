/* MAIZO Wii: full-width, gamepad-first catalog UI.
 * 4:3 CRT logical canvas 640x480. CPU/YUYV VI backend: no GX FIFO.
 * Three identical library/system layouts: grid, split list and carousel. */
#include "app.h"
#include "video_sw.h"
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
 "Wii","GC","PS1","SNES","NES","GBA","GB","MEDIA"
};
static const char *layout_name[3]={"GRADE","LISTA","CARROSSEL"};
static const char *sys_names[SYSTEM_COUNT]={
 "NINTENDO WII","GAMECUBE","PLAYSTATION","SUPER NINTENDO",
 "NINTENDO NES","GAME BOY ADV","GAME BOY","VIDEOS E MUSICAS"
};
static int layout_of(const Config*c){return c->layout<3?(int)c->layout:0;}
static int count_items(const State*u,const Library*l,int home){
 return home?SYSTEM_COUNT:library_total(l,u->system,u->search,u->favorites);
}
static const Game *game_at(const State*u,const Library*l,int home,int pos){
 int idx=home?library_visible(l,(System)pos,"",0,0):
  library_visible(l,u->system,u->search,u->favorites,pos);
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
 rounded(x,y,w,h,0xff344550u);
 if(g&&cover_draw(g,x+2,y+2,w-4,h-4))return;
 /* Genre-color visual placeholder for unloaded artwork. */
 static const unsigned palette[SYSTEM_COUNT]={
 0xff386c78u,0xff5a617eu,0xff656183u,0xff6c7763u,
 0xff796a54u,0xff49796eu,0xff626a86u,0xff46707au
 };
 quad(x+5,y+5,w-10,h-10,palette[sys],0xff26343cu);
 quad(x+9,y+9,w-18,3,accent,accent);
 float z=sys==SYSTEM_MEDIA?1.32f:sys==SYSTEM_SNES?1.46f:1.7f;
 clipped(x+14,y+h*.46f,sys_short[sys],z,ink,7);
}
static void topbar(const State*u,const Library*l,int hid){
 rounded(19,12,602,53,surface);
 quad(20,12,600,3,accent,accent);
 label(33,28,"MAIZO",2.0f,ink);
 quad(155,23,2,30,accent,accent);
 label(170,31,u->page==PAGE_HOME||u->page==PAGE_WELCOME?"MEUS SISTEMAS":
  system_title(u->system),1.05f,0xffd5e1e6u);
 rounded(462,26,57,25,u->storage_usb?0xff345b4du:0xff42474bu);
 clipped(471,34,u->storage_usb?"USB OK":"USB --",0.85f,ink,8);
 rounded(524,26,48,25,u->storage_sd?0xff345b4du:0xff42474bu);
 clipped(533,34,u->storage_sd?"SD OK":"SD --",0.85f,ink,7);
 rounded(577,26,29,25,hid?0xff3b9678u:0xff38434bu);
 clipped(584,34,"HID",0.77f,ink,4);
}
static void footer(const State*u){
 rounded(19,446,602,26,surface);
 const char *msg="A ABRIR   B VOLTAR   L/R SISTEMA   Y FAVORITO   START AJUSTES";
 if(u->page==PAGE_HOME)
  msg="A ENTRAR   SETAS NAVEGAM   START AJUSTES   Z REINDEXAR";
 else if(u->page==PAGE_LIBRARY)
  msg="A DETALHES   B SISTEMAS   X BUSCA   Y FAVORITO   START AJUSTES";
 else if(u->page==PAGE_DETAIL)
  msg="A INICIAR   START: AJUSTES DESTE JOGO   B VOLTAR";
 else if(u->page==PAGE_SETTINGS)
  msg="SETAS ALTERAM   A CONFIRMA   B SALVA   Z DIAGNOSTICO";
 else if(u->page==PAGE_WELCOME)
  msg="100% GAMEPAD    A CONTINUAR    DIREITA/ESQUERDA TEMA";
 else if(u->page==PAGE_SEARCH)
  msg="A ESCREVER   B APAGAR   START VER RESULTADOS";
 else if(u->page==PAGE_INFO)
  msg="A/B VOLTAR";
 else if(u->page==PAGE_HID)
  msg="B/START VOLTAR    USB HID E PERFIS";
 else if(u->page==PAGE_CALIBRATION)
  msg="SETAS AJUSTAR   A APLICAR   B SALVAR";
 clipped(31,455,msg,0.90f,0xffcad4dcu,81);
}
static void section_title(const State*u,const Library*l,const Config*c,int home){
 char s[98];
 int n=count_items(u,l,home);
 label(27,83,home?"ESCOLHA UMA PLATAFORMA":system_title(u->system),
       1.56f,ink);
 snprintf(s,sizeof(s),"%s  /  %d %s  /  %s",layout_name[layout_of(c)],n,
    home?"SISTEMAS":"ITENS",l->scanning?"INDEXANDO...":"PRONTO");
 clipped(29,111,s,0.94f,muted,77);
}
static void grid_tile(const State*u,const Library*l,int home,int pos,
                      float x,float y){
 int selected=item_marked(u,home,pos);
 if(selected)rounded(x-3,y-3,130,151,accent);
 rounded(x,y,124,145,selected?0xff3b4d55u:card);
 art(u,l,home,pos,x+11,y+7,102,114);
 clipped(x+8,y+127,item_name(u,l,home,pos),0.87f,ink,15);
 if(home){
  int ready=module_available((System)pos);
  quad(x+112,y+8,6,6,ready?0xff6fdab0u:0xffd88876u,
       ready?0xff6fdab0u:0xffd88876u);
 }else{
  const Game*g=game_at(u,l,0,pos);
  if(g&&g->favorite)clipped(x+109,y+128,"*",0.98f,0xffeac777u,2);
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
static void carousel(const State*u,const Library*l,int home){
 int n=count_items(u,l,home);
 if(n<=0)return;
 int focused=home?(int)u->system:u->selected;
 /* Draw far cards first, then center. Every card uses the same native
  * cover aspect and preserves the bitmap's 128x160 geometry. */
 for(int delta=-2;delta<=2;delta++){
  int pos=focused+delta;
  if(pos<0||pos>=n)continue;
  float x,y,w,h;
  if(delta==0){x=256;y=143;w=128;h=180;}
  else if(delta<0){x=delta==-1?99:15;y=delta==-1?170:195;
                    w=delta==-1?112:79;h=delta==-1?150:114;}
  else {x=delta==1?429:546;y=delta==1?170:195;
        w=delta==1?112:79;h=delta==1?150:114;}
  if(delta==0)rounded(x-4,y-4,w+8,h+8,accent);
  rounded(x-2,y-2,w+4,h+4,surface);
  art(u,l,home,pos,x,y,w,h);
 }
 rounded(91,346,458,71,surface);
 clipped(111,359,item_name(u,l,home,focused),1.40f,ink,31);
 char s[75];snprintf(s,sizeof(s),"%d / %d   <  %s  >",
    focused+1,n,home?system_title((System)focused):"A: DETALHES");
 clipped(112,391,s,0.90f,muted,56);
}
static void no_items(const State*u,const Library*l){
 rounded(44,140,552,270,surface);
 label(75,188,l->scanning?"PROCURANDO JOGOS...":"NENHUM ARQUIVO ENCONTRADO",1.47f,ink);
 clipped(76,232,l->scanning?l->status:"CONFIRA USB/SD, PASTAS E FORMATOS",1.08f,muted,51);
 label(75,281,"B VOLTA PARA ESCOLHER OUTRO SISTEMA",1.01f,muted);
 if(!u->storage_usb)label(75,324,"USB NAO MONTADO: TENTANDO NOVAMENTE",0.95f,0xffe7ab89u);
}
static void catalog(const State*u,const Library*l,const Config*c,int home){
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
 cover_draw(g,48,143,166,245);
 clipped(255,141,g->name,1.54f,ink,25);
 clipped(255,184,module_name(g->system),1.04f,muted,38);
 if(g->id[0]){
  char id[24];snprintf(id,sizeof(id),"ID: %.6s",g->id);
  label(255,208,id,1.05f,muted);
 }
 clipped(255,244,g->path,0.87f,muted,48);
 if(g->system==SYSTEM_MEDIA)
  label(255,280,u->game_config.media_player?"WIIMC ORIGINAL":"WIIMC HID",0.93f,muted);
 int can=module_variant_available(g->system,&u->game_config);
 rounded(255,317,322,52,can?0xff40675bu:0xff754d4du);
 label(279,334,can?"A   INICIAR":"MODULO NAO INSTALADO",1.26f,ink);
 label(256,392,"START: AJUSTES DESTE JOGO",0.99f,muted);
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
 char s[65];const char *names[]={"VIOLETA","TURQUESA","CORAL","GRAFITE"};
 snprintf(s,sizeof(s),"TEMA: %s  < >",names[c->theme%4]);
 label(69,346,s,1.11f,accent);
}
static void hid_page(void){
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
 label(43,413,"B OU START PARA VOLTAR",0.90f,muted);
}
static void calibration_ui(const State*u,const Config*c){
 label(38,83,"CALIBRACAO DO FRONTEND",1.56f,ink);
 clipped(38,111,"VI HORIZONTAL + AJUSTE DE CONTEUDO; NAO ALTERA EMULADORES",0.94f,muted,77);
 const char*names[]={"CENTRO HORIZONTAL","CENTRO VERTICAL","LARGURA / VI","ALTURA DO CONTEUDO","RESTAURAR PADRAO","SALVAR E VOLTAR"};
 const int vals[]={((int)c->video_x-50),((int)c->video_y-50),
                   (int)c->video_w,(int)c->video_h};
 for(int i=0;i<6;i++){
  int y=143+i*42;
  rounded(39,y,557,35,i==u->calibration_row?0xff4a6e73u:surface);
  label(53,y+11,names[i],1.05f,ink);
  char value[32]="";
  if(i<2)snprintf(value,sizeof(value),"%+d",vals[i]);
  if(i>=2&&i<4)snprintf(value,sizeof(value),"%d%%",vals[i]);
  if(i<4)label(521,y+11,value,1.04f,ink);
 }
}
static void hid_note(void){
 HidDiagnostics d;input_hid_diagnostics(&d);
 char s[128];snprintf(s,sizeof(s),"HID %04X:%04X   REL %u   ERROS %u",
   d.vid,d.pid,d.reports,d.read_errors);
 clipped(38,414,s,0.82f,muted,80);
}
static void settings_ui(const State*u,const Config*c){
 label(39,83,u->settings_for_game?"OPCOES DESTE JOGO":"AJUSTES DO MAIZO",1.57f,ink);
 clipped(39,111,u->settings_for_game?"PARAMETROS DESTE JOGO":system_title(u->system),
  0.95f,muted,65);
 int n=settings_count(u->system);
 int top=u->settings_row-7;if(top<0)top=0;
 if(top>n-9)top=n>9?n-9:0;
 for(int i=top;i<n&&i<top+9;i++){
  int y=139+(i-top)*29;
  rounded(39,y,555,25,u->settings_row==i?0xff4b7479u:surface);
  clipped(52,y+8,settings_label(u->system,i),0.99f,ink,35);
  char val[56];settings_value(c,u->system,i,val,sizeof(val));
  clipped(454,y+8,val,0.92f,ink,18);
 }
 hid_note();
}
void ui_boot_message(const char*stage){
 maizo_video_clear();
 ink=0xfff7f8fau;muted=0xffb2c4cdu;base=0xff10151au;
 surface=0xff202b31u;card=0xff27333au;accent=0xff82afb2u;
 quad(0,0,640,480,base,0xff1b2a30u);
 rounded(65,150,510,190,surface);
 label(94,193,"MAIZO",2.85f,ink);
 label(94,248,"INICIANDO WII HUB",1.28f,muted);
 clipped(94,288,stage,1.02f,accent,65);
 maizo_video_present();
}
int ui_init(void){return maizo_video_init();}
void ui_draw(const State*u,const Library*l,const Config*c,int hid){
 static const unsigned accents[]={0xff7772dbu,0xff37b9a4u,0xffe07c97u,0xff82afb2u};
 accent=accents[c->theme%4];
 int graphite=c->theme==3;
 base=graphite?0xff10191eu:0xff141b26u;
 surface=graphite?0xff202a30u:0xff232b3bu;
 card=graphite?0xff29363bu:0xff293449u;
 muted=graphite?0xffb8c9cbu:0xffb3bed5u;
 maizo_video_clear();
 quad(0,0,640,480,base,0xff1e2b31u);
 quad(0,0,640,6,accent,accent);
 topbar(u,l,hid);
 switch(u->page){
 case PAGE_HOME:catalog(u,l,c,1);break;
 case PAGE_LIBRARY:catalog(u,l,c,0);break;
 case PAGE_DETAIL:detail(u,l,c);break;
 case PAGE_SEARCH:search_ui(u,l);break;
 case PAGE_SETTINGS:settings_ui(u,u->settings_for_game?&u->game_config:c);break;
 case PAGE_HID:hid_page();break;
 case PAGE_CALIBRATION:calibration_ui(u,c);break;
 case PAGE_WELCOME:welcome(c);break;
 case PAGE_INFO:
  label(43,126,"AVISO MAIZO",1.83f,ink);
  clipped(43,197,u->info,1.05f,muted,70);
  label(43,259,"APERTE A OU B PARA VOLTAR",1.13f,accent);
  break;
 }
 footer(u);
 maizo_video_present();
}
void ui_shutdown(void){maizo_video_shutdown();}
