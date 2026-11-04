#include "app.h"
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
typedef struct { DIR *fd;char path[MAIZO_PATH]; } Dir;
typedef struct { Dir dirs[MAIZO_DEPTH]; } Work;
static const struct { System system;const char *path; } roots[]={
 {SYSTEM_WII,"usb:/wbfs"},{SYSTEM_WII,"sd:/wbfs"},{SYSTEM_WII,"usb:/roms/wii"},{SYSTEM_WII,"sd:/roms/wii"},
 {SYSTEM_GC,"usb:/games"},{SYSTEM_GC,"sd:/games"},{SYSTEM_GC,"usb:/roms/gamecube"},{SYSTEM_GC,"sd:/roms/gamecube"},
 {SYSTEM_SNES,"usb:/snes9xgx/roms"},{SYSTEM_SNES,"sd:/snes9xgx/roms"},{SYSTEM_SNES,"usb:/roms/snes"},{SYSTEM_SNES,"sd:/roms/snes"},
 {SYSTEM_NES,"usb:/fceugx/roms"},{SYSTEM_NES,"sd:/fceugx/roms"},{SYSTEM_NES,"usb:/roms/nes"},{SYSTEM_NES,"sd:/roms/nes"},
 {SYSTEM_GBA,"usb:/vbagx/roms"},{SYSTEM_GBA,"sd:/vbagx/roms"},{SYSTEM_GBA,"usb:/roms/gba"},{SYSTEM_GBA,"sd:/roms/gba"},
 {SYSTEM_GB,"usb:/vbagx/roms"},{SYSTEM_GB,"sd:/vbagx/roms"},{SYSTEM_GB,"usb:/roms/gb"},{SYSTEM_GB,"sd:/roms/gb"},
 {SYSTEM_PS1,"usb:/ps1"},{SYSTEM_PS1,"sd:/ps1"},{SYSTEM_PS1,"usb:/wiisxrx/isos"},{SYSTEM_PS1,"sd:/wiisxrx/isos"},
 {SYSTEM_MEDIA,"usb:/videos"},{SYSTEM_MEDIA,"sd:/videos"},{SYSTEM_MEDIA,"usb:/movies"},{SYSTEM_MEDIA,"sd:/movies"},{SYSTEM_MEDIA,"usb:/music"},{SYSTEM_MEDIA,"sd:/music"}
};
static int ext(const char*s,const char*e){const char*d=strrchr(s,'.');return d&&!strcasecmp(d,e);}
static int allowed(System s,const char*n){
 if(s==SYSTEM_WII)return ext(n,".wbfs")||ext(n,".iso");
 if(s==SYSTEM_GC)return ext(n,".iso")||ext(n,".gcm")||ext(n,".ciso");
 if(s==SYSTEM_SNES)return ext(n,".sfc")||ext(n,".smc")||ext(n,".fig")||ext(n,".zip");
 if(s==SYSTEM_NES)return ext(n,".nes")||ext(n,".fds")||ext(n,".unf")||ext(n,".zip");
 if(s==SYSTEM_GBA)return ext(n,".gba");
 if(s==SYSTEM_GB)return ext(n,".gb")||ext(n,".gbc")||ext(n,".sgb");
 if(s==SYSTEM_PS1)return ext(n,".cue")||ext(n,".chd")||ext(n,".pbp")||ext(n,".iso")||ext(n,".img")||ext(n,".bin")||ext(n,".m3u");
 const char*e[]={".avi",".mp4",".mkv",".m4v",".mov",".flv",".mpg",".mpeg",".vob",".wmv",".mp3",".ogg",".flac",".wav",".aac",".m4a"};
 for(unsigned i=0;i<sizeof(e)/sizeof(e[0]);i++)if(ext(n,e[i]))return 1;return 0;
}
static void insert(Library*l,System sys,const char*path,const char*filename,const char*parent) {
 if(l->count>=MAIZO_MAX_GAMES)return;
 if(sys==SYSTEM_PS1&&ext(filename,".bin")){char p[MAIZO_PATH];snprintf(p,sizeof(p),"%s",path);char*d=strrchr(p,'.');if(d){strcpy(d,".cue");FILE*f=fopen(p,"rb");if(f){fclose(f);return;}}}
 for(int i=0;i<l->count;i++)if(!strcasecmp(path,l->games[i].path))return;
 Game*g=&l->games[l->count++];memset(g,0,sizeof(*g));g->system=sys;
 snprintf(g->path,sizeof(g->path),"%s",path);
 const char*n=(!strcasecmp(filename,"game.iso")||!strcasecmp(filename,"disc2.iso"))?parent:filename;
 snprintf(g->name,sizeof(g->name),"%s",n);
 char*d=strrchr(g->name,'.');if(d)*d=0;
 for(d=g->name;*d;d++)if(*d=='_')*d=' ';
 if(sys==SYSTEM_WII){
  const char*b=strrchr(parent,'[');const char*end=b?strchr(b,']'):NULL;
  if(b&&end&&end-b==7){int valid=1;for(int j=1;j<=6;j++)if(!isalnum((unsigned char)b[j]))valid=0;
   if(valid){memcpy(g->id,b+1,6);g->id[6]=0;}}
  if(!g->id[0]&&strlen(filename)>=10&&filename[6]=='.'){int valid=1;for(int j=0;j<6;j++)if(!isalnum((unsigned char)filename[j]))valid=0;
   if(valid){memcpy(g->id,filename,6);g->id[6]=0;}}
  if(!g->id[0]&&ext(filename,".iso")){FILE*f=fopen(path,"rb");if(f){char id[6];if(fread(id,1,6,f)==6){int valid=1;for(int j=0;j<6;j++)if(!isalnum((unsigned char)id[j]))valid=0;
   if(valid){memcpy(g->id,id,6);g->id[6]=0;}}fclose(f);}}
 }
 if(sys==SYSTEM_GC){FILE*f=fopen(path,"rb");if(f){char id[6];if(fread(id,1,6,f)==6){int ok=1;for(int j=0;j<6;j++)if(!isalnum((unsigned char)id[j]))ok=0;if(ok){memcpy(g->id,id,6);g->id[6]=0;}}fclose(f);}}
}
static int sort_games(const void*a,const void*b){const Game*x=a,*y=b;return x->system!=y->system?(int)x->system-(int)y->system:strcasecmp(x->name,y->name);}
void library_end(Library*l){Work*w=(Work*)l->work;if(w){for(int i=0;i<l->depth;i++)if(w->dirs[i].fd)closedir(w->dirs[i].fd);free(w);}l->work=NULL;l->depth=0;l->scanning=0;}
void library_begin(Library*l){library_end(l);memset(l,0,sizeof(*l));l->work=calloc(1,sizeof(Work));l->scanning=l->work!=NULL;snprintf(l->status,sizeof(l->status),"Procurando arquivos...");}
static int open_root(Library*l){Work*w=(Work*)l->work;
 while(l->root<(int)(sizeof(roots)/sizeof(roots[0]))){int n=l->root++;DIR*d=opendir(roots[n].path);if(!d)continue;
 l->depth=1;w->dirs[0].fd=d;snprintf(w->dirs[0].path,sizeof(w->dirs[0].path),"%s",roots[n].path);
 snprintf(l->status,sizeof(l->status),"Lendo %s",roots[n].path);return 1;}return 0;}
