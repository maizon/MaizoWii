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
 label(456,29,hid?"HID CONECTADO":"USB / SD",1.10f,0xffb9bbd6);
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
 label(118,451,"A SELECIONAR  B VOLTAR  X BUSCAR  Y FAVORITO",0.93f,0xffb3b8d2u);
}
static void home(const State*u,const Library*l){
 label(116,83,"ESCOLHA SUA PLATAFORMA",1.55f,0xfff6f6ffu);
 const char*names[]={"GAMECUBE","PLAYSTATION 1","MULTIMIDIA"};
 const unsigned colors[]={0xff7864dcu,0xffdc6296u,0xff22a6a6u};
 const char*art[]={"GC","PS","TV"};
 for(int i=0;i<3;i++){
  float x=111+i*173.0f;int focus=(int)u->system==i;
  if(focus)roundrect(x-3,126,168,250,bright);
  roundrect(x,129,162,244,0xff22253bu);
  quad(x+7,137,148,152,colors[i],0xff20203fu);
  int cover_index=library_visible(l,(System)i,"",0,0);
  if(cover_index<0||!cover_draw(&l->games[cover_index],x+23,143,118,140))label(x+55,197,art[i],2.8f,0xffffffffu);
  clipped(x+12,304,names[i],1.18f,0xfff9f9ffu,20);
  char buf[40];snprintf(buf,sizeof(buf),"%d ARQUIVOS",library_total(l,(System)i,"",0));
  label(x+12,338,buf,0.96f,0xffb3b7d0u);
 }
 label(122,401,"NAVEGUE COM L/R OU ESQUERDA/DIREITA",1.06f,0xff9da3bcu);
}
static void library(const State*u,const Library*l,const Config*c){
 const char*sys[]={"GAMECUBE","PLAYSTATION 1","VIDEOS E MUSICAS"};
 label(117,81,sys[u->system],1.80f,0xffffffffu);
 char count[60];snprintf(count,sizeof(count),"%d ITENS %s",library_total(l,u->system,u->search,u->favorites),u->favorites?"- FAVORITOS":"");
 label(119,115,count,1.02f,0xffa6a9c9u);
 for(int i=0;i<8;i++){int index=u->scroll+i;
  int idx=library_visible(l,u->system,u->search,u->favorites,index);
  if(idx<0)break;const Game*g=&l->games[idx];int y=138+i*36;
  if(index==u->selected)roundrect(113,y,509,34,bright);
  else if(i%2==0)roundrect(113,y,509,34,0xff27283au);
  clipped(125,y+11,g->name,1.19f,0xfff7f7ffu,42);
  if(g->favorite)label(597,y+11,"*",1.2f,0xffffd783u);
 }
 if(l->scanning)label(119,420,l->status,0.96f,0xffc3b8eeu);
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
 label(119,85,"BUSCAR NA BIBLIOTECA",1.55f,0xffffffffu);
 roundrect(119,125,490,55,0xff33334fu);label(132,144,u->search[0]?u->search:"DIGITE UM TITULO",1.5f,0xffffffffu);
 label(119,203,"ESQUERDA/DIREITA = LETRA, A = INSERIR, B = APAGAR",1.04f,0xffaeb0c8u);
 roundrect(119,240,489,84,0xff202334u);
 char cur[32];snprintf(cur,sizeof(cur),"[ %c ]",characters[u->search_index]);
 label(288,257,cur,2.9f,0xffc0adffu);
 label(119,353,"START = RESULTADOS  L/R = PLATAFORMA",1.12f,0xffaeb0c8u);
}
static const char*setting_name[]={"TEMA","MODO DE BIBLIOTECA","GC MEMORY CARD","GC WIDESCREEN","GC CHEATS","GC SKIP IPL","GC VIDEO MODE","PS1 INTERPRETADOR","PS1 FRAME SKIP","PS1 AUDIO","MEDIA VOLUME","VOLTAR"};
static void settings(const State*u,const Config*c){
 label(119,82,"CONFIGURACOES",1.65f,0xffffffffu);
 char val[44];
 for(int i=0;i<12;i++){int y=114+i*26;
  if(i==u->settings_row)roundrect(116,y,498,25,bright);
  clipped(124,y+8,setting_name[i],1.02f,0xffffffffu,28);
  unsigned v=0;switch(i){case 0:v=c->theme;break;case 1:v=c->layout;break;case 2:v=c->gc_memcard;break;case 3:v=c->gc_wide;break;case 4:v=c->gc_cheats;break;case 5:v=c->gc_skip_ipl;break;case 6:v=c->gc_video;break;case 7:v=c->ps1_interpreter;break;case 8:v=c->ps1_frameskip;break;case 9:v=c->ps1_audio;break;case 10:v=c->media_volume;break;}
  if(i==0)snprintf(val,sizeof(val),"%s",v==0?"VIOLETA":v==1?"TURQUESA":"CORAL");
  else if(i==1)snprintf(val,sizeof(val),"%s",v?"LISTA":"CARDS");
  else if(i==6)snprintf(val,sizeof(val),"%s",v==0?"AUTO":v==1?"MPAL":v==2?"NTSC":"PAL50");
  else if(i==10)snprintf(val,sizeof(val),"%u%%",v);
  else if(i==11)snprintf(val,sizeof(val),"B / A");
  else snprintf(val,sizeof(val),"%s",v?"ON":"OFF");
  label(481,y+8,val,1.02f,0xffffffffu);
 }
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
