#include "app.h"
#include <gccore.h>
#include <fat.h>
#include <ogc/disc_io.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
static Library library_state;
static Config config;
static State state;
/* Forwarder and HBC enter through different IOS/device states. Mount both
 * independently; fatInitDefault may report SD success while USB fails. */
static int storage_root(const char *path){
 struct stat st;return stat(path,&st)==0 && S_ISDIR(st.st_mode);
}
static int mount_storage(void){
 fatInitDefault();
 if(!storage_root("sd:/")){
  for(int n=0;n<2 && !storage_root("sd:/");n++){
   if(__io_wiisd.startup())fatMountSimple("sd",&__io_wiisd);
  }
 }
 if(!storage_root("usb:/")){
  for(int n=0;n<8 && !storage_root("usb:/");n++){
   if(__io_usbstorage.startup())fatMountSimple("usb",&__io_usbstorage);
   if(!storage_root("usb:/"))usleep(250000);
  }
 }
 return storage_root("sd:/") || storage_root("usb:/");
}
static int total(void){return library_total(&library_state,state.system,state.search,state.favorites);}
static void clamp_selection(void){
 int count=total();if(state.selected>=count)state.selected=count?count-1:0;
 if(state.selected<0)state.selected=0;
 if(!config.layout){
  if(state.selected<state.scroll)state.scroll=(state.selected/4)*4;
  if(state.selected>=state.scroll+8){
   int row=state.selected/4;
   state.scroll=(row>0?row-1:0)*4;
  }
 }else{
  if(state.scroll>state.selected)state.scroll=state.selected;
  if(state.selected>=state.scroll+8)state.scroll=state.selected-7;
 }
}
static void change_system(int diff){state.system=(System)((state.system+SYSTEM_COUNT+diff)%SYSTEM_COUNT);
 state.selected=0;state.scroll=0;state.favorites=0;state.search[0]=0;
 if(state.page==PAGE_DETAIL)state.page=PAGE_LIBRARY;
 state.settings_row=0;}
