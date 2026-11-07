/* Optional MaizoWii USB Loader GX menu VI width profile. The game
 * patcher has a separate best-effort GXRModeObj scan. Calls are at
 * menu video reconfiguration points (never in frame rendering).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int maizo_loader_menu_vi704_requested(void){
 const char*paths[]={"usb:/maizo/maizo-launch.txt","sd:/maizo/maizo-launch.txt"};
 for(unsigned root=0;root<2;root++){
  FILE*f=fopen(paths[root],"rb");
  if(!f)continue;
  char line[180];int valid=0,sys=-1,force=0,dev=0;
  if(fgets(line,sizeof(line),f)&&!strncmp(line,"MAIZO-LAUNCH/3",14))valid=1;
  while(valid&&fgets(line,sizeof(line),f)){
   if(!strncmp(line,"system=",7))sys=atoi(line+7);
   else if(!strncmp(line,"force_vi704=",12))force=atoi(line+12)==1;
   else if(!strncmp(line,"file=",5)){
    dev=(root==0&&!strncmp(line+5,"usb:/",5))||
        (root==1&&!strncmp(line+5,"sd:/",4));
   }
  }
  fclose(f);
  if(valid && sys==0 && force && dev)return 1;
 }
 return 0;
}
