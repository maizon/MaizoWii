#include "app.h"
#include "game_profile.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
static const char*where(void){struct stat s;if(!stat("sd:/",&s))return "sd:/maizo";return "usb:/maizo";}
void config_defaults(Config*c){memset(c,0,sizeof(*c));c->wii_ios=249;c->wii_usb_port=0;c->gc_memcard=1;c->gc_skip_ipl=1;c->ps1_audio=1;c->media_volume=75;}
void config_load(Config*c){config_defaults(c);char p[80];snprintf(p,sizeof(p),"%s/settings.ini",where());FILE*f=fopen(p,"rb");if(!f)return;
 char key[64];unsigned val;
 while(fscanf(f," %63[^=]=%u",key,&val)==2) {
  if(!strcmp(key,"theme"))c->theme=val%4;
  else if(!strcmp(key,"wii_ios"))c->wii_ios=(val==58|| (val>=200&&val<=255))?val:249;
  else if(!strcmp(key,"wii_usb_port"))c->wii_usb_port=val>1?0:val;
  else if(!strcmp(key,"gc_mc_multi"))c->gc_mc_multi=!!val;
  else if(!strcmp(key,"gc_native_si"))c->gc_native_si=!!val;
  else if(!strcmp(key,"gc_rem_limit"))c->gc_rem_limit=!!val;
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
 fprintf(f,"theme=%u\nlayout=%u\nwii_ios=%u\nwii_usb_port=%u\ngc_mc_multi=%u\ngc_native_si=%u\ngc_rem_limit=%u\ngc_memcard=%u\ngc_wide=%u\ngc_cheats=%u\ngc_skip_ipl=%u\ngc_video=%u\nps1_interpreter=%u\nps1_frameskip=%u\nps1_audio=%u\nmedia_volume=%u\n",
 c->theme,c->layout,c->wii_ios,c->wii_usb_port,c->gc_mc_multi,c->gc_native_si,c->gc_rem_limit,c->gc_memcard,c->gc_wide,c->gc_cheats,c->gc_skip_ipl,c->gc_video,c->ps1_interpreter,c->ps1_frameskip,c->ps1_audio,c->media_volume);
 if(fclose(f))return 0;remove(p);return rename(tmp,p)==0;}
void favorites_load(Library*l){char p[80];snprintf(p,sizeof(p),"%s/favorites.txt",where());FILE*f=fopen(p,"rb");if(!f)return;char line[MAIZO_PATH+4];while(fgets(line,sizeof(line),f)){line[strcspn(line,"\r\n")]=0;for(int i=0;i<l->count;i++)if(!strcmp(line,l->games[i].path)){l->games[i].favorite=1;break;}}fclose(f);}
int favorites_save(const Library*l){const char*r=where();mkdir(r,0777);char p[80];snprintf(p,sizeof(p),"%s/favorites.txt",r);FILE*f=fopen(p,"wb");if(!f)return 0;for(int i=0;i<l->count;i++)if(l->games[i].favorite)fprintf(f,"%s\n",l->games[i].path);return fclose(f)==0;}


/* Overlay only the settings the selected engine actually consumes.
 * Themes/layout and unrelated systems stay global. Missing profile is
 * an intentional inheritance, not a broken default. */
void config_game_load(Config*out,const Config*defaults,const Game*g){
 if(!out||!defaults)return;
 *out=*defaults;
 if(!g)return;
 char dest[112];
 if(!maizo_game_profile_path((unsigned)g->system,g->path,dest,sizeof(dest)))return;
 FILE*f=fopen(dest,"rb");
 if(!f){
  /* Recover the last known-good profile after interrupted FAT rename. */
  char backup[128];snprintf(backup,sizeof(backup),"%s.bak",dest);
  f=fopen(backup,"rb");
 }
 if(!f)return;
 char line[128],key[48];
 unsigned value;
 while(fgets(line,sizeof(line),f)){
  if(sscanf(line," %47[^=]=%u",key,&value)!=2)continue;
  if(g->system==SYSTEM_WII){
   if(!strcmp(key,"wii_ios")&&(value==58||(value>=200&&value<=255)))
    out->wii_ios=value;
   else if(!strcmp(key,"wii_usb_port")&&value<=1)
    out->wii_usb_port=value;
  } else if(g->system==SYSTEM_GC){
   if(!strcmp(key,"gc_memcard"))out->gc_memcard=!!value;
   else if(!strcmp(key,"gc_wide"))out->gc_wide=!!value;
   else if(!strcmp(key,"gc_cheats"))out->gc_cheats=!!value;
   else if(!strcmp(key,"gc_skip_ipl"))out->gc_skip_ipl=!!value;
   else if(!strcmp(key,"gc_video")&&value<4)out->gc_video=value;
   else if(!strcmp(key,"gc_mc_multi"))out->gc_mc_multi=!!value;
   else if(!strcmp(key,"gc_native_si"))out->gc_native_si=!!value;
   else if(!strcmp(key,"gc_rem_limit"))out->gc_rem_limit=!!value;
  } else if(g->system==SYSTEM_PS1){
   if(!strcmp(key,"ps1_interpreter"))out->ps1_interpreter=!!value;
   else if(!strcmp(key,"ps1_frameskip"))out->ps1_frameskip=!!value;
   else if(!strcmp(key,"ps1_audio"))out->ps1_audio=!!value;
  } else if(g->system==SYSTEM_MEDIA){
   if(!strcmp(key,"media_volume")&&value<=100)out->media_volume=value;
  }
 }
 fclose(f);
}
int config_game_save(const Config*c,const Game*g){
 if(!c||!g)return 0;
 if(g->system!=SYSTEM_WII&&g->system!=SYSTEM_GC&&
    g->system!=SYSTEM_PS1&&g->system!=SYSTEM_MEDIA)return 1;
 char dest[112],tmp[118];
 if(!maizo_game_profile_path((unsigned)g->system,g->path,dest,sizeof(dest)))return 0;
 const char*dev=!strncmp(g->path,"sd:/",4)?"sd:":"usb:";
 char root[40],folder[64];
 snprintf(root,sizeof(root),"%s/maizo",dev);
 snprintf(folder,sizeof(folder),"%s/maizo/game-profiles",dev);
 mkdir(root,0777);mkdir(folder,0777);
 snprintf(tmp,sizeof(tmp),"%s.tmp",dest);
 FILE*f=fopen(tmp,"wb");if(!f)return 0;
 int ok=fprintf(f,"format=1\n")>0;
 switch(g->system){
  case SYSTEM_WII:
   ok &= fprintf(f,"wii_ios=%u\nwii_usb_port=%u\n",c->wii_ios,c->wii_usb_port)>0;
   break;
  case SYSTEM_GC:
   ok &= fprintf(f,"gc_memcard=%u\ngc_wide=%u\ngc_cheats=%u\n"
    "gc_skip_ipl=%u\ngc_video=%u\ngc_mc_multi=%u\n"
    "gc_native_si=%u\ngc_rem_limit=%u\n",
    c->gc_memcard,c->gc_wide,c->gc_cheats,c->gc_skip_ipl,
    c->gc_video,c->gc_mc_multi,c->gc_native_si,c->gc_rem_limit)>0;
   break;
  case SYSTEM_PS1:
   ok &= fprintf(f,"ps1_interpreter=%u\nps1_frameskip=%u\nps1_audio=%u\n",
    c->ps1_interpreter,c->ps1_frameskip,c->ps1_audio)>0;
   break;
  case SYSTEM_MEDIA:
   ok &= fprintf(f,"media_volume=%u\n",c->media_volume)>0;
   break;
  default:break;
 }
 if(fclose(f)!=0)ok=0;
 if(!ok){remove(tmp);return 0;}
 /* Same-volume rename, never overwrite or create files outside /maizo. */
 if(rename(tmp,dest)==0)return 1;
 /* FAT renames may refuse to overwrite. Keep a rollback copy rather
  * than deleting the previous working profile before committing. */
 char backup[128];snprintf(backup,sizeof(backup),"%s.bak",dest);
 if(rename(dest,backup)!=0){remove(tmp);return 0;}
 if(rename(tmp,dest)==0){remove(backup);return 1;}
 rename(backup,dest);remove(tmp);return 0;
}
