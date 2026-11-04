#include "app.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
static const char*where(void){struct stat s;if(!stat("sd:/",&s))return "sd:/maizo";return "usb:/maizo";}
void config_defaults(Config*c){memset(c,0,sizeof(*c));c->gc_memcard=1;c->gc_skip_ipl=1;c->ps1_audio=1;c->media_volume=75;}
void config_load(Config*c){config_defaults(c);char p[80];snprintf(p,sizeof(p),"%s/settings.ini",where());FILE*f=fopen(p,"rb");if(!f)return;
 char key[64];unsigned val;
 while(fscanf(f," %63[^=]=%u",key,&val)==2) {
  if(!strcmp(key,"theme"))c->theme=val%3;
  else if(!strcmp(key,"layout"))c->layout=val%2;
  else if(!strcmp(key,"gc_memcard"))c->gc_memcard=!!val;
  else if(!strcmp(key,"gc_wide"))c->gc_wide=!!val;
  else if(!strcmp(key,"gc_cheats"))c->gc_cheats=!!val;
  else if(!strcmp(key,"gc_skip_ipl"))c->gc_skip_ipl=!!val;
  else if(!strcmp(key,"gc_video"))c->gc_video=val%4;
  else if(!strcmp(key,"ps1_interpreter"))c->ps1_interpreter=!!val;
  else if(!strcmp(key,"ps1_frameskip"))c->ps1_frameskip=!!val;
  else if(!strcmp(key,"ps1_audio"))c->ps1_audio=!!val;
  else if(!strcmp(key,"media_volume"))c->media_volume=val>100?100:val;
 }fclose(f);}
int config_save(const Config*c){const char*root=where();mkdir(root,0777);
 char p[80],tmp[80];snprintf(p,sizeof(p),"%s/settings.ini",root);snprintf(tmp,sizeof(tmp),"%s/settings.tmp",root);FILE*f=fopen(tmp,"wb");if(!f)return 0;
 fprintf(f,"theme=%u\nlayout=%u\ngc_memcard=%u\ngc_wide=%u\ngc_cheats=%u\ngc_skip_ipl=%u\ngc_video=%u\nps1_interpreter=%u\nps1_frameskip=%u\nps1_audio=%u\nmedia_volume=%u\n",
 c->theme,c->layout,c->gc_memcard,c->gc_wide,c->gc_cheats,c->gc_skip_ipl,c->gc_video,c->ps1_interpreter,c->ps1_frameskip,c->ps1_audio,c->media_volume);
 if(fclose(f))return 0;remove(p);return rename(tmp,p)==0;}
void favorites_load(Library*l){char p[80];snprintf(p,sizeof(p),"%s/favorites.txt",where());FILE*f=fopen(p,"rb");if(!f)return;char line[MAIZO_PATH+4];while(fgets(line,sizeof(line),f)){line[strcspn(line,"\r\n")]=0;for(int i=0;i<l->count;i++)if(!strcmp(line,l->games[i].path)){l->games[i].favorite=1;break;}}fclose(f);}
int favorites_save(const Library*l){const char*r=where();mkdir(r,0777);char p[80];snprintf(p,sizeof(p),"%s/favorites.txt",r);FILE*f=fopen(p,"wb");if(!f)return 0;for(int i=0;i<l->count;i++)if(l->games[i].favorite)fprintf(f,"%s\n",l->games[i].path);return fclose(f)==0;}
