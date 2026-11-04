#include "app.h"
#include <gccore.h>
#include <fat.h>
#include <stdio.h>
#include <string.h>
static Library library_state;
static Config config;
static State state;
static int total(void){return library_total(&library_state,state.system,state.search,state.favorites);}
static void clamp_selection(void){
 int count=total();if(state.selected>=count)state.selected=count?count-1:0;
 if(state.selected<0)state.selected=0;
 if(state.scroll>state.selected)state.scroll=state.selected;
 if(state.selected>=state.scroll+8)state.scroll=state.selected-7;
}
static void change_system(int diff){state.system=(System)((state.system+SYSTEM_COUNT+diff)%SYSTEM_COUNT);
 state.selected=0;state.scroll=0;state.favorites=0;state.search[0]=0;
 if(state.page==PAGE_DETAIL)state.page=PAGE_LIBRARY;}
static void option_change(int dir){
 unsigned*dst=NULL;unsigned limit=2;
 switch(state.settings_row){
 case 0:dst=&config.theme;limit=3;break;case 1:dst=&config.layout;break;
 case 2:dst=&config.gc_memcard;break;case 3:dst=&config.gc_wide;break;
 case 4:dst=&config.gc_cheats;break;case 5:dst=&config.gc_skip_ipl;break;
 case 6:dst=&config.gc_video;limit=4;break;
 case 7:dst=&config.ps1_interpreter;break;
 case 8:dst=&config.ps1_frameskip;break;
 case 9:dst=&config.ps1_audio;break;
 case 10:dst=&config.media_volume;limit=101;break;
 default:state.page=PAGE_HOME;config_save(&config);return;
 }
 if(dst){if(state.settings_row==10){*dst=dir<0?(*dst>=5?*dst-5:0):(*dst<=95?*dst+5:100);}
 else *dst=(*dst+limit+(dir<0?-1:1))%limit;config_save(&config);}
}
static void handle(const Input*i){
 if(state.page==PAGE_INFO){if(i->back||i->accept)state.page=PAGE_LIBRARY;return;}
 if(i->tab_prev)change_system(-1);
 if(i->tab_next)change_system(1);
 if(i->settings && state.page!=PAGE_SETTINGS && state.page!=PAGE_SEARCH){state.page=PAGE_SETTINGS;return;}
 if(i->search && state.page==PAGE_LIBRARY){state.page=PAGE_SEARCH;state.search_index=1;return;}
 if(i->refresh && state.page==PAGE_LIBRARY){library_begin(&library_state);state.selected=state.scroll=0;return;}
 if(state.page==PAGE_HOME){
  if(i->left||i->up)change_system(-1);if(i->right||i->down)change_system(1);
  if(i->accept){state.page=PAGE_LIBRARY;state.selected=state.scroll=0;}
  return;
 }
 if(state.page==PAGE_SETTINGS){
  if(i->up&&state.settings_row>0)state.settings_row--;
  if(i->down&&state.settings_row<11)state.settings_row++;
  if(i->left)option_change(-1);
  if(i->right||i->accept)option_change(1);
  if(i->back){config_save(&config);state.page=PAGE_HOME;}
  return;
 }
 if(state.page==PAGE_SEARCH){
  static const char*alphabet=" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";
  const int n=(int)strlen(alphabet);
  if(i->left)state.search_index=(state.search_index+n-1)%n;
  if(i->right)state.search_index=(state.search_index+1)%n;
  if(i->accept){size_t len=strlen(state.search);if(len<sizeof(state.search)-2){state.search[len]=alphabet[state.search_index];state.search[len+1]=0;}}
  if(i->back){size_t len=strlen(state.search);if(len)state.search[len-1]=0;else state.page=PAGE_LIBRARY;}
  if(i->settings){state.page=PAGE_LIBRARY;state.scroll=state.selected=0;}
  return;
 }
 if(state.page==PAGE_LIBRARY){
  int n=total();if(i->up&&n>0)state.selected=(state.selected+n-1)%n;
  if(i->down&&n>0)state.selected=(state.selected+1)%n;
  clamp_selection();
  if(i->favorite){int idx=library_visible(&library_state,state.system,state.search,state.favorites,state.selected);
   if(idx>=0){library_state.games[idx].favorite^=1;favorites_save(&library_state);clamp_selection();}}
  if(i->accept){int idx=library_visible(&library_state,state.system,state.search,state.favorites,state.selected);
   if(idx>=0){state.detail=idx;state.page=PAGE_DETAIL;}}
  if(i->back){state.page=PAGE_HOME;state.search[0]=0;}
  return;
 }
 if(state.page==PAGE_DETAIL){
  if(i->back){state.page=PAGE_LIBRARY;return;}
  if(i->favorite&&state.detail>=0&&state.detail<library_state.count){library_state.games[state.detail].favorite^=1;favorites_save(&library_state);}
  if(i->accept&&state.detail>=0&&state.detail<library_state.count){
   char error[160]="";
   module_launch(&library_state.games[state.detail],&config,error,sizeof(error));
   snprintf(state.info,sizeof(state.info),"%s",error);
   state.page=PAGE_INFO;
  }
 }
}
int main(int argc,char**argv){
 (void)argc;(void)argv;
 fatInitDefault();config_load(&config);
 memset(&state,0,sizeof(state));state.system=SYSTEM_GC;state.page=PAGE_HOME;
 library_begin(&library_state);input_init();
 if(!ui_init()){input_shutdown();library_end(&library_state);return 1;}
 while(1){
  Input in;input_poll(&in);
  handle(&in);
  library_step(&library_state,10);
  clamp_selection();
  ui_draw(&state,&library_state,&config,in.hid_present);
  VIDEO_WaitVSync();state.frame++;
 }
 ui_shutdown();input_shutdown();library_end(&library_state);return 0;
}
