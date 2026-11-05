/* Wii image ID6 probing. Portable C, used by the on-device scanner. */
#include "wii_id.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
int maizo_wii_id_valid(const char *id){
 if(!id||strlen(id)!=6)return 0;
 for(int i=0;i<6;i++)if(!((id[i]>='A'&&id[i]<='Z')||
                             (id[i]>='0'&&id[i]<='9')))return 0;
 return 1;
}
int maizo_wii_id_from_file(const char *path,char id[7]){
 if(!path||!id)return 0;
 const char *ext=strrchr(path,'.');
 if(!ext|| (strcasecmp(ext,".wbfs")&&strcasecmp(ext,".iso")))return 0;
 FILE *f=fopen(path,"rb");
 if(!f)return 0;
 const long offset=strcasecmp(ext,".wbfs")==0?0x200L:0L;
 char header[7]={0};int ok=0;
 if(offset && fseek(f,0,SEEK_SET)==0){
  char magic[4]={0};
  if(fread(magic,1,4,f)!=4||memcmp(magic,"WBFS",4)!=0){fclose(f);return 0;}
 }
 if(fseek(f,offset,SEEK_SET)==0 && fread(header,1,6,f)==6){
  header[6]=0;
  if(maizo_wii_id_valid(header)){memcpy(id,header,7);ok=1;}
 }
 fclose(f);
 return ok;
}
