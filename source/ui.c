#include "app.h"
#include "font8x8.h"
#include <ogc/system.h>
#include <ogc/video.h>
#include <ogc/gx.h>
#include <ogc/gu.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
static GXRModeObj*vmode;
static void*framebuffer[2],*fifo;
static int which=0;
#define RGB(r,g,b) (0xff000000u|((unsigned)(r)<<16)|((unsigned)(g)<<8)|(b))
static unsigned background=0xff11111du,panel=0xff1e2030u,bright=0xff6366f1u;
static void vertex(float x,float y,unsigned col){
 GX_Position2f32(x,y);GX_Color4u8((col>>16)&255,(col>>8)&255,col&255,(col>>24)&255);
}
static void quad(float x,float y,float w,float h,unsigned a,unsigned b){
 if(w<=0||h<=0)return;
 GX_Begin(GX_QUADS,GX_VTXFMT0,4);
 vertex(x,y,a);vertex(x+w,y,a);vertex(x+w,y+h,b);vertex(x,y+h,b);
 GX_End();
}
static void roundrect(float x,float y,float w,float h,unsigned col){
 quad(x+6,y,w-12,h,col,col);quad(x,y+6,6,h-12,col,col);quad(x+w-6,y+6,6,h-12,col,col);
}
static void text(float x,float y,const char*str,float z,unsigned color,int max){
 float start=x;
 for(int n=0;str&&*str&&n<max;str++,n++){
  unsigned char c=(unsigned char)*str;if(c>127)c='?';
  if(c=='\n'){y+=10*z;x=start;continue;}
  unsigned bits=0;
  for(int row=0;row<8;row++){unsigned char v=font8x8_basic[c][row];while(v){bits+=(v&1);v>>=1;}}
  if(bits){GX_Begin(GX_QUADS,GX_VTXFMT0,4*bits);
   for(int row=0;row<8;row++)for(int j=0;j<8;j++)if(font8x8_basic[c][row]&(1u<<j)){
    float px=x+j*z,py=y+row*z;vertex(px,py,color);vertex(px+z,py,color);vertex(px+z,py+z,color);vertex(px,py+z,color);
   }GX_End();
  }x+=8.4f*z;
 }
}
static void label(float x,float y,const char*s,float z,unsigned color){text(x,y,s,z,color,70);}
static void clipped(float x,float y,const char*s,float z,unsigned color,int max){text(x,y,s,z,color,max);}
static void titlebar(const State*u,const Library*l,int hid){
 roundrect(104,10,526,56,panel);
 label(124,28,"MAIZO",2.35f,0xffffffffu);
 roundrect(269,24,124,26,bright);label(283,32,"WII EDITION",1.05f,0xffffffffu);
 label(445,29,hid?"CONTROLE USB":"SD / USB",1.08f,0xffb9bbd6);
}
static void sidebar(const State*u){
 roundrect(10,10,84,460,panel);roundrect(22,22,60,60,bright);label(39,45,"M",2.0f,0xffffffffu);
 const char*items[]={"HOME","JOGOS","BUSCA","AJUSTES"};
 int active=u->page==PAGE_HOME?0:(u->page==PAGE_LIBRARY||u->page==PAGE_DETAIL)?1:u->page==PAGE_SEARCH?2:3;
 for(int i=0;i<4;i++){int y=112+i*70;
  if(i==active)roundrect(17,y,70,56,0xff35335fu);
  label(27,y+20,items[i],i==3?0.96f:1.00f,i==active?0xffffffffu:0xffabb0c9u);}
 label(28,437,"v0.1",1.0f,0xff8589a5u);
}
static void footer(const State*u,const Library*l){
 roundrect(104,440,526,30,panel);
 label(118,451,"A ABRIR   B VOLTAR   L/R SISTEMA   X BUSCA   Y FAVORITO",0.78f,0xffb3b8d2u);
}
static void home(const State*u,const Library*l){
 label(116,78,"SUA CENTRAL DE JOGOS",1.52f,0xfff6f6ffu);
 label(116,103,"ESCOLHA UM SISTEMA - 100% NAVEGAVEL POR CONTROLE",0.89f,0xffb8b9d7u);
 const unsigned colors[]={0xff16a7b8u,0xff7864dcu,0xffdc6296u,0xff6e77ddu,0xffd5a14bu,0xff45aa84u,0xff60a1d6u,0xff22a6a6u};
 const char*symbols[]={"WII","GC","PS1","SNES","NES","GBA","GB","TV"};
 for(int i=0;i<SYSTEM_COUNT;i++){
  float x=112+(i%4)*129.0f,y=123+(i/4)*148.0f;
  int focus=(int)u->system==i;
  if(focus)roundrect(x-3,y-3,128,143,bright);
  roundrect(x,y,122,137,0xff24263du);
  quad(x+6,y+5,110,69,colors[i],0xff1c243bu);
  int idx=library_visible(l,(System)i,"",0,0);
  if(idx<0||!cover_draw(&l->games[idx],x+36,y+6,49,64))
   clipped(x+27,y+26,symbols[i],1.62f,0xffffffffu,6);
  clipped(x+7,y+83,system_title((System)i),0.77f,0xfff9f9ffu,17);
  char buf[28];snprintf(buf,sizeof(buf),"%d JOGOS",library_total(l,(System)i,"",0));
  label(x+7,y+102,buf,0.89f,0xffb6b6d2u);
  if(!module_available((System)i))label(x+7,y+118,"MODULO AUSENTE",0.63f,0xffffa8acu);
 }
 label(117,424,"L/R TROCAR SISTEMA  A ABRIR  START AJUSTES",0.91f,0xffaeb2d1u);
}
static void library(const State*u,const Library*l,const Config*c){
 label(117,77,system_title(u->system),1.54f,0xffffffffu);
 char count[76];snprintf(count,sizeof(count),"%d ITENS  %s  %s",
 library_total(l,u->system,u->search,u->favorites),u->favorites?"FAVORITOS":"",
 c->layout?"LISTA":"GRADE");
 label(119,106,count,0.98f,0xffa6a9c9u);
 if(!c->layout){
  for(int i=0;i<8;i++){
   int pos=u->scroll+i,idx=library_visible(l,u->system,u->search,u->favorites,pos);
   if(idx<0)break;const Game*g=&l->games[idx];
   float x=115+(i%4)*127.0f,y=133+(i/4)*136.0f;
   roundrect(x,y,120,129,pos==u->selected?bright:0xff27283au);
   roundrect(x+5,y+5,110,89,0xff343750u);
   if(!cover_draw(g,x+23,y+6,74,86))
    clipped(x+32,y+40,"JOGO",1.33f,0xffffffffu,7);
   clipped(x+7,y+104,g->name,0.77f,0xfff7f7ffu,17);
   if(g->favorite)label(x+107,y+105,"*",0.98f,0xffffd783u);
  }
 }else{
  for(int i=0;i<8;i++){
   int pos=u->scroll+i,idx=library_visible(l,u->system,u->search,u->favorites,pos);
   if(idx<0)break;const Game*g=&l->games[idx];int y=132+i*36;
   if(pos==u->selected)roundrect(113,y,509,34,bright);
   else if(i%2==0)roundrect(113,y,509,34,0xff27283au);
   clipped(125,y+11,g->name,1.08f,0xfff7f7ffu,48);
   if(g->favorite)label(597,y+11,"*",1.2f,0xffffd783u);
  }
 }
 if(l->scanning)clipped(119,418,l->status,0.87f,0xffc3b8eeu,56);
}
static void detail(const State*u,const Library*l,const Config*c){
 if(u->detail<0||u->detail>=l->count)return;const Game*g=&l->games[u->detail];
 label(119,89,"DETALHES DO ARQUIVO",1.4f,0xffb6b7cfu);
 roundrect(119,124,170,247,0xff393451u);roundrect(126,132,156,233,0xff6c5bd3u);
 if(!cover_draw(g,126,132,156,233))label(176,235,g->system==SYSTEM_GC?"GC":g->system==SYSTEM_PS1?"PS1":"MEDIA",1.7f,0xffffffffu);
 clipped(310,135,g->name,1.52f,0xffffffffu,24);
 label(310,176,module_name(g->system),1.13f,0xffb9bcd9u);
 if(g->id[0]){char b[28];snprintf(b,sizeof(b),"ID: %.6s",g->id);label(310,199,b,1.1f,0xffb9bcd9u);}
 clipped(310,230,g->path,0.85f,0xffb1b2ccu,37);
 roundrect(313,291,269,48,module_available(g->system)?0xff4fbc91u:0xff73536bu);
 label(345,307,module_available(g->system)?"A  INICIAR":"MODULO AUSENTE",1.39f,0xffffffffu);
 label(313,358,"Y FAVORITO  B VOLTAR",1.1f,0xffb9bcd9u);
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
static void settings(const State*u,const Config*c){
 label(119,77,"CONFIGURACOES",1.53f,0xffffffffu);
 clipped(119,105,system_title(u->system),1.07f,0xffb4b8d6u,35);
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
int ui_init(void){
 VIDEO_Init();vmode=VIDEO_GetPreferredMode(NULL);if(!vmode)return 0;
 framebuffer[0]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
 framebuffer[1]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
 if(!framebuffer[0]||!framebuffer[1])return 0;
 VIDEO_Configure(vmode);VIDEO_SetNextFramebuffer(framebuffer[0]);VIDEO_SetBlack(FALSE);VIDEO_Flush();VIDEO_WaitVSync();
 fifo=memalign(32,256*1024);if(!fifo)return 0;GX_Init(fifo,256*1024);
 GX_SetCopyClear((GXColor){0,0,0,255},0x00ffffff);
 GX_SetViewport(0,0,vmode->fbWidth,vmode->efbHeight,0,1);
 GX_SetScissor(0,0,vmode->fbWidth,vmode->efbHeight);
 GX_SetDispCopySrc(0,0,vmode->fbWidth,vmode->efbHeight);
 GX_SetDispCopyDst(vmode->fbWidth,vmode->xfbHeight);
 GX_SetDispCopyYScale((f32)vmode->xfbHeight/(f32)vmode->efbHeight);
 GX_SetCopyFilter(vmode->aa,vmode->sample_pattern,GX_TRUE,vmode->vfilter);
 GX_SetFieldMode(vmode->field_rendering,((vmode->viHeight==2*vmode->xfbHeight)?GX_ENABLE:GX_DISABLE));
 GX_SetPixelFmt(GX_PF_RGB8_Z24,GX_ZC_LINEAR);GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);
 GX_SetNumChans(1);GX_SetNumTexGens(0);GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
 GX_SetBlendMode(GX_BM_BLEND,GX_BL_SRCALPHA,GX_BL_INVSRCALPHA,GX_LO_CLEAR);
 GX_SetCullMode(GX_CULL_NONE);GX_ClearVtxDesc();GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
 GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XY,GX_F32,0);
 GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
 Mtx44 p;guOrtho(p,0,480,0,640,0,1);GX_LoadProjectionMtx(p,GX_ORTHOGRAPHIC);
 GX_SetColorUpdate(GX_TRUE);return 1;
}
void ui_draw(const State*u,const Library*l,const Config*c,int hid){
 bright=c->theme==1?0xff1cbea8u:c->theme==2?0xffdd6689u:0xff6661eau;
 panel=0xff202132u;background=0xff121420u;
 quad(0,0,640,480,background,0xff171b2eu);
 quad(0,0,640,7,bright,bright);
 sidebar(u);titlebar(u,l,hid);
 switch(u->page){case PAGE_HOME:home(u,l);break;case PAGE_LIBRARY:library(u,l,c);break;case PAGE_DETAIL:detail(u,l,c);break;case PAGE_SEARCH:search_ui(u,l);break;case PAGE_SETTINGS:settings(u,c);break;case PAGE_INFO:
 label(120,86,"AVISO MAIZO",1.8f,0xffffffffu);label(120,156,u->info,1.0f,0xffc9c9dfu);
 label(120,204,"APERTE B PARA VOLTAR",1.05f,0xffaeb1c8u);break;}
 footer(u,l);GX_DrawDone();GX_CopyDisp(framebuffer[which],GX_TRUE);
 VIDEO_SetNextFramebuffer(framebuffer[which]);VIDEO_Flush();which^=1;
}
void ui_shutdown(void){VIDEO_SetBlack(TRUE);VIDEO_Flush();VIDEO_WaitVSync();if(fifo){free(fifo);fifo=NULL;}}
