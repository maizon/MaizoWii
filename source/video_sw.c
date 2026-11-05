/* MAIZO software-only 2D VI backend.
 * Wii XFB is an uncached YUYV 4:2:2 buffer, not a linear RGB surface.
 * This path deliberately avoids a GX FIFO, draw-done waits and EFB copies:
 * those operations regressed to a black screen on Dolphin and on real Wii.
 */
#include "app.h"
#include <ogc/video.h>
#include <string.h>
#include <ogc/system.h>
#include <stdint.h>
#include <stddef.h>
/* Own a writable mode snapshot: the shared preferred template must never
 * be mutated because it is reused by libogc and other homebrew code. */
static GXRModeObj video_mode,boot_video_mode;
static GXRModeObj *mode;
static unsigned original_vi_width=640,original_vi_x=40;
static void *buffers[2];
static int back=0, visible=0;
static int cal_x=0,cal_y=0;
static unsigned cal_w=100,cal_h=100;
static void apply_vi_geometry(void);
/* Frontend output only. Other DOLs reconfigure the VI on their own. */
int maizo_video_frontend_select(unsigned selection){
 if(!mode||selection>2)return 0;
 video_mode=boot_video_mode;
 if(selection==1){
  video_mode.viTVMode=(video_mode.viTVMode&~3u)|VI_INTERLACE;
  video_mode.xfbMode=VI_XFBMODE_DF;
 }else if(selection==2){
  video_mode.viTVMode=(video_mode.viTVMode&~3u)|VI_NON_INTERLACE;
  video_mode.viHeight/=2;
  video_mode.xfbHeight/=2;
  video_mode.efbHeight/=2;
  video_mode.viYOrigin/=2;
  video_mode.xfbMode=VI_XFBMODE_SF;
  video_mode.field_rendering=0;
 }
 original_vi_width=video_mode.viWidth;
 original_vi_x=video_mode.viXOrigin;
 apply_vi_geometry();return 1;
}
/* Geometry is not just a framebuffer crop: the video interface itself
 * outputs a wider active region. This can remove overscan/black side bands
 * that drawing wider inside an unchanged 640px VI window cannot touch.
 * Other DOLs reconfigure VI themselves and are NOT affected. */
static void apply_vi_geometry(void){
 if(!mode)return;
 unsigned width=original_vi_width*cal_w/100u;
 if(width<512u)width=512u;
 if(width>704u)width=704u; /* <= Wii VI 720 active-pixel ceiling */
 int center=(int)original_vi_x+(int)original_vi_width/2;
 int x=center-(int)width/2+cal_x;
 if(x<0)x=0;
 if(x>720-(int)width)x=720-(int)width;
 mode->viWidth=(u16)width;
 mode->viXOrigin=(u16)x;
 VIDEO_Configure(mode);
 VIDEO_Flush();
}
void maizo_video_set_calibration(int x,int y,unsigned w,unsigned h){
 cal_x=x<-50?-50:x>50?50:x;
 cal_y=y<-50?-50:y>50?50:y;
 cal_w=w<70?70:w>130?130:w;
 cal_h=h<70?70:h>130?130:h;
 apply_vi_geometry();
}
void maizo_video_clear(void){
 if(!mode||!buffers[back])return;
 uint32_t *dst=(uint32_t *)buffers[back];
 size_t count=((size_t)mode->fbWidth*mode->xfbHeight)/2u;
 for(size_t i=0;i<count;i++)dst[i]=0x10801080u;
}
static int limit8(int value){return value<0?0:value>255?255:value;}
static uint32_t yuyv_pair(int r0,int g0,int b0,int r1,int g1,int b1){
 int y0=16+((66*r0+129*g0+25*b0+128)>>8);
 int y1=16+((66*r1+129*g1+25*b1+128)>>8);
 int u=128+((-38*(r0+r1)-74*(g0+g1)+112*(b0+b1)+256)>>9);
 int v=128+((112*(r0+r1)-94*(g0+g1)-18*(b0+b1)+256)>>9);
 return ((uint32_t)limit8(y0)<<24)|((uint32_t)limit8(u)<<16)|
        ((uint32_t)limit8(y1)<<8)|(uint32_t)limit8(v);
}
/* Keep the untouched neighbor's luma when a 1px font/edge falls on
 * just half of a YUYV pair. Both pixels still share chroma as required. */
