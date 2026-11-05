/* MAIZO software-only 2D VI backend.
 * Wii XFB is an uncached YUYV 4:2:2 buffer, not a linear RGB surface.
 * This path deliberately avoids a GX FIFO, draw-done waits and EFB copies:
 * those operations regressed to a black screen on Dolphin and on real Wii.
 */
#include "app.h"
#include <ogc/video.h>
#include <ogc/system.h>
#include <stdint.h>
#include <stddef.h>
static GXRModeObj *mode;
static void *buffers[2];
static int back=0, visible=0;
static int cal_x=0,cal_y=0;
static unsigned cal_w=100,cal_h=100;
void maizo_video_set_calibration(int x,int y,unsigned w,unsigned h){
 cal_x=x<-50?-50:x>50?50:x;
 cal_y=y<-50?-50:y>50?50:y;
 cal_w=w<70?70:w>130?130:w;
 cal_h=h<70?70:h>130?130:h;
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
 *x0=(int)((320.0f+(x-320.0f)*cal_w/100.0f+cal_x)*sx+0.5f);
 *x1=(int)((320.0f+(x+w-320.0f)*cal_w/100.0f+cal_x)*sx+0.5f);
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
 mode=VIDEO_GetPreferredMode(NULL);
 if(!mode||!mode->fbWidth||!mode->xfbHeight||(mode->fbWidth&1))return 0;
 buffers[0]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
 buffers[1]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
 if(!buffers[0]||!buffers[1])return 0;
 back=0;visible=0;
 VIDEO_Configure(mode);
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
 for(int py=y0;py<y1;py++){
  int row=py-y0;
  int r=(ar*(height-row)+br*row)/height;
  int g=(ag*(height-row)+bg*row)/height;
  int bl=(ab*(height-row)+bb*row)/height;
  uint32_t color=yuyv_pair(r,g,bl,r,g,bl);
  uint32_t *dest=(uint32_t *)((uint8_t *)buffers[back]+
                             (size_t)py*mode->fbWidth*2u);
  for(int pair=x0/2;pair<(x1+1)/2;pair++)
   set_pair(dest,pair,x0,x1,color);
 }
}
void maizo_video_cover(const unsigned char *source,
                       float x,float y,float w,float h){
 if(!mode||!source||w<=0||h<=0)return;
 int x0,y0,x1,y1;
 rect_coords(x,y,w,h,&x0,&y0,&x1,&y1);
 if(x0>=x1||y0>=y1)return;
 /* MZG1 files store 128x160 RGBA8 in GX 4x4-tiled AR/GB planes.
  * We decode directly to YUYV, so existing saved covers stay compatible. */
 for(int py=y0;py<y1;py++){
  int ty=(py-y0)*160/(y1-y0);
  if(ty>159)ty=159;
  uint32_t *dest=(uint32_t *)((uint8_t *)buffers[back]+
                             (size_t)py*mode->fbWidth*2u);
  for(int pair=x0/2;pair<(x1+1)/2;pair++){
   int rgb[2][3];
   for(int side=0;side<2;side++){
    int px=pair*2+side;
    int tx=(px-x0)*128/(x1-x0);
    if(tx<0)tx=0;
    if(tx>127)tx=127;
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