static void close_settings(void){
 if(state.settings_for_game){
  int idx=state.detail;
  int ok=idx>=0 && idx<library_state.count &&
         config_game_save(&state.game_config,&library_state.games[idx]);
  state.settings_for_game=0;
  if(ok)state.page=PAGE_DETAIL;
  else {
   snprintf(state.info,sizeof(state.info),
            "FALHA AO SALVAR OPCOES DESTE JOGO");
   state.page=PAGE_INFO;
  }
 }else{
  config_save(&config);state.page=PAGE_HOME;
 }
}
static void option_change(int dir){
 if(state.settings_row>=settings_count(state.system)-1){
  close_settings();return;
 }
 if(state.settings_for_game && state.settings_row<2)return;
 Config*active=state.settings_for_game?&state.game_config:&config;
 if(settings_change(active,state.system,state.settings_row,dir) &&
    !state.settings_for_game)config_save(&config);
}
static void adjust_calibration(int dir){
 unsigned *p=NULL,low=0,high=100,step=1;
 switch(state.calibration_row){
 case 0:p=&config.video_x;break;
 case 1:p=&config.video_y;break;
 case 2:p=&config.video_w;low=70;high=130;break;
 case 3:p=&config.video_h;low=70;high=130;break;
 case 4:config.video_x=config.video_y=50;config.video_w=config.video_h=100;break;
 default:return;
 }
 if(p){
  if(dir<0)*p=*p<=low+step?low:*p-step;
  else *p=*p>=high-step?high:*p+step;
 }
 maizo_video_set_calibration((int)config.video_x-50,(int)config.video_y-50,
                             config.video_w,config.video_h);
 config_save(&config);
}
static void handle(const Input*i){
 if(state.page==PAGE_CALIBRATION){
  if(i->up&&state.calibration_row>0)state.calibration_row--;
  if(i->down&&state.calibration_row<5)state.calibration_row++;
  if(i->left)adjust_calibration(-1);
  if(i->right||i->accept){
   if(state.calibration_row==5){config_save(&config);state.page=PAGE_SETTINGS;}
   else adjust_calibration(1);
  }
  if(i->back){config_save(&config);state.page=PAGE_SETTINGS;}
  return;
 }
 if(state.page==PAGE_HID){
  if(i->back||i->settings||i->accept)state.page=PAGE_SETTINGS;
  return;
 }
 if(state.page==PAGE_INFO){if(i->back||i->accept)state.page=PAGE_LIBRARY;return;}
 if(!state.settings_for_game &&
    (state.page==PAGE_HOME||state.page==PAGE_LIBRARY||state.page==PAGE_SEARCH)){
  if(i->tab_prev)change_system(-1);
  if(i->tab_next)change_system(1);
 }
 if(i->settings && state.page==PAGE_DETAIL){
  if(state.detail>=0&&state.detail<library_state.count){
   const Game*g=&library_state.games[state.detail];
   config_game_load(&state.game_config,&config,g);
   state.settings_for_game=1;state.settings_row=2;state.page=PAGE_SETTINGS;
  }
  return;
 }
 if(i->settings && state.page!=PAGE_SETTINGS && state.page!=PAGE_SEARCH){
  state.settings_for_game=0;state.settings_row=0;state.page=PAGE_SETTINGS;
  return;
 }
 if(i->search && (state.page==PAGE_LIBRARY||state.page==PAGE_HOME)){state.page=PAGE_SEARCH;state.search_index=1;return;}
 if(i->refresh && state.page==PAGE_HOME){
  module_refresh_available();
  library_begin(&library_state);
  state.selected=state.scroll=0;
  return;
 }
 if(i->refresh && state.page==PAGE_LIBRARY){state.favorites=!state.favorites;state.selected=state.scroll=0;return;}
 if(state.page==PAGE_HOME){
  if(i->left)change_system(-1);if(i->right)change_system(1);
  if(i->up)change_system(-4);if(i->down)change_system(4);
  if(i->accept||i->favorite){state.page=PAGE_LIBRARY;state.favorites=!!i->favorite;state.selected=state.scroll=0;}
  return;
 }
 if(state.page==PAGE_SETTINGS){
  if(state.settings_row==settings_count(state.system)-2 && (i->accept||i->right)){
   state.calibration_row=0;state.page=PAGE_CALIBRATION;return;
  }
  if(i->refresh){state.page=PAGE_HID;return;}
  if(i->up&&state.settings_row>0)state.settings_row--;
  if(i->down&&state.settings_row<settings_count(state.system)-1)state.settings_row++;
  if(i->left)option_change(-1);
  if(i->right||i->accept)option_change(1);
  if(i->back)close_settings();
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
   const Game*g=&library_state.games[state.detail];
   Config effective;config_game_load(&effective,&config,g);
   module_launch(g,&effective,error,sizeof(error));
   snprintf(state.info,sizeof(state.info),"%s",error);
   state.page=PAGE_INFO;
  }
 }
}
/* Start the software VI framebuffer first. FAT/USB HID may block:
 * a visible boot-stage label gives us a precise failure boundary. */
int main(int argc,char**argv){
 (void)argc;(void)argv;
 if(!ui_init())return 1;
 ui_boot_message("VIDEO VI OK");
 VIDEO_WaitVSync();
 ui_boot_message("MONTANDO SD E USB");
 int storage_ready=mount_storage();
 ui_boot_message(storage_ready?"CARREGANDO CONFIGURACOES":"SEM SD/USB - VERIFIQUE O DISPOSITIVO");
 config_load(&config);
 maizo_video_set_calibration((int)config.video_x-50,(int)config.video_y-50,config.video_w,config.video_h);
 memset(&state,0,sizeof(state));
 state.system=SYSTEM_GC;
 state.page=PAGE_HOME;
 ui_boot_message("VERIFICANDO MODULOS");
 module_refresh_available();
 ui_boot_message("INDEXANDO BIBLIOTECA");
 library_begin(&library_state);
 ui_boot_message("INICIALIZANDO CONTROLES");
 input_init();
 /* Show a complete home frame before the first USB HID probe. */
 ui_draw(&state,&library_state,&config,0);
 for(;;){
  Input in;
  input_poll(&in);
  handle(&in);
  library_step(&library_state,10);
  clamp_selection();
  ui_draw(&state,&library_state,&config,in.hid_present);
  state.frame++;
 }
}
