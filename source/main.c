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
 if(state.page==PAGE_DETAIL)state.page=PAGE_LIBRARY;
 state.settings_row=0;}
static void option_change(int dir){
 if(state.settings_row>=settings_count(state.system)-1){
  config_save(&config);state.page=PAGE_HOME;return;
 }
 if(settings_change(&config,state.system,state.settings_row,dir))config_save(&config);
}
static void handle(const Input*i){
 if(state.page==PAGE_INFO){if(i->back||i->accept)state.page=PAGE_LIBRARY;return;}
 if(i->tab_prev)change_system(-1);
 if(i->tab_next)change_system(1);
 if(i->settings && state.page!=PAGE_SETTINGS && state.page!=PAGE_SEARCH){state.page=PAGE_SETTINGS;return;}
 if(i->search && state.page==PAGE_LIBRARY){state.page=PAGE_SEARCH;state.search_index=1;return;}
 if(i->refresh && state.page==PAGE_HOME){library_begin(&library_state);return;}
 if(i->refresh && state.page==PAGE_LIBRARY){state.favorites=!state.favorites;state.selected=state.scroll=0;return;}
 if(state.page==PAGE_HOME){
  if(i->left)change_system(-1);if(i->right)change_system(1);
  if(i->up)change_system(-4);if(i->down)change_system(4);
  if(i->accept||i->favorite){state.page=PAGE_LIBRARY;state.favorites=!!i->favorite;state.selected=state.scroll=0;}
  return;
 }
 if(state.page==PAGE_SETTINGS){
  if(i->up&&state.settings_row>0)state.settings_row--;
  if(i->down&&state.settings_row<settings_count(state.system)-1)state.settings_row++;
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
  if(i->up)state.search_index=(state.search_index+n-8)%n;
  if(i->down)state.search_index=(state.search_index+8)%n;
  if(i->accept){size_t len=strlen(state.search);if(len<sizeof(state.search)-2){state.search[len]=alphabet[state.search_index];state.search[len+1]=0;}}
  if(i->back){size_t len=strlen(state.search);if(len)state.search[len-1]=0;else state.page=PAGE_LIBRARY;}
  if(i->settings){state.page=PAGE_LIBRARY;state.scroll=state.selected=0;}
  return;
 }
 if(state.page==PAGE_LIBRARY){
  int n=total();if(n>0){
   int stride=config.layout?1:4;
   if(i->up)state.selected=(state.selected+n-(stride%n))%n;
   if(i->down)state.selected=(state.selected+stride)%n;
   if(!config.layout&&i->left)state.selected=(state.selected+n-1)%n;
   if(!config.layout&&i->right)state.selected=(state.selected+1)%n;
  }
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
