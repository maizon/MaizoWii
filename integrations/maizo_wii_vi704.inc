/* Wii USB Loader GX optional 704 sample VI output (Maizo only).
 * It changes recognized video mode structures of the loaded Wii game, not
 * the Wii System Menu, IOS or unknown executable memory layouts.
 * This is a best-effort game patch: games can replace modes at runtime.
 */
#include <stdio.h>
#include <stdlib.h>
static int maizo_wii_vi704_requested(void){
 static int requested=-1;
 if(requested>=0)return requested;
 requested=0;
 const char*paths[]={"usb:/maizo/maizo-launch.txt","sd:/maizo/maizo-launch.txt"};
 for(unsigned j=0;j<2;j++){
  FILE*f=fopen(paths[j],"rb");
  if(!f)continue;
  char line[180];int system=-1,force=0,valid=0,dev=0;
  if(fgets(line,sizeof(line),f)&&!strncmp(line,"MAIZO-LAUNCH/3",14))valid=1;
  while(valid&&fgets(line,sizeof(line),f)){
   if(!strncmp(line,"system=",7))system=atoi(line+7);
   else if(!strncmp(line,"force_vi704=",12))force=atoi(line+12)==1;
   else if(!strncmp(line,"file=",5))
    dev=(j==0&&!strncmp(line+5,"usb:/",5))||
        (j==1&&!strncmp(line+5,"sd:/",4));
  }
  fclose(f);
  if(valid&&system==0&&force&&dev){requested=1;break;}
 }
 return requested;
}
static void maizo_patch_game_vi704(u8 *address,int length){
 if(!address || length<(int)sizeof(GXRModeObjRVL))return;
 unsigned altered=0;
 for(int offset=0;offset+(int)sizeof(GXRModeObjRVL)<=length;offset+=4){
  GXRModeObjRVL *m=(GXRModeObjRVL*)(address+offset);
  /* libogc video structures: require layout, sample pattern and valid VI
   * dimensions. No random 16-bit search/replace on game text or data. */
  if(m->viWidth<620||m->viWidth>720||m->fbWidth<512||
     m->fbWidth>720||(m->fbWidth&1)||m->viHeight<224||
     m->viHeight>576||m->xfbHeight<200||m->xfbHeight>576||
     m->efbHeight<200||m->efbHeight>576||
     m->viXOrigin>80||m->field_rendering>1||m->aa>1||
     (m->xfbMode!=VI_XFBMODE_DF && m->xfbMode!=VI_XFBMODE_SF))
   continue;
  if(memcmp(m->sample_pattern,PATTERN,sizeof(PATTERN)) &&
     memcmp(m->sample_pattern,PATTERN_AA,sizeof(PATTERN_AA)))continue;
  m->viWidth=704;
  m->viXOrigin=8;
  altered++;
  offset+=(int)sizeof(GXRModeObjRVL)-4;
 }
 if(altered)gprintf("MAIZO force VI 704: %u Wii game modes\n",altered);
}
