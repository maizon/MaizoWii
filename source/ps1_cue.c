/* Prevent PS1 autoboot of missing or malformed disc/track images.
 * This is not a replacement for the WiiSXRX disc decoder. */
#include "ps1_cue.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
static int fail(char*error,size_t cap,const char*msg){
 if(error&&cap)snprintf(error,cap,"%s",msg);
 return 0;
}
static int cue_extension(const char*p){
 const char*d=strrchr(p,'.');
 return d && !strcasecmp(d,".cue");
}
int maizo_probe_ps1_image(const char*path,char*error,size_t cap){
 if(!path||!*path)return fail(error,cap,"Caminho de disco PS1 vazio");
 FILE *f=fopen(path,"rb");
 if(!f)return fail(error,cap,"Imagem PS1 ausente/inacessivel");
 if(!cue_extension(path)){
  unsigned char magic[16];
  size_t n=fread(magic,1,sizeof(magic),f);
  int bad=ferror(f);fclose(f);
  if(bad||n!=sizeof(magic))return fail(error,cap,"Imagem PS1 vazia/incompleta");
  return 1;
 }
 char line[1024];unsigned count=0;
 const char*slash=strrchr(path,'/');
 const size_t dirlen=slash?(size_t)(slash-path)+1:0;
 while(fgets(line,sizeof(line),f)){
  char *p=line;
  while(*p==' '||*p=='\t')p++;
  if(strncasecmp(p,"FILE",4) || !isspace((unsigned char)p[4]))continue;
  p+=4;while(*p==' '||*p=='\t')p++;
  char name[512];size_t n=0;
  int quoted=*p=='"';
  if(quoted)p++;
  while(*p && *p!='\r'&&*p!='\n' && n+1<sizeof(name)){
   if(quoted?*p=='"':isspace((unsigned char)*p))break;
   if((unsigned char)*p<32 || *p==':'){fclose(f);return fail(error,cap,"CUE tem caminho de trilha invalido");}
   name[n++]=*p=='\\'?'/':*p;
   p++;
  }
  name[n]=0;
  if(!n || (quoted&&*p!='"') || name[0]=='/' || ++count>64){
   fclose(f);return fail(error,cap,"Arquivo CUE invalido ou trilhas em excesso");
  }
  char full[1024];
  if(dirlen+n+1>sizeof(full)){
   fclose(f);return fail(error,cap,"Caminho de trilha CUE muito longo");
  }
  if(dirlen)memcpy(full,path,dirlen);
  memcpy(full+dirlen,name,n+1);
  FILE *track=fopen(full,"rb");
  if(!track){
   if(error&&cap)snprintf(error,cap,"CUE referencia trilha ausente: %.85s",name);
   fclose(f);return 0;
  }
  fclose(track);
 }
 int bad=ferror(f);fclose(f);
 if(bad)return fail(error,cap,"Falha de leitura do CUE");
 if(!count)return fail(error,cap,"CUE nao contem instrucoes FILE");
 return 1;
}
