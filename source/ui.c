/* Stable MaizoWii frontend: no GX commands are needed for the 2D UI.
 * The VI reads RGB-converted YUYV pixels prepared directly by the CPU.
 * This avoids the FIFO/EFB/XFB synchronization path that produced both
 * a black regression and corrupted bars on hardware. */
#include "app.h"
#include "video_sw.h"
#include "font8x8.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
static unsigned background=0xff11111du,panel=0xff1e2030u,bright=0xff6366f1u;
static int graphite=0;
static void quad(float x,float y,float w,float h,unsigned a,unsigned b){
 maizo_video_rect(x,y,w,h,a,b);
}
static void roundrect(float x,float y,float w,float h,unsigned color){
 quad(x+6,y,w-12,h,color,color);
 quad(x,y+6,6,h-12,color,color);
 quad(x+w-6,y+6,6,h-12,color,color);
}
/* Map common Portuguese UTF-8 accents to readable ASCII in the compact
 * built-in bitmap font. The original bytes remain intact in game metadata. */
static unsigned char glyph_ascii(unsigned char first,unsigned char second){
 if(first!=0xc3)return '?';
 switch(second){
  case 0x80:case 0x81:case 0x82:case 0x83:case 0x84:case 0x85:return 'A';
  case 0xa0:case 0xa1:case 0xa2:case 0xa3:case 0xa4:case 0xa5:return 'a';
  case 0x87:return 'C';case 0xa7:return 'c';
  case 0x88:case 0x89:case 0x8a:case 0x8b:return 'E';
  case 0xa8:case 0xa9:case 0xaa:case 0xab:return 'e';
  case 0x8c:case 0x8d:case 0x8e:case 0x8f:return 'I';
  case 0xac:case 0xad:case 0xae:case 0xaf:return 'i';
  case 0x91:return 'N';case 0xb1:return 'n';
  case 0x92:case 0x93:case 0x94:case 0x95:case 0x96:return 'O';
  case 0xb2:case 0xb3:case 0xb4:case 0xb5:case 0xb6:return 'o';
  case 0x99:case 0x9a:case 0x9b:case 0x9c:return 'U';
  case 0xb9:case 0xba:case 0xbb:case 0xbc:return 'u';
  default:return '?';
 }
}
static void text(float x,float y,const char *s,float scale,
                 unsigned color,int max){
 float start=x;
 for(int n=0;s&&*s&&n<max;s++,n++){
  unsigned char ch=(unsigned char)*s;
  if(ch==0xc3&&s[1]){ch=glyph_ascii(ch,(unsigned char)s[1]);s++;}
  else if(ch>127)ch='?';
  if(ch=='\n'){x=start;y+=10*scale;continue;}
  for(int row=0;row<8;row++)
   for(int bit=0;bit<8;bit++)
    if(font8x8_basic[ch][row]&(1u<<bit))
     quad(x+bit*scale,y+row*scale,scale,scale,color,color);
  x+=8.0f*scale;
 }
}
static void label(float x,float y,const char*s,float z,unsigned color){text(x,y,s,z,color,70);}
static void clipped(float x,float y,const char*s,float z,unsigned color,int max){text(x,y,s,z,color,max);}
static void titlebar(const State*u,const Library*l,int hid){
 roundrect(104,10,526,56,panel);
 quad(104,10,526,3,bright,bright);
 label(124,28,"MAIZO",2.35f,0xffffffffu);
 roundrect(269,24,124,26,bright);label(283,32,"WII EDITION",1.05f,0xffffffffu);
 label(445,29,hid?"CONTROLE USB":"SD / USB",1.08f,0xffb9bbd6);
}
static void sidebar(const State*u){
 roundrect(10,10,84,460,panel);roundrect(22,22,60,60,bright);label(39,45,"M",2.0f,0xffffffffu);
 const char*items[]={"HOME","JOGOS","BUSCA","AJUSTES"};
 int active=u->page==PAGE_HOME?0:(u->page==PAGE_LIBRARY||u->page==PAGE_DETAIL)?1:u->page==PAGE_SEARCH?2:3;
 for(int i=0;i<4;i++){int y=112+i*70;
  if(i==active)roundrect(17,y,70,56,graphite?0xff3c444au:0xff35335fu);
  label(27,y+20,items[i],i==3?0.96f:1.00f,i==active?0xffffffffu:0xffabb0c9u);}
 label(28,437,"v0.1",1.0f,0xff8589a5u);
}
static void footer(const State*u,const Library*l){
 roundrect(104,440,526,30,panel);
 label(118,451,u->page==PAGE_HID?
  "B/START VOLTAR":
  u->page==PAGE_SETTINGS?
  "SELECT: DIAGNOSTICOS HID  A ALTERAR  B SALVAR":
  u->settings_for_game?
  "ESQUERDA/DIREITA AJUSTAR  A SALVAR  B VOLTAR":
  "A ABRIR  B VOLTAR  L/R SISTEMA  X BUSCA  Y FAVORITO",
  0.96f,0xffb3b8d2u);
}
static void home(const State*u,const Library*l){
 label(116,80,"SELECIONE UM SISTEMA",1.55f,0xfff8f9ffu);
 label(116,105,"JOGOS, EMULADORES E MULTIMIDIA",1.02f,0xffc4c8dbu);
 const unsigned colors[]={0xff13a4b8u,0xff7664d4u,0xffce5c91u,0xff697bd4u,
                          0xffc58f36u,0xff369a7au,0xff579ed3u,0xff119c9bu};
 const unsigned gray[]={0xff545d67u,0xff525c66u,0xff62666du,0xff525a65u,
                        0xff656b70u,0xff48565bu,0xff53616au,0xff4a5a5fu};
 const char*symbols[]={"WII","GC","PS1","SNES","NES","GBA","GB","TV"};
 const char*names[]={"NINTENDO WII","GAMECUBE","PLAYSTATION","SUPER NINT.",
                     "NINTENDO NES","GAME BOY ADV","GAME BOY","MULTIMIDIA"};
 for(int i=0;i<SYSTEM_COUNT;i++){
  float x=114+(i%4)*128.0f,y=132+(i/4)*145.0f;
  int focus=(int)u->system==i;
  if(focus)roundrect(x-3,y-3,126,139,bright);
  roundrect(x,y,120,133,focus?0xff35364au:0xff242838u);
  quad(x+5,y+5,110,66,graphite?gray[i]:colors[i],
       graphite?0xff303840u:0xff1b2337u);
  int idx=library_visible(l,(System)i,"",0,0);
  if(idx<0||!cover_draw(&l->games[idx],x+38,y+10,43,55))
   clipped(x+29,y+28,symbols[i],1.75f,0xffffffffu,5);
  clipped(x+7,y+81,names[i],1.04f,0xfff7f8ffu,13);
  char buf[28];snprintf(buf,sizeof(buf),"%d JOGOS",library_total(l,(System)i,"",0));
  label(x+7,y+100,buf,1.00f,0xffc8cde0u);
  label(x+7,y+117,module_available((System)i)?"PRONTO":"SEM MODULO",
        0.93f,module_available((System)i)?0xff92e2c4u:0xffffb5bdu);
 }
 label(117,424,"L/R SISTEMA    A ABRIR    START AJUSTES",1.00f,0xffc0c5dau);
}
static void library(const State*u,const Library*l,const Config*c){
 label(117,77,system_title(u->system),1.54f,0xffffffffu);
 char count[76];snprintf(count,sizeof(count),"%d ITENS  %s  %s",
 library_total(l,u->system,u->search,u->favorites),u->favorites?"FAVORITOS":"",
 c->layout?"LISTA":"GRADE");
 label(119,106,count,1.03f,0xffc1c5d8u);
 if(library_total(l,u->system,u->search,u->favorites)==0){
  roundrect(123,157,483,202,0xff292e43u);
  label(151,187,l->scanning?"PROCURANDO ARQUIVOS...":"BIBLIOTECA VAZIA",1.72f,0xffffffffu);
  label(151,235,l->scanning?"LENDO USB / SD":"COLOQUE JOGOS NA PASTA DO SISTEMA",1.06f,0xffccd1e0u);
  label(151,277,"Z ATUALIZAR FAVORITOS    B VOLTAR",0.97f,0xffafb7d1u);
  if(l->scanning)clipped(151,320,l->status,0.95f,0xff9bdccdu,46);
  return;
 }
 if(!c->layout){
  for(int i=0;i<8;i++){
   int pos=u->scroll+i,idx=library_visible(l,u->system,u->search,u->favorites,pos);
   if(idx<0)break;const Game*g=&l->games[idx];
   float x=115+(i%4)*127.0f,y=133+(i/4)*136.0f;
   roundrect(x,y,120,129,pos==u->selected?bright:0xff27283au);
   roundrect(x+5,y+5,110,89,0xff343750u);
   if(!cover_draw(g,x+23,y+6,74,86))
    clipped(x+32,y+40,"JOGO",1.33f,0xffffffffu,7);
   clipped(x+7,y+104,g->name,1.01f,0xfff7f7ffu,13);
   if(g->favorite)label(x+107,y+105,"*",0.98f,0xffffd783u);
  }
 }else{
  for(int i=0;i<8;i++){
   int pos=u->scroll+i,idx=library_visible(l,u->system,u->search,u->favorites,pos);
   if(idx<0)break;const Game*g=&l->games[idx];int y=132+i*36;
   if(pos==u->selected)roundrect(113,y,509,34,bright);
   else roundrect(113,y,509,34,i%2?0xff252a3au:0xff2d3144u);
   clipped(125,y+11,g->name,1.08f,0xfff7f7ffu,48);
   if(g->favorite)label(597,y+11,"*",1.2f,0xffffd783u);
  }
 }
 if(l->scanning)clipped(119,418,l->status,0.87f,0xffc3b8eeu,56);
}
static void detail(const State*u,const Library*l,const Config*c){
 if(u->detail<0||u->detail>=l->count)return;const Game*g=&l->games[u->detail];
 label(119,89,"DETALHES DO ARQUIVO",1.4f,0xffb6b7cfu);
 roundrect(119,124,170,247,0xff393451u);roundrect(126,132,156,233,graphite?0xff4b535au:0xff6c5bd3u);
 if(!cover_draw(g,126,132,156,233))label(176,235,g->system==SYSTEM_GC?"GC":g->system==SYSTEM_PS1?"PS1":"MEDIA",1.7f,0xffffffffu);
 clipped(310,135,g->name,1.52f,0xffffffffu,24);
 label(310,176,module_name(g->system),1.13f,0xffb9bcd9u);
 if(g->id[0]){char b[28];snprintf(b,sizeof(b),"ID: %.6s",g->id);label(310,199,b,1.1f,0xffb9bcd9u);}
 clipped(310,230,g->path,0.85f,0xffb1b2ccu,37);
 roundrect(313,291,269,48,module_available(g->system)?(graphite?0xff455f55u:0xff4fbc91u):0xff73536bu);
 label(345,307,module_available(g->system)?"A  INICIAR":"MODULO AUSENTE",1.39f,0xffffffffu);
 label(313,358,"Y FAVORITO  B VOLTAR",1.1f,0xffb9bcd9u);
 label(313,386,"START: AJUSTES DESTE JOGO",0.92f,0xffb9bcd9u);
}
static void search_ui(const State*u,const Library*l){
 const char*characters=" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";
 label(119,77,"PESQUISAR",1.55f,0xffffffffu);
 roundrect(119,108,490,47,0xff33334fu);
 clipped(132,125,u->search[0]?u->search:"DIGITE UM TITULO",1.34f,0xffffffffu,34);
 label(119,165,"SETAS: ESCOLHER  A: INSERIR  B: APAGAR",0.92f,0xffaeb0c8u);
 int n=(int)strlen(characters);
 for(int i=0;i<n;i++){
  int col=i%8,row=i/8;float x=126+col*59.0f,y=194+row*39.0f;
  roundrect(x,y,48,34,i==u->search_index?bright:0xff343650u);
  char str[4]={' ',0,0,0};str[0]=characters[i]==' '?'-':characters[i];
  label(x+15,y+10,str,1.35f,0xffffffffu);
 }
 label(119,415,"START: RESULTADOS   L/R: SISTEMA",0.98f,0xffaeb0c8u);
}
static void hid_diagnostics_line(void){
 HidDiagnostics d;input_hid_diagnostics(&d);
 char status[112];
 if(!d.vid)snprintf(status,sizeof(status),"HID USB: NENHUM DETECTADO");
 else snprintf(status,sizeof(status),"HID %04X:%04X  %s  REL:%u ERR:%u",
  d.vid,d.pid,d.connected?"CONECTADO":d.profile_loaded?"DESCONECTADO":"SEM PERFIL",
  d.reports,d.read_errors);
 clipped(119,424,status,0.77f,0xffc2c7e5u,65);
}
static void hid_page(void){
 HidDiagnostics d;input_hid_diagnostics(&d);
 label(119,81,"DIAGNOSTICO USB HID",1.60f,0xffffffffu);
 char line[128];
 if(!d.vid){
  label(119,121,"NENHUM GAMEPAD HID IDENTIFICADO",1.02f,0xffffb7b7u);
  label(119,160,"RECEPTORES XINPUT/PROPRIETARIOS",0.94f,0xffc3c4d8u);
  label(119,183,"PRECISAM DE DRIVER OU MODO HID.",0.94f,0xffc3c4d8u);
 }else{
  snprintf(line,sizeof(line),"ULTIMO USB: %04X:%04X",d.vid,d.pid);
  label(119,122,line,1.24f,0xffffffffu);
  const char*driver=d.profile_loaded==1?"CONTROLLER.INI":
                     d.profile_loaded==2?"DESCRITOR PADRAO":"SEM PERFIL";
  snprintf(line,sizeof(line),"PERFIL: %s",driver);
  label(119,153,line,1.04f,0xffc1c4dfu);
  snprintf(line,sizeof(line),"CONEXOES: %u  ENDPOINT: %02X  PACOTE: %u",
           d.connected_ports,d.endpoint,d.packet_size);
  label(119,183,line,0.98f,0xffc1c4dfu);
  snprintf(line,sizeof(line),"RELATORIOS: %u  FALHAS: %u  ID: %u",
           d.reports,d.read_errors,d.last_report_id);
  label(119,211,line,0.98f,0xffc1c4dfu);
  snprintf(line,sizeof(line),"ROTEAMENTO MULTIIN: %u",d.multiin);
  label(119,238,line,0.98f,0xffc1c4dfu);
  for(int i=0;i<4;i++){
   snprintf(line,sizeof(line),"PORTA %d    BOT: %04X    X:%+4d    Y:%+4d",
            i+1,d.held[i],d.stick_x[i],d.stick_y[i]);
   label(119,273+i*29,line,0.92f,0xfff3f3ffu);
  }
 }
 label(119,415,"B OU START: VOLTAR A CONFIGURACOES",0.90f,0xffb6b8d6u);
}
static void settings(const State*u,const Config*c){
 label(119,77,u->settings_for_game?"OPCOES DESTE JOGO":"CONFIGURACOES",1.53f,0xffffffffu);
 clipped(119,105,u->settings_for_game?"CONFIGURACAO INDIVIDUAL - TEMA/LAYOUT GLOBAIS":system_title(u->system),0.96f,0xffb4b8d6u,53);
 int n=settings_count(u->system);
 for(int i=0;i<n;i++){
  int y=137+i*25;
  if(i==u->settings_row)roundrect(116,y,498,24,bright);
  clipped(124,y+8,settings_label(u->system,i),1.02f,0xffffffffu,31);
  char val[48];settings_value(c,u->system,i,val,sizeof(val));
  clipped(488,y+8,val,0.91f,0xffffffffu,15);
 }
 if(u->system==SYSTEM_WII)label(119,406,"WII: USB LOADER GX ENHANCED / CIOS",0.94f,0xffb2b7d1u);
 else if(u->system==SYSTEM_MEDIA)label(119,406,"VOLUME: NECESSITA ADAPTACAO DO WIIMC",0.85f,0xffb2b7d1u);
 else if(u->system!=SYSTEM_GC&&u->system!=SYSTEM_PS1)
  label(119,406,"AJUSTES AVANCADOS NO MOTOR ORIGINAL",0.93f,0xffb2b7d1u);
 hid_diagnostics_line();
}
/* With the CPU backend the boot message can be displayed before the
 * filesystem, HID receiver, controller and cover scanner are initialized. */
