/* MAIZO requested output standard. Wii's video hardware selects the encoder;
 * these modes are only supported for rebuilt DOLs, not stock binaries.
 * Keep framebuffer dimensions untouched when changing 480i/240p TV timing.
 * PAL50 uses 480 active lines centered in 576 to avoid reallocating XFB.
 */
#ifndef MAIZO_TV_STANDARD_INC
#define MAIZO_TV_STANDARD_INC
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* 0 auto, 1 NTSC, 2 PAL50, 3 PAL60 (EURGB60), 4 PAL-M. */
static int maizo_tv_standard_from_launch(void){
 static int cached=-1;
 if(cached>=0)return cached;
 static const char *paths[]={"usb:/maizo/maizo-launch.txt","sd:/maizo/maizo-launch.txt"};
 for(unsigned root=0;root<2;root++){
  FILE *f=fopen(paths[root],"rb");
  if(!f)continue;
  char line[600];
  if(!fgets(line,sizeof(line),f) || strncmp(line,"MAIZO-LAUNCH/3",14)){
   fclose(f);continue;
  }
  int tv=-1,device_ok=0;
  while(fgets(line,sizeof(line),f)){
   if(!strncmp(line,"file=",5)){
    device_ok=(root==0&&!strncmp(line+5,"usb:/",5))||
              (root==1&&!strncmp(line+5,"sd:/",4));
   }else if(!strncmp(line,"tv_standard=",12)){
    int requested=atoi(line+12);
    if(requested>=0&&requested<=4)tv=requested;
   }
  }
  fclose(f);
  if(device_ok&&tv>=0){cached=tv;return tv;}
 }
 return 0; /* Retry on next startup stage if FAT is not mounted yet. */
}
static void maizo_tv_apply_to_mode(GXRModeObj *mode,int standard){
 if(!mode||standard<1||standard>4)return;
 /* Never force interlace over an existing component-cable 480p output. */
 if((mode->viTVMode&3u)==VI_PROGRESSIVE)return;
 int low=mode->xfbHeight<=288;
 const GXRModeObj *reference=NULL;
 switch(standard){
  case 1:reference=low?&TVNtsc240Ds:&TVNtsc480IntDf;break;
  case 2:reference=low?&TVPal264Ds:&TVPal576IntDfScale;break;
  case 3:reference=low?&TVEurgb60Hz240Ds:&TVEurgb60Hz480IntDf;break;
  case 4:reference=low?&TVMpal240Ds:&TVMpal480IntDf;break;
 }
 if(!reference)return;
 mode->viTVMode=reference->viTVMode;
 unsigned lines=standard==2?576u:480u;
 if(mode->viHeight>lines)mode->viHeight=lines;
 mode->viYOrigin=(u16)((lines-mode->viHeight)/2u);
}
#endif
