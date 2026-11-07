/* MAIZO game-only video profile, loaded once at emulation start.
 * Never changes upstream menus or TV standards. Wii/GC modules unaffected.
 * Compile into each supported engine's game renderer, not the frontend. */
#ifndef MAIZO_GAME_VIDEO_INC
#define MAIZO_GAME_VIDEO_INC
#include <stdio.h>
#include <gccore.h>
#include <string.h>
typedef struct {int x,y,w,h;} MaizoGameVideo;
static int maizo_launch_native_output=0;
static int maizo_launch_force_vi704=0;
static MaizoGameVideo maizo_game_video(int lowres){
 static int loaded=0;
 static MaizoGameVideo values[2]={{0,0,100,100},{0,0,100,100}};

 if(!loaded){
  /* Video can initialize before FAT on a child DOL. Retry at later engine
   * initialization points instead of caching a permanent false negative. */
  static const char*roots[]={"usb:/maizo/maizo-launch.txt","sd:/maizo/maizo-launch.txt"};
  FILE*f=NULL;
  for(int i=0;i<2&&!f;i++)f=fopen(roots[i],"rb");
  if(f){
   char line[128];int known=0;
   if(fgets(line,sizeof(line),f)&&!strncmp(line,"MAIZO-LAUNCH/3",14))known=1;
   if(known)loaded=1;
   while(known&&fgets(line,sizeof(line),f)){
    char key[40];int value;
    if(sscanf(line,"%39[^=]=%d",key,&value)!=2)continue;
    if(!strcmp(key,"ps1_native"))maizo_launch_native_output=value==1;
    if(!strcmp(key,"force_vi704"))maizo_launch_force_vi704=value==1;
    for(int i=0;i<2;i++){
     const char *suffix=i?"240":"480";
     char expected[40];
     snprintf(expected,sizeof(expected),"game_x%s",suffix);
     if(!strcmp(key,expected)&&value>=-50&&value<=50)values[i].x=value;
     snprintf(expected,sizeof(expected),"game_y%s",suffix);
     if(!strcmp(key,expected)&&value>=-50&&value<=50)values[i].y=value;
     snprintf(expected,sizeof(expected),"game_w%s",suffix);
     if(!strcmp(key,expected)&&value>=70&&value<=130)values[i].w=value;
     snprintf(expected,sizeof(expected),"game_h%s",suffix);
     if(!strcmp(key,expected)&&value>=70&&value<=130)values[i].h=value;
    }
   }
   fclose(f);
  }
 }
 return values[lowres?1:0];
}
static int maizo_game_native_output(void){
 /* Ensure one read and no FAT traffic in the per-frame draw path. */
 (void)maizo_game_video(0);
 return maizo_launch_native_output;
}
static int maizo_game_force_vi704(void){
 (void)maizo_game_video(0);
 return maizo_launch_force_vi704;
}
/* For WiiSX variants, the NativeOutput argv switches rendering to 240p.
 * Current user mode can also select native inside WiiSX; in that case
 * the engine's own video mode must be consulted instead of this hint. */
static void maizo_video_adjust_vi(GXRModeObj *mode,int lowres){
 if(!mode)return;
 MaizoGameVideo c=maizo_game_video(lowres);
 int old=(int)mode->viWidth;
 int width=maizo_game_force_vi704()?704:old*c.w/100;
 if(width<512)width=512;
 if(width>704)width=704;
 int x=(int)mode->viXOrigin+(old-width)/2+c.x;
 if(x<0)x=0;
 if(x>720-width)x=720-width;
 mode->viWidth=(u16)width;mode->viXOrigin=(u16)x;
}
#endif