static void set_pair(uint32_t *dest,int pair,int x0,int x1,uint32_t color){
 int left=pair*2;
 if(left>=x0&&left+1<x1)dest[pair]=color;
 else if(left>=x0)
  dest[pair]=(dest[pair]&0x0000ff00u)|(color&0xffff00ffu);
 else
  dest[pair]=(dest[pair]&0xff000000u)|(color&0x00ffffffu);
}
static void rect_coords(float x,float y,float w,float h,
                        int *x0,int *y0,int *x1,int *y1){
 float sx=mode->fbWidth/640.0f;
 float sy=mode->xfbHeight/480.0f;
 /* X width and offset are handled at the real VI output stage;
  * applying them here as well would double-scale and clip the canvas. */
 *x0=(int)(x*sx+0.5f);
 *x1=(int)((x+w)*sx+0.5f);
 *y0=(int)((240.0f+(y-240.0f)*cal_h/100.0f+cal_y)*sy+0.5f);
 *y1=(int)((240.0f+(y+h-240.0f)*cal_h/100.0f+cal_y)*sy+0.5f);
 if(*x0<0)*x0=0;
 if(*y0<0)*y0=0;
 if(*x1<0)*x1=0;
 if(*y1<0)*y1=0;
 if(*x1>(int)mode->fbWidth)*x1=mode->fbWidth;
 if(*y1>(int)mode->xfbHeight)*y1=mode->xfbHeight;
}
int maizo_video_init(void){
 VIDEO_Init();
 GXRModeObj *preferred=VIDEO_GetPreferredMode(NULL);
 if(!preferred||!preferred->fbWidth||!preferred->xfbHeight||
    (preferred->fbWidth&1))return 0;
 video_mode=*preferred;
 boot_video_mode=*preferred;
 mode=&video_mode;
 original_vi_width=mode->viWidth;
 original_vi_x=mode->viXOrigin;
 buffers[0]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
 buffers[1]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
 if(!buffers[0]||!buffers[1])return 0;
 back=0;visible=0;
 apply_vi_geometry();
 VIDEO_SetBlack(TRUE);
 VIDEO_Flush();
 VIDEO_WaitVSync();
 return 1;
}
void maizo_video_rect(float x,float y,float w,float h,unsigned a,unsigned b){
 if(!mode||w<=0||h<=0)return;
 int x0,y0,x1,y1;
 rect_coords(x,y,w,h,&x0,&y0,&x1,&y1);
 if(x0>=x1||y0>=y1)return;
 int height=y1-y0;
 int ar=(a>>16)&255,ag=(a>>8)&255,ab=a&255;
 int br=(b>>16)&255,bg=(b>>8)&255,bb=b&255;
 /* The UI has thousands of solid rectangles (especially tiny font
  * pixels). Color conversion and 3 divisions per scanline for each solid
  * rectangle were unnecessary. Convert solid colors exactly once. */
 const int solid=a==b;
 const uint32_t solid_color=yuyv_pair(ar,ag,ab,ar,ag,ab);
 for(int py=y0;py<y1;py++){
  uint32_t color=solid_color;
  if(!solid){
   int row=py-y0;
   int r=(ar*(height-row)+br*row)/height;
   int g=(ag*(height-row)+bg*row)/height;
   int bl=(ab*(height-row)+bb*row)/height;
   color=yuyv_pair(r,g,bl,r,g,bl);
  }
  uint32_t *dest=(uint32_t *)((uint8_t *)buffers[back]+
                             (size_t)py*mode->fbWidth*2u);
  for(int pair=x0/2;pair<(x1+1)/2;pair++)
   set_pair(dest,pair,x0,x1,color);
 }
}
void maizo_video_cover(const unsigned char *source,unsigned tex_w,unsigned tex_h,
                       float x,float y,float w,float h){
 if(!mode||!source||tex_w!=128||(tex_h!=160&&tex_h!=176)||w<=0||h<=0)return;
 int x0,y0,x1,y1;
 rect_coords(x,y,w,h,&x0,&y0,&x1,&y1);
 if(x0>=x1||y0>=y1)return;
 /* Wii VI framebuffer <=720px. Precalculate the horizontal nearest-neighbor
  * coordinate once per cover instead of dividing twice for every pixel
  * on every scanline (tens of thousands of divisions per card). */
 unsigned char tex_x[720];
 const int width=(int)mode->fbWidth;
 if(width>720)return;
 for(int px=x0;px<x1;px++){
  int tx=(px-x0)*128/(x1-x0);
  tex_x[px]=(unsigned char)(tx>127?127:tx);
 }
 if(x0&1)tex_x[x0-1]=0;
 if((x1&1)&&x1<width)tex_x[x1]=127;
 /* MZG1 files store 128x160 (legacy) or 128x176 GX 4x4 tiles.
  * We decode directly to YUYV, so existing saved covers stay compatible. */
 for(int py=y0;py<y1;py++){
  int ty=(py-y0)*(int)tex_h/(y1-y0);
  if(ty>=(int)tex_h)ty=(int)tex_h-1;
  uint32_t *dest=(uint32_t *)((uint8_t *)buffers[back]+
                             (size_t)py*mode->fbWidth*2u);
  for(int pair=x0/2;pair<(x1+1)/2;pair++){
   int rgb[2][3];
   for(int side=0;side<2;side++){
    int px=pair*2+side;
    int tx=tex_x[px];
    int base=((ty/4)*32+tx/4)*64+
             (((ty&3)*4+(tx&3))*2);
    rgb[side][0]=source[base+1];
    rgb[side][1]=source[base+32];
    rgb[side][2]=source[base+33];
   }
   set_pair(dest,pair,x0,x1,
            yuyv_pair(rgb[0][0],rgb[0][1],rgb[0][2],
                      rgb[1][0],rgb[1][1],rgb[1][2]));
  }
 }
}
void maizo_video_present(void){
 VIDEO_SetNextFramebuffer(buffers[back]);
 if(!visible){VIDEO_SetBlack(FALSE);visible=1;}
 VIDEO_Flush();
 VIDEO_WaitVSync();
 back^=1;
}
void maizo_video_shutdown(void){
 VIDEO_SetBlack(TRUE);VIDEO_Flush();VIDEO_WaitVSync();
}