static void present(void){maizo_video_present();}
int ui_init(void){return maizo_video_init();}
void ui_boot_message(const char*stage){
 graphite=0;
 bright=0xff7777e0u;
 panel=0xff202538u;
 quad(0,0,640,480,0xff101521u,0xff182338u);
 quad(0,0,640,8,bright,bright);
 roundrect(100,142,440,198,0xff252c42u);
 label(133,177,"MAIZO WII",2.75f,0xffffffffu);
 label(134,228,"INICIALIZANDO FRONTEND",1.25f,0xffcad2f4u);
 clipped(134,275,stage,1.13f,0xffa9f0d6u,46);
 label(134,307,"MODO DE VIDEO DO CONSOLE PRESERVADO",0.97f,0xffb8c3dcu);
 present();
}
void ui_draw(const State*u,const Library*l,const Config*c,int hid){
 graphite=c->theme==3;
 bright=graphite?0xff8998a4u:c->theme==1?0xff1cbea8u:c->theme==2?0xffdd6689u:0xff6661eau;
 panel=graphite?0xff24282du:0xff202132u;
 background=graphite?0xff131619u:0xff121420u;
 quad(0,0,640,480,background,0xff171b2eu);
 quad(0,0,640,7,bright,bright);
 sidebar(u);titlebar(u,l,hid);
 switch(u->page){case PAGE_HOME:home(u,l);break;case PAGE_LIBRARY:library(u,l,c);break;case PAGE_DETAIL:detail(u,l,c);break;case PAGE_SEARCH:search_ui(u,l);break;case PAGE_SETTINGS:settings(u,u->settings_for_game?&u->game_config:c);break;case PAGE_HID:hid_page();break;case PAGE_INFO:
 label(120,86,"AVISO MAIZO",1.8f,0xffffffffu);label(120,156,u->info,1.0f,0xffc9c9dfu);
 label(120,204,"APERTE B PARA VOLTAR",1.05f,0xffaeb1c8u);break;}
 footer(u,l);
 present();
}
void ui_shutdown(void){maizo_video_shutdown();}