void library_step(Library*l,int budget){if(!l->scanning||!l->work)return;Work*w=(Work*)l->work;
 while(budget-->0){
  if(!l->depth){if(!open_root(l)){l->scanning=0;qsort(l->games,l->count,sizeof(Game),sort_games);favorites_load(l);snprintf(l->status,sizeof(l->status),"%d arquivos encontrados",l->count);free(l->work);l->work=NULL;return;}continue;}
  Dir*top=&w->dirs[l->depth-1];struct dirent*d=readdir(top->fd);
  if(!d){closedir(top->fd);top->fd=NULL;l->depth--;continue;}
  if(d->d_name[0]=='.')continue;l->entries++;
  char full[MAIZO_PATH];if(snprintf(full,sizeof(full),"%s/%s",top->path,d->d_name)>=(int)sizeof(full))continue;
  struct stat st;if(stat(full,&st))continue;
  if(S_ISDIR(st.st_mode)){if(l->depth==MAIZO_DEPTH)continue;DIR*sub=opendir(full);if(!sub)continue;Dir*child=&w->dirs[l->depth++];child->fd=sub;snprintf(child->path,sizeof(child->path),"%s",full);continue;}
  System sys=roots[l->root-1].system;
  if(S_ISREG(st.st_mode)&&allowed(sys,d->d_name)){const char*p=strrchr(top->path,'/');insert(l,sys,full,d->d_name,p?p+1:top->path);}
 }}
static int match(const char*s,const char*q){if(!q||!*q)return 1;
 for(;*s;s++){const char*a=s,*b=q;while(*a&&*b&&tolower((unsigned char)*a)==tolower((unsigned char)*b)){a++;b++;}if(!*b)return 1;}return 0;}
int library_visible(const Library*l,System sys,const char*q,int fav,int idx){if(idx<0)return -1;
 for(int i=0;i<l->count;i++){const Game*g=&l->games[i];if(g->system==sys&&(!fav||g->favorite)&&match(g->name,q)&&idx--==0)return i;}return -1;}
int library_total(const Library*l,System sys,const char*q,int fav){int n=0;for(int i=0;i<l->count;i++){const Game*g=&l->games[i];if(g->system==sys&&(!fav||g->favorite)&&match(g->name,q))n++;}return n;}
