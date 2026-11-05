/* Wii frontend launch argument builder — host-testable, GPL-3.0-or-later.
 * This code builds the very same NUL-separated argv block sent to __argv.
 * It does NOT claim a third-party binary consumes the arguments correctly. */
#include "launch_args.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static int fail(char *error,size_t cap,const char *reason){
 if(error && cap)snprintf(error,cap,"%s",reason);
 return 0;
}
static int append(MaizoLaunchArgs *out,const char *arg){
 if(!arg)return 0;
 size_t n=strlen(arg)+1;
 if(n>sizeof(out->data)-out->length)return 0;
 memcpy(out->data+out->length,arg,n);
 out->length+=n;
 out->argc++;
 return 1;
}
/* Accept only absolute FAT paths, never traversal or command-line controls. */
static int storage_path(const char *p){
 if(!p)return 0;
 size_t prefix=!strncmp(p,"usb:/",5)?5:(!strncmp(p,"sd:/",4)?4:0);
 if(!prefix || !p[prefix])return 0;
 const char *part=p+prefix;
 for(const char *c=part;;c++){
  if(*c=='\\' || (*c && (unsigned char)*c<32))return 0;
  if(*c=='/' || !*c){
   size_t n=(size_t)(c-part);
   if(n==0 || (n==1 && part[0]=='.') ||
      (n==2 && part[0]=='.' && part[1]=='.'))return 0;
   if(!*c)break;
   part=c+1;
  }
 }
 return 1;
}
static int id6(const char *id){
 if(!id || strlen(id)!=6)return 0;
 for(int i=0;i<6;i++)if(!isalnum((unsigned char)id[i]))return 0;
 return 1;
}
static int split_path(const char*path,char*dir,size_t cap,const char**base){
 const char*slash=strrchr(path,'/');
 if(!slash || !slash[1])return 0;
 size_t n=(size_t)(slash-path)+1;
 if(n>=cap)return 0;
 memcpy(dir,path,n);dir[n]=0;
 *base=slash+1;
 return 1;
}
static int ext_is(const char *path,const char *ext){
 size_t a=strlen(path),b=strlen(ext);
 if(a<b)return 0;
 for(size_t i=0;i<b;i++)if(tolower((unsigned char)path[a-b+i])!=tolower((unsigned char)ext[i]))return 0;
 return 1;
}
int maizo_build_args(const MaizoLaunchSpec *s,MaizoLaunchArgs *out,char*error,size_t cap){
 if(!s||!out)return fail(error,cap,"Argumentos nulos");
 memset(out,0,sizeof(*out));
 if(s->system<0 || s->system>=MAIZO_ARG_SYSTEM_COUNT)
  return fail(error,cap,"Sistema invalido");
 if(!s->executable || !storage_path(s->executable) || !*s->executable)
  return fail(error,cap,"Executavel invalido");
 if(!storage_path(s->path) || !*s->path)
  return fail(error,cap,"Caminho do jogo invalido");
 if(!append(out,s->executable))return fail(error,cap,"Comando excede limite");
 char option[64];
 if(s->system==MAIZO_ARG_WII){
  /* Never mistake an ID-less Wii entry for a successful direct autoboot. */
  if(!id6(s->id))return fail(error,cap,"ID6 ausente: autoboot Wii indisponivel");
  if(s->wii_usb_port>1 || !(s->wii_ios==58 || (s->wii_ios>=200 && s->wii_ios<=255)))
   return fail(error,cap,"IOS ou porta USB invalidos");
  if(!append(out,s->id))return fail(error,cap,"ID6 excede limite");
  snprintf(option,sizeof(option),"-bootios=%u",s->wii_ios);
  if(!append(out,option))return fail(error,cap,"IOS excede limite");
  snprintf(option,sizeof(option),"-usbport=%u",s->wii_usb_port);
  if(!append(out,option))return fail(error,cap,"Porta excede limite");
 }else if(s->system==MAIZO_ARG_PS1){
  if(ext_is(s->path,".chd") || ext_is(s->path,".m3u"))
   return fail(error,cap,"WiiSXRX original nao suporta CHD/M3U");
  if(s->ps1_interpreter>1 || s->ps1_frameskip>1 || s->ps1_audio>1)
   return fail(error,cap,"Opcoes PS1 invalidas");
  char dir[512];const char*base=NULL;
  if(!split_path(s->path,dir,sizeof(dir),&base))return fail(error,cap,"Caminho PS1 invalido");
  if(!append(out,dir)||!append(out,base))return fail(error,cap,"Caminho PS1 excede limite");
  snprintf(option,sizeof(option),"Core=%u",s->ps1_interpreter);
  if(!append(out,option))return fail(error,cap,"Core excede limite");
  snprintf(option,sizeof(option),"Audio=%u",s->ps1_audio);
  if(!append(out,option))return fail(error,cap,"Audio excede limite");
  snprintf(option,sizeof(option),"SkipFrames=%u",s->ps1_frameskip);
  if(!append(out,option))return fail(error,cap,"Frameskip excede limite");
 }else if(s->system>=MAIZO_ARG_SNES && s->system<=MAIZO_ARG_GB){
  char dir[512];const char*base=NULL;
  if(!split_path(s->path,dir,sizeof(dir),&base))return fail(error,cap,"Caminho ROM invalido");
  if(!append(out,dir)||!append(out,base))return fail(error,cap,"ROM excede limite");
 }else if(s->system==MAIZO_ARG_MEDIA){
  /* WiiMC-SSLC upstream treats argv[1] unsafely as printf format string. */
  if(strchr(s->path,'%'))return fail(error,cap,"Nome com % inseguro no WiiMC original");
  char media[514];int written=-1;
  if(!strncmp(s->path,"usb:/",5))written=snprintf(media,sizeof(media),"usb1:/%s",s->path+5);
  else if(!strncmp(s->path,"sd:/",4))written=snprintf(media,sizeof(media),"sd1:/%s",s->path+4);
  if(written<0 || (size_t)written>=sizeof(media))return fail(error,cap,"Caminho de midia excede limite");
  if(!append(out,media))return fail(error,cap,"Midia excede limite");
 }
 return 1;
}
