/* MaizoWii -> adapted WiiMC-SSLC settings bridge.
 * Read a single validated launch record only. Never edit network shares,
 * passwords, the original settings.xml or IOS. The official fallback is
 * deliberately untouched; this is injected solely into rebuilt WiiMC.
 * The profile is applied AFTER DefaultSettings() and XML load.
 */
#ifndef MAIZO_WIIMC_SETTINGS_BRIDGE
#define MAIZO_WIIMC_SETTINGS_BRIDGE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* Defined in the adapted WiiMC video.cpp; never modifies the system VI. */
void MaizoForceVI704(int enabled);
static const char *const maizo_wiimc_keys[]={
 "videoZoomHor",
 "videoZoomVert",
 "videoXshift",
 "videoYshift",
 "frameDropping",
 "cacheFill",
 "audioDelay",
 "autoResume",
 "autoPlayNextVideo",
 "skipBackward",
 "skipForward",
 "videoFull",
 "audioNorm",
 "videoDf",
 "viWidth",
 "skipLoop",
 "interlaceHandle",
 "dvdMenu",
 "dvdDisabled",
 "subtitleVisibility",
 "subtitleDelay",
 "subtitleSize",
 "bold",
 "monofont",
 "doubleStrike",
 "tiledRender",
 "saveExit",
 "hideExtensions",
 "libass",
 "force576p",
 "playOrder",
 "nativeLoops",
 "onlineCacheFill",
 "screensaverArt",
 "rumble",
};
typedef char maizo_media_option_count_check[(sizeof(maizo_wiimc_keys)/sizeof(maizo_wiimc_keys[0])==35)?1:-1];
static void maizo_wiimc_apply_profile(void){
 const char *paths[]={"usb:/maizo/maizo-launch.txt","sd:/maizo/maizo-launch.txt"};
 for(unsigned root=0;root<2;root++){
  FILE *f=fopen(paths[root],"rb");
  if(!f)continue;
  char line[256]={0};
  if(!fgets(line,sizeof(line),f)||strncmp(line,"MAIZO-LAUNCH/3",14)){fclose(f);continue;}
  int correct_system=0,correct_device=0,enabled=0,force704=0,volume=-1;
  int values[35]={0},found[35]={0};
  while(fgets(line,sizeof(line),f)){
   if(!strncmp(line,"system=",7))correct_system=atoi(line+7)==7;
   else if(!strncmp(line,"file=",5)){
    correct_device=(root==0&&!strncmp(line+5,"usb:/",5))||
                   (root==1&&!strncmp(line+5,"sd:/",4));
   }
   else if(!strncmp(line,"wiimc_apply=",12))enabled=atoi(line+12)==1;
   else if(!strncmp(line,"force_vi704=",12))force704=atoi(line+12)==1;
   else if(!strncmp(line,"media_volume=",13)){
    int v=atoi(line+13);if(v>=0&&v<=100)volume=v;
   }else if(!strncmp(line,"wiimc_",6)){
    char key[64];int v;
    if(sscanf(line+6,"%63[^=]=%d",key,&v)==2){
     for(unsigned i=0;i<35;i++)if(!strcmp(key,maizo_wiimc_keys[i])){
      values[i]=v;found[i]=1;break;
     }
    }
   }
  }
  fclose(f);
  if(!correct_system||!correct_device)continue;
  if(force704)MaizoForceVI704(1);
  if(!enabled)break;
  if(volume>=0)WiiSettings.volume=volume;
  if(found[0]){int v=values[0];WiiSettings.videoZoomHor=v/100.0f;}
  if(found[1]){int v=values[1];WiiSettings.videoZoomVert=v/100.0f;}
  if(found[2]){int v=values[2];WiiSettings.videoXshift=v-40;}
  if(found[3]){int v=values[3];WiiSettings.videoYshift=v-40;}
  if(found[4]){int v=values[4];WiiSettings.frameDropping=v;}
  if(found[5]){int v=values[5];WiiSettings.cacheFill=v;}
  if(found[6]){int v=values[6];WiiSettings.audioDelay=(v-40)/10.0f;}
  if(found[7]){int v=values[7];WiiSettings.autoResume=v;}
  if(found[8]){int v=values[8];WiiSettings.autoPlayNextVideo=v;}
  if(found[9]){int v=values[9];WiiSettings.skipBackward=v;}
  if(found[10]){int v=values[10];WiiSettings.skipForward=v;}
  if(found[11]){int v=values[11];WiiSettings.videoFull=v;}
  if(found[12]){int v=values[12];WiiSettings.audioNorm=v;}
  if(found[13]){int v=values[13];WiiSettings.videoDf=v;}
  if(found[14]){int v=values[14];WiiSettings.viWidth=v;}
  if(found[15]){int v=values[15];WiiSettings.skipLoop=v;}
  if(found[16]){int v=values[16];WiiSettings.interlaceHandle=v;}
  if(found[17]){int v=values[17];WiiSettings.dvdMenu=v;}
  if(found[18]){int v=values[18];WiiSettings.dvdDisabled=v;}
  if(found[19]){int v=values[19];WiiSettings.subtitleVisibility=v;}
  if(found[20]){int v=values[20];WiiSettings.subtitleDelay=(v-40)/10.0f;}
  if(found[21]){int v=values[21];WiiSettings.subtitleSize=v/10.0f;}
  if(found[22]){int v=values[22];WiiSettings.bold=v;}
  if(found[23]){int v=values[23];WiiSettings.monofont=v;}
  if(found[24]){int v=values[24];WiiSettings.doubleStrike=v;}
  if(found[25]){int v=values[25];WiiSettings.tiledRender=v;}
  if(found[26]){int v=values[26];WiiSettings.saveExit=v;}
  if(found[27]){int v=values[27];WiiSettings.hideExtensions=v;}
  if(found[28]){int v=values[28];WiiSettings.libass=v;}
  if(found[29]){int v=values[29];WiiSettings.force576p=v;}
  if(found[30]){int v=values[30];WiiSettings.playOrder=v;}
  if(found[31]){int v=values[31];WiiSettings.nativeLoops=v;}
  if(found[32]){int v=values[32];WiiSettings.onlineCacheFill=v;}
  if(found[33]){int v=values[33];WiiSettings.screensaverArt=v;}
  if(found[34]){int v=values[34];WiiSettings.rumble=v;}
  break;
 }
}
#endif
