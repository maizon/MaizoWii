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
static unsigned muted=0xff9fa9c0u,card=0xff242d38u;
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
static void button(float x,float y,float w,const char*s,unsigned color){
 roundrect(x,y,w,25,color);
 clipped(x+10,y+8,s,0.92f,0xffffffffu,24);
}
static void status_chips(const State*u){
 button(444,27,69,u->storage_usb?"USB OK":"USB --",u->storage_usb?0xff305a50u:0xff3e4048u);
 button(521,27,68,u->storage_sd?"SD OK":"SD --",u->storage_sd?0xff305a50u:0xff3e4048u);
}
static void titlebar(const State*u,const Library*l,int hid){
 roundrect(103,11,528,54,panel);
 quad(104,11,526,3,bright,bright);
 label(121,27,"MAIZO",2.27f,0xffffffffu);
 quad(223,22,2,34,bright,bright);
 label(236,29,"WII HUB",1.16f,0xffd3d9e6u);
 status_chips(u);
 if(hid)quad(606,27,8,25,0xff53e0b6u,0xff53e0b6u);
}
static void sidebar(const State*u){
 roundrect(10,10,83,460,panel);
 roundrect(20,23,63,65,bright);
 label(42,43,"M",2.42f,0xffffffffu);
 quad(24,97,55,2,0xff48515fu,0xff48515fu);
 const char*items[]={"HOME","JOGOS","BUSCA","AJUSTE"};
 int active=u->page==PAGE_HOME||u->page==PAGE_WELCOME?0:
   (u->page==PAGE_LIBRARY||u->page==PAGE_DETAIL)?1:
   u->page==PAGE_SEARCH?2:3;
 for(int i=0;i<4;i++){
  int y=114+i*67;
  if(active==i){
   roundrect(15,y,72,51,graphite?0xff3e4b50u:0xff35375bu);
   quad(16,y+9,3,33,bright,bright);
  }
  clipped(23,y+20,items[i],0.92f,
   active==i?0xffffffffu:0xffa1acc1u,7);
 }
 label(30,430,"V6.1",0.96f,0xff8394a8u);
}
static void footer(const State*u,const Library*l){
 roundrect(104,440,526,30,panel);
 const char*hint="A ABRIR   B VOLTAR   L/R SISTEMA   X BUSCA   Y FAVORITO";
 if(u->page==PAGE_HOME)hint="A SISTEMA   L/R TROCAR   Z ATUALIZAR   START AJUSTES";
 else if(u->page==PAGE_WELCOME)hint="A INICIAR   ESQUERDA/DIREITA TEMA   B PULAR";
 else if(u->page==PAGE_HID)hint="B/START VOLTAR  |  VID/PID, PACOTES E MULTIIN";
 else if(u->page==PAGE_SETTINGS)hint="SETA AJUSTA   A CONFIRMA   B SALVA   Z DIAGNOSTICO";
 else if(u->page==PAGE_SEARCH)hint="A DIGITAR   B APAGAR   START RESULTADOS";
 else if(u->page==PAGE_DETAIL)hint="A INICIAR   START AJUSTES DO JOGO   B VOLTAR";
 else if(u->page==PAGE_INFO)hint="A/B VOLTAR";
 clipped(115,451,hint,0.93f,0xffbfc7d6u,62);
}
static void home(const State*u,const Library*l){
 label(115,79,"MINHA CENTRAL",1.61f,0xfff6f8ffu);
 char summary[100];
 snprintf(summary,sizeof(summary),"%d TITULOS  |  %s",l->count,
          l->scanning?"ATUALIZANDO BIBLIOTECA":"WII / GC / EMULADORES / MEDIA");
 clipped(116,105,summary,0.96f,muted,61);
 const unsigned colors[]={0xff3e8895u,0xff776baau,0xffb16b86u,0xff6b86a3u,
                          0xffb59153u,0xff4f927bu,0xff718ca3u,0xff3b8d91u};
 const char*symbols[]={"WII","GC","PS1","SNES","NES","GBA","GB","TV"};
 const char*names[]={"NINTENDO WII","GAMECUBE","PLAYSTATION 1","SUPER NINTENDO",
                     "NINTENDO NES","GAME BOY ADV.","GAME BOY","VIDEOS / AUDIO"};
 for(int i=0;i<SYSTEM_COUNT;i++){
  float x=114+(i%4)*128.0f,y=130+(i/4)*143.0f;
  int focused=(int)u->system==i,ready=module_available((System)i);
  if(focused)roundrect(x-3,y-3,126,141,bright);
  roundrect(x,y,120,135,focused?0xff33404cu:card);
  quad(x+5,y+5,110,69,graphite?0xff56646au:colors[i],
       graphite?0xff303c41u:0xff253144u);
  int idx=library_visible(l,(System)i,"",0,0);
  if(idx<0||!cover_draw(&l->games[idx],x+38,y+8,44,64))
   clipped(x+21,y+27,symbols[i],1.86f,0xffffffffu,5);
  quad(x+6,y+78,108,1,0xff485663u,0xff485663u);
  clipped(x+9,y+86,names[i],0.92f,0xffffffffu,14);
  char count[28];snprintf(count,sizeof(count),"%d ARQUIVOS",library_total(l,(System)i,"",0));
  clipped(x+9,y+103,count,0.91f,0xffbdc9d8u,17);
  quad(x+9,y+122,6,6,ready?0xff70d9a6u:0xffe49e8cu,ready?0xff70d9a6u:0xffe49e8cu);
  clipped(x+20,y+120,ready?"MODULO PRONTO":"SEM MODULO",0.77f,0xffc0cadbu,16);
 }
 if(l->scanning)clipped(117,418,l->status,0.85f,0xff80d7b6u,55);
 else label(117,417,"ESCOLHA UMA PLATAFORMA PARA EXPLORAR",0.92f,muted);
}
static void library(const State*u,const Library*l,const Config*c){
 label(117,79,system_title(u->system),1.55f,0xfff6f9ffu);
 const int n=library_total(l,u->system,u->search,u->favorites);
 char subtitle[92];snprintf(subtitle,sizeof(subtitle),"%d TITULOS    %s    %s",
  n,u->favorites?"SOMENTE FAVORITOS":"TODOS OS ARQUIVOS",c->layout?"LISTA":"GRADE");
 clipped(117,106,subtitle,0.96f,muted,58);
 if(n==0){
  roundrect(119,141,494,255,card);
  label(147,177,l->scanning?"PROCURANDO ARQUIVOS...":"NENHUM ARQUIVO",1.64f,0xffffffffu);
  clipped(147,221,l->scanning?"INDEXANDO USB / SD":"VERIFIQUE A PASTA DO SISTEMA NO USB/SD",
          1.04f,0xffcbd5e3u,46);
  label(147,265,"X BUSCA  |  Y FAVORITO  |  B VOLTAR",0.92f,muted);
  if(l->scanning)clipped(147,318,l->status,0.94f,0xff95debfu,50);
  return;
 }
 if(!c->layout){
  for(int i=0;i<8;i++){
   int pos=u->scroll+i,idx=library_visible(l,u->system,u->search,u->favorites,pos);
   if(idx<0)break;
   const Game*g=&l->games[idx];
   float x=115+(i%4)*127.0f,y=130+(i/4)*138.0f;
   int selected=pos==u->selected;
   if(selected)roundrect(x-3,y-3,126,139,bright);
   roundrect(x,y,120,133,selected?0xff35414du:card);
   roundrect(x+5,y+5,110,88,0xff3a4855u);
   if(!cover_draw(g,x+21,y+6,78,86))
    clipped(x+26,y+43,g->system==SYSTEM_MEDIA?"VIDEO":"JOGO",1.2f,0xffffffffu,8);
   clipped(x+7,y+104,g->name,1.02f,0xfff5f8ffu,13);
   if(g->favorite)label(x+108,y+105,"*",1.1f,0xfff9cd72u);
  }
 }else{
  for(int i=0;i<8;i++){
   int pos=u->scroll+i,idx=library_visible(l,u->system,u->search,u->favorites,pos);
   if(idx<0)break;
   const Game*g=&l->games[idx];
   float y=129+i*36.0f;int selected=pos==u->selected;
   roundrect(116,y,503,33,selected?bright:(i%2?0xff25303au:card));
   clipped(128,y+10,g->name,1.04f,0xfff2f6feu,49);
   if(g->favorite)label(596,y+10,"*",1.1f,0xffffce79u);
   if(!selected)quad(118,y+32,499,1,0xff394552u,0xff394552u);
  }
 }
 int selected=library_visible(l,u->system,u->search,u->favorites,u->selected);
 if(selected>=0){
  const Game*g=&l->games[selected];
  quad(117,412,3,19,bright,bright);
  clipped(130,417,g->name,0.93f,0xffe9eff8u,45);
 }
 if(l->scanning)clipped(401,417,l->status,0.78f,0xff8bd9bdu,26);
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
 int can_launch=module_variant_available(g->system,&u->game_config);
 roundrect(313,291,269,48,can_launch?(graphite?0xff455f55u:0xff4fbc91u):0xff73536bu);
 label(345,307,can_launch?"A  INICIAR":"MODULO AUSENTE",1.39f,0xffffffffu);
 if(g->system==SYSTEM_MEDIA)
  label(313,267,u->game_config.media_player?"WIIMC ORIGINAL":"WIIMC HID",0.95f,muted);
 label(313,358,"Y FAVORITO  B VOLTAR",1.1f,0xffb9bcd9u);
 label(313,386,"START: AJUSTES DESTE JOGO",0.92f,0xffb9bcd9u);
}
static void search_ui(const State*u,const Library*l){
 const char*characters=" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";
 label(119,77,"PESQUISAR",1.55f,0xffffffffu);
 char count[48];
 snprintf(count,sizeof(count),"%d RESULTADOS",
          library_total(l,u->system,u->search,u->favorites));
 clipped(449,82,count,0.88f,muted,20);
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
static void welcome(const Config*c){
 label(127,91,"BEM-VINDO AO MAIZO",2.06f,0xfff4f7ffu);
 clipped(127,129,"SEU HUB DE JOGOS E MIDIA NO WII",1.08f,muted,47);
 roundrect(119,178,493,193,card);
 button(137,194,123,"100% GAMEPAD",0xff405661u);
 button(271,194,133,"USB / SD",0xff405661u);
 button(415,194,169,"JOGOS E VIDEOS",0xff405661u);
 label(140,239,"A  EXPLORAR BIBLIOTECA",1.24f,0xffffffffu);
 label(140,267,"L/R  TROCAR DE SISTEMA",1.05f,0xffd2dae6u);
 label(140,293,"X  BUSCAR   Y  FAVORITOS",1.04f,0xffd2dae6u);
 const char*palettes[]={"VIOLETA","TURQUESA","CORAL","GRAFITE"};
 char b[60];snprintf(b,sizeof(b),"TEMA: %s  < >",palettes[c->theme%4]);
 label(138,347,b,1.06f,0xffa8e2d1u);
 label(124,406,"A INICIAR. O TEMA PODERA SER ALTERADO.",0.95f,muted);
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
static void calibration_ui(const State*u,const Config*c){
 label(119,76,"CALIBRACAO DA TELA",1.48f,0xffffffffu);
 label(119,100,"VALE APENAS PARA A INTERFACE MAIZO",0.91f,0xffc9d0deu);
 const char*items[]={"POSICAO HORIZONTAL","POSICAO VERTICAL","LARGURA","ALTURA","RESTAURAR PADRAO","SALVAR E VOLTAR"};
 const int vals[]={((int)c->video_x-50),((int)c->video_y-50),(int)c->video_w,(int)c->video_h};
 for(int i=0;i<6;i++){
  int y=129+i*36;
  roundrect(116,y,498,32,i==u->calibration_row?bright:0xff2a2d3cu);
  label(128,y+10,items[i],1.02f,0xffffffffu);
  char value[32]="";
  if(i<2)snprintf(value,sizeof(value),"%+d",vals[i]);
  else if(i<4)snprintf(value,sizeof(value),"%d%%",vals[i]);
  if(i<4)label(533,y+10,value,0.98f,0xffffffffu);
 }
 /* Reference rectangle exposes clipping/overscan on CRT televisions. */
 quad(125,367,476,2,0xfff5dd7bu,0xfff5dd7bu);
 quad(125,411,476,2,0xfff5dd7bu,0xfff5dd7bu);
 quad(125,367,2,46,0xfff5dd7bu,0xfff5dd7bu);
 quad(599,367,2,46,0xfff5dd7bu,0xfff5dd7bu);
 label(145,385,"SETAS: AJUSTAR  A: APLICAR  B: SALVAR",0.92f,0xffffffffu);
}
static void settings(const State*u,const Config*c){
 label(119,77,u->settings_for_game?"OPCOES DESTE JOGO":"CONFIGURACOES",1.53f,0xffffffffu);
 clipped(119,105,u->settings_for_game?"CONFIGURACAO INDIVIDUAL - TEMA/LAYOUT GLOBAIS":system_title(u->system),0.96f,0xffb4b8d6u,53);
 int n=settings_count(u->system);
 int top=u->settings_row-8;if(top<0)top=0;
 if(top>n-9)top=n>9?n-9:0;
 for(int i=top;i<n&&i<top+9;i++){
  int y=137+(i-top)*29;
  if(i==u->settings_row)roundrect(116,y,498,24,bright);
  clipped(124,y+8,settings_label(u->system,i),1.02f,0xffffffffu,31);
  char val[48];settings_value(c,u->system,i,val,sizeof(val));
  clipped(488,y+8,val,0.91f,0xffffffffu,15);
 }
 if(u->system==SYSTEM_WII)label(119,406,"WII: USB LOADER GX ENHANCED / CIOS",0.94f,0xffb2b7d1u);
 else if(u->system==SYSTEM_MEDIA)label(119,406,"VOLUME: NECESSITA ADAPTACAO DO WIIMC",0.85f,0xffb2b7d1u);
 else if(u->system==SYSTEM_PS1)
  label(119,404,"WiiSX: VIDEO/240P; RX: OPCOES DO CORE",0.90f,0xffb2b7d1u);
 else if(u->system==SYSTEM_GC)
  label(119,404,"NINTENDONT: LARGURA E DESLOC. VERTICAL",0.82f,0xffb2b7d1u);
 else label(119,406,"CALIBRACAO SOMENTE DA INTERFACE MAIZO",0.86f,0xffb2b7d1u);
 hid_diagnostics_line();
}
/* With the CPU backend the boot message can be displayed before the
 * filesystem, HID receiver, controller and cover scanner are initialized. */
static void present(void){maizo_video_present();}
int ui_init(void){return maizo_video_init();}
void ui_boot_message(const char*stage){
 maizo_video_clear();
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
 maizo_video_clear();
 graphite=c->theme==3;
 bright=graphite?0xff84b4b9u:c->theme==1?0xff1cbea8u:c->theme==2?0xffdd6689u:0xff6661eau;
 panel=graphite?0xff222a30u:0xff20283au;
 card=graphite?0xff29343au:0xff28334au;
 muted=graphite?0xffb9c5cbu:0xffadbcd0u;
 background=graphite?0xff111a1fu:0xff121b2du;
 quad(0,0,640,480,background,graphite?0xff1d2930u:0xff202a43u);
 quad(0,0,640,7,bright,bright);
 sidebar(u);titlebar(u,l,hid);
 switch(u->page){case PAGE_HOME:home(u,l);break;case PAGE_LIBRARY:library(u,l,c);break;case PAGE_DETAIL:detail(u,l,c);break;case PAGE_SEARCH:search_ui(u,l);break;case PAGE_SETTINGS:settings(u,u->settings_for_game?&u->game_config:c);break;case PAGE_HID:hid_page();break;case PAGE_CALIBRATION:calibration_ui(u,c);break;case PAGE_WELCOME:welcome(c);break;case PAGE_INFO:
 label(120,86,"AVISO MAIZO",1.8f,0xffffffffu);label(120,156,u->info,1.0f,0xffc9c9dfu);
 label(120,204,"APERTE B PARA VOLTAR",1.05f,0xffaeb1c8u);break;}
 footer(u,l);
 present();
}
void ui_shutdown(void){maizo_video_shutdown();}
