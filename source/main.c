#include "app.h"
#include "nav.h"
#include "cover_download.h"
#include "video_sw.h"
#include <gccore.h>
#include <fat.h>
#include <ogc/disc_io.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
static Library library_state;
static Config config;
static State state;
/* Mount both devices like a standalone homebrew, not by relying on the
 * previous channel's libfat state. In particular, fatInitDefault() may report
 * overall success after SD mount while USB is absent. The previously used
 * stat("usb:/") test can also reject a valid FAT root on some devoptabs. */
static int media_ready=0,storage_usb=0,storage_sd=0;
static int storage_root(const char *path){
 DIR *d=opendir(path);
 if(!d)return 0;
 closedir(d);
 return 1;
}
/* Priiloader autoboot reaches the DOL before Priiloader's interactive
 * InitMounts(). Do not depend on the menu having warmed up USB, and do not
 * make fatInitDefault() race the explicit mass-storage interface.
 * USB and SD are independent: a working SD must never conceal failed USB.
 */
static int mount_storage(int initial){
 int usb=storage_root("usb:/"),sd=storage_root("sd:/");
 if(!sd){
  if(initial)ui_boot_message("SD: INICIALIZANDO...");
  for(int n=0;n<(initial?3:1)&&!sd;n++){
   int started=__io_wiisd.startup();
   int present=started&&__io_wiisd.isInserted();
   if(present&&fatMountSimple("sd",&__io_wiisd)){
    sd=storage_root("sd:/");
    if(!sd)fatUnmount("sd");
   }
   if(!sd&&initial)usleep(120000);
  }
 }
 if(!usb){
  const int tries=initial?12:2;
  for(int n=0;n<tries&&!usb;n++){
   /* A visible stage identifies whether IOS USB enumeration itself hangs.
    * This does not pretend that a synchronous libogc call is cancellable. */
   if(initial)ui_boot_message(n==0?"USB: INICIALIZANDO...":"USB: NOVA TENTATIVA...");
   const int started=__io_usbstorage.startup();
   const int present=started&&__io_usbstorage.isInserted();
   if(present&&fatMountSimple("usb",&__io_usbstorage)){
    usb=storage_root("usb:/");
    if(!usb)fatUnmount("usb");
   }
   if(!usb){
    if(started)__io_usbstorage.shutdown();
    if(initial)usleep(350000);
   }
  }
 }
 storage_usb=usb;storage_sd=sd;media_ready=usb||sd;
 return media_ready;
}
/* Forwarder -> DOL handoff may reopen FAT after the initial mount window.
 * Retry USB without requiring a Wiimote, a press of Z, or an SD card. */
static void retry_missing_storage(void){
 if((storage_usb&&storage_sd)||state.frame==0||(state.frame%300)!=0)return;
 int old_usb=storage_usb,old_sd=storage_sd;
 mount_storage(0);
 state.storage_usb=storage_usb;state.storage_sd=storage_sd;
 if(old_usb!=storage_usb||old_sd!=storage_sd){
  input_storage_ready(media_ready);
  module_refresh_available();
  cover_reset();
  library_begin(&library_state);
  state.selected=state.scroll=0;
 }
}
static int total(void){return library_total(&library_state,state.system,state.search,state.favorites);}
static void inspect_cover_library(void){
 memset(state.covers_total,0,sizeof(state.covers_total));
 memset(state.covers_ready,0,sizeof(state.covers_ready));
 for(int n=0;n<library_state.count;n++){
  const Game *g=&library_state.games[n];
  if(g->system>=SYSTEM_WII&&g->system<=SYSTEM_GB){
   int sys=(int)g->system;state.covers_total[sys]++;
   if(cover_on_disk(g))state.covers_ready[sys]++;
  }
 }
}
/* One cover file per cycle, never from ui_draw(). This is deliberate:
 * synchronous FAT access in the raster hot path caused frame stalls when
 * navigating between covers on real storage. */
static unsigned cover_cycle=0;
static const char *icon_codes[SYSTEM_COUNT]={
 "IWII","IGC","IPS1","ISNES","INES","IGBA","IGB","IMEDIA","IHB"
};
static void prefetch_covers(void){
 if(!media_ready)return;
 unsigned round=cover_cycle++;
 if(state.page==PAGE_HOME){
  unsigned system=round%SYSTEM_COUNT;
  Game icon={0};icon.system=(System)system;
  snprintf(icon.id,sizeof(icon.id),"%s",icon_codes[system]);
  cover_prefetch(&icon);
 }else if(state.page==PAGE_LIBRARY){
  /* After reindex/hotplug a cover_reset clears icons too. Refill pinned
   * console artwork in small slices without blocking frame rendering. */
  if(round%9==0){
   /* Both home portraits and transparent topbar sprites survive hotplug. */
   unsigned icon_round=round/9;
   unsigned system=icon_round%SYSTEM_COUNT;
   Game icon={0};icon.system=(System)system;
   snprintf(icon.id,sizeof(icon.id),"%s",icon_codes[system]);
   if((icon_round/SYSTEM_COUNT)&1)icon.id[0]='T';
   cover_prefetch(&icon);
   return;
  }
  int n=total();if(n<=0)return;
  static const int offsets[]={0,-1,1,-2,2,-3,3};
  int pos=state.selected;
  if(config.layout==0)pos=state.scroll+(int)(round%8);
  else if(config.layout==2)pos+=offsets[round%5];
  else pos+=offsets[round%7];
  if(pos<0||pos>=n)return;
  int idx=library_visible(&library_state,state.system,state.search,0,pos);
  if(idx>=0)cover_prefetch(&library_state.games[idx]);
 }else if(state.page==PAGE_DETAIL &&
          state.detail>=0&&state.detail<library_state.count){
  cover_prefetch(&library_state.games[state.detail]);
 }
}
static void clamp_selection(void){
 int count=total();if(state.selected>=count)state.selected=count?count-1:0;
 if(state.selected<0)state.selected=0;
 state.scroll=maizo_nav_scroll(state.selected,state.scroll,count,
                                 config.layout==0?4:1,8);
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
  if(ok)state.page=state.settings_return;
  else {
   snprintf(state.info,sizeof(state.info),
            "FALHA AO SALVAR OPCOES DESTE JOGO");
   state.info_return=state.settings_return==PAGE_DETAIL?PAGE_DETAIL:PAGE_LIBRARY;state.page=PAGE_INFO;
  }
 }else{
  if(config_save(&config))state.page=state.settings_return;
  else{
   snprintf(state.info,sizeof(state.info),"NAO FOI POSSIVEL GRAVAR AJUSTES NO USB/SD");
   state.info_return=PAGE_SETTINGS;state.page=PAGE_INFO;
  }
 }
}
static void option_change(int dir){
 System scope=state.settings_global?SYSTEM_COUNT:state.system;
 if(state.settings_row>=settings_count(scope)-1){
  close_settings();return;
 }
 if(state.settings_for_game && state.settings_row<2)return;
 Config*active=state.settings_for_game?&state.game_config:&config;
 if(settings_change(active,scope,state.settings_row,dir) &&
    !state.settings_for_game){
   if(scope==SYSTEM_COUNT && state.settings_row==2)
    maizo_video_force_vi704(config.force_vi704);
   config_save(&config);
  }
}
static void adjust_calibration(int dir){
 unsigned *p=NULL,low=0,high=100,step=1;
 switch(state.calibration_row){
 case 0:p=&config.video_x;break;
 case 1:p=&config.video_y;break;
 case 2:p=&config.video_w;low=70;high=130;break;
 case 3:p=&config.video_h;low=70;high=130;break;
 case 4:{
  unsigned next=(config.frontend_video_mode+3+(dir<0?-1:1))%3;
  if(!state.video_preview_frames)state.video_previous_mode=config.frontend_video_mode;
  if(maizo_video_frontend_select(next)){
   config.frontend_video_mode=next;state.video_preview_frames=600;
  }
  return;
 }
 case 5:config.video_x=config.video_y=50;config.video_w=config.video_h=100;break;
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
/* Direct play from the library and details use the exact same safe path. */
static void play_game(int index,Page back){
 if(index<0||index>=library_state.count)return;
 const Game*g=&library_state.games[index];
 state.detail=index;
 struct stat st;
 if(stat(g->path,&st)!=0 || !S_ISREG(st.st_mode)){
  snprintf(state.info,sizeof(state.info),"JOGO INDISPONIVEL. CONFIRA USB/SD E REINDEXE");
  state.info_return=back;state.page=PAGE_INFO;return;
 }
 Config effective;config_game_load(&effective,&config,g);
 char error[160]="";
 if(!module_launch(g,&effective,error,sizeof(error))){
  snprintf(state.info,sizeof(state.info),"%s",error[0]?error:"FALHA NA INICIALIZACAO DO JOGO");
  state.info_return=back;state.page=PAGE_INFO;
 }
}
static void handle(const Input*i){
 if(state.page==PAGE_WELCOME){
  if(i->left||i->right)config.theme=(config.theme+6+(i->left?-1:1))%6;
  if(i->accept||i->back||i->settings){
   if(!config_save(&config)){
    snprintf(state.info,sizeof(state.info),"BEM-VINDO! AJUSTES NAO SALVOS NO DISPOSITIVO");
    state.info_return=PAGE_HOME;state.page=PAGE_INFO;
   }else state.page=PAGE_HOME;
  }
  return;
 }
 if(state.page==PAGE_CALIBRATION){
  if(i->up&&state.calibration_row>0)state.calibration_row--;
  if(i->down&&state.calibration_row<6)state.calibration_row++;
  if(i->left)adjust_calibration(-1);
  if(i->right||i->accept){
   if(state.calibration_row==4 && state.video_preview_frames>0){
    state.video_preview_frames=0;config_save(&config);
   }else if(state.calibration_row==6){
    state.video_preview_frames=0;config_save(&config);state.page=PAGE_SETTINGS;
   }else adjust_calibration(1);
  }
  if(i->back){
   if(state.video_preview_frames>0){
    maizo_video_frontend_select(state.video_previous_mode);
    config.frontend_video_mode=state.video_previous_mode;
    state.video_preview_frames=0;
   }
   config_save(&config);state.page=PAGE_SETTINGS;
  }
  return;
 }
 if(state.page==PAGE_HID){
  if(i->back||i->settings||i->accept)state.page=PAGE_SETTINGS;
  return;
 }
 if(state.page==PAGE_COVERS){
  MaizoCoverDownloadStatus d=maizo_cover_download_status();
  if(i->back||i->settings){maizo_cover_download_cancel();inspect_cover_library();state.page=PAGE_COVER_PICK;}
  else if(!d.active&&i->accept){inspect_cover_library();state.page=PAGE_COVER_PICK;}
  return;
 }
 if(state.page==PAGE_COVER_PICK){
  if(i->back||i->settings){state.page=PAGE_SETTINGS;return;}
  if(i->up&&state.covers_row>0)state.covers_row--;
  if(i->down&&state.covers_row<15)state.covers_row++;
  if(i->accept||i->left||i->right){
   int row=state.covers_row,dir=i->left?-1:1;
   if(row<7)state.covers_mask^=(1u<<row);
   else if(row==7)state.covers_mask=0x7fu;
   else if(row==8)state.covers_mask=0;
   else if(row==9)state.covers_update=!state.covers_update;
   else if(row==10)config.cover_source=(config.cover_source+3+dir)%3;
   else if(row==11)config.cover_art=(config.cover_art+3+dir)%3;
   else if(row==12)config.cover_region=(config.cover_region+5+dir)%5;
   else if(row==13)state.covers_mode=(state.covers_mode+3+dir)%3;
   else if(row==14){
    cover_reset();
    snprintf(state.info,sizeof(state.info),
         "CACHE DE MEMORIA LIMPO. CAPAS NO SD/USB PRESERVADAS.");
    state.info_return=PAGE_COVER_PICK;state.page=PAGE_INFO;
   }else if(row==15){
    if(!state.covers_mask){
     snprintf(state.info,sizeof(state.info),"SELECIONE PELO MENOS UM SISTEMA");
     state.info_return=PAGE_COVER_PICK;state.page=PAGE_INFO;
    }else{
     maizo_cover_download_custom(&library_state,state.covers_mask,state.covers_update,
       (int)config.cover_source,(int)config.cover_art,(int)config.cover_region,
       state.covers_mode,state.covers_single);
     state.page=PAGE_COVERS;
    }
   }
   if(row>=10&&row<=12)config_save(&config);
  }
  return;
 }
 if(state.page==PAGE_INFO){
  if(i->back||i->accept)state.page=state.info_return;
  return;
 }
 if(!state.settings_for_game &&
    (state.page==PAGE_LIBRARY||state.page==PAGE_SEARCH)){
  if(i->tab_prev)change_system(-1);
  if(i->tab_next)change_system(1);
 }
 if(i->favorite && state.page==PAGE_LIBRARY){
  int idx=library_visible(&library_state,state.system,state.search,0,state.selected);
  if(idx>=0){
   state.detail=idx;
   config_game_load(&state.game_config,&config,&library_state.games[idx]);
   state.page=PAGE_DETAIL;
  }
  return;
 }
 if(i->settings && state.page!=PAGE_SETTINGS && state.page!=PAGE_SEARCH){
  state.settings_for_game=0;
  state.settings_global=state.page==PAGE_HOME;
  state.settings_return=state.page;
  state.settings_row=state.settings_global?0:2;
  state.page=PAGE_SETTINGS;
  return;
 }
 if(i->search && (state.page==PAGE_LIBRARY||state.page==PAGE_HOME)){state.page=PAGE_SEARCH;state.search_index=1;return;}
 if(i->refresh && state.page==PAGE_HOME){
  mount_storage(0);
  state.storage_usb=storage_usb;state.storage_sd=storage_sd;
  input_storage_ready(media_ready);
  module_refresh_available();
  cover_reset();
  if(media_ready)library_begin(&library_state);
  else library_end(&library_state);
  state.selected=state.scroll=0;
  return;
 }
 /* Library shows all games. No favorites filter. */
 if(state.page==PAGE_HOME){
  if(i->left)change_system(-1);if(i->right)change_system(1);
  if(i->up)change_system(config.layout==0?-4:-1);
  if(i->down)change_system(config.layout==0?4:1);
  if(i->accept){state.page=PAGE_LIBRARY;state.in_system=1;state.selected=state.scroll=0;}
  return;
 }
 if(state.page==PAGE_SETTINGS){
  System scope=state.settings_global?SYSTEM_COUNT:state.system;
  if(!state.settings_for_game &&
     state.settings_row==settings_count(scope)-3 && (i->accept||i->right)){
   if(!state.settings_global&&(state.system==SYSTEM_MEDIA||state.system==SYSTEM_HB)){
    /* These modules do not expose public boxart providers. Offer a REAL
     * rescan instead of a misleading or non-functional download control. */
    library_begin(&library_state);
    cover_reset();
    state.selected=state.scroll=0;
    state.page=PAGE_LIBRARY;
    return;
   }
   state.covers_row=0;
   state.covers_mask=state.settings_global?0x7fu:(1u<<state.system);
   state.covers_mode=0;
   state.covers_single=state.settings_global?-1:library_visible(&library_state,state.system,"",0,state.selected);
   inspect_cover_library();state.page=PAGE_COVER_PICK;return;
  }
  if(state.settings_global && state.settings_row==settings_count(scope)-2 && (i->accept||i->right)){
   state.calibration_row=0;state.page=PAGE_CALIBRATION;return;
  }
  if(i->refresh){state.page=PAGE_HID;return;}
  if(i->up&&state.settings_row>(state.settings_global?0:2))state.settings_row--;
  if(i->down&&state.settings_row<settings_count(scope)-1)state.settings_row++;
  if(!state.settings_global&&!state.settings_for_game && state.settings_row==settings_count(scope)-2)
   state.settings_row+=(i->up?-1:1);
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
  if(i->back){size_t len=strlen(state.search);if(len)state.search[len-1]=0;else state.page=state.in_system?PAGE_LIBRARY:PAGE_HOME;}
  if(i->settings){state.page=PAGE_LIBRARY;state.scroll=state.selected=0;}
  return;
 }
 if(state.page==PAGE_LIBRARY){
  int n=total();
  if(n>0){
   int columns=config.layout==0?4:1;
   /* Carousel moves horizontally one cover at a time, while grid/list
    * use their own navigation geometry. */
   if(config.layout==2){
    if(i->left)state.selected=(state.selected+n-1)%n;
    if(i->right)state.selected=(state.selected+1)%n;
    if(i->up)state.selected=(state.selected+n-1)%n;
    if(i->down)state.selected=(state.selected+1)%n;
   }else{
    if(i->up)state.selected=maizo_nav_move(state.selected,n,columns,0,-1);
    if(i->down)state.selected=maizo_nav_move(state.selected,n,columns,0,1);
    if(i->left)state.selected=maizo_nav_move(state.selected,n,columns,1,-1);
    if(i->right)state.selected=maizo_nav_move(state.selected,n,columns,1,1);
   }
  }
  clamp_selection();
  
  if(i->accept){
   int idx=library_visible(&library_state,state.system,state.search,0,state.selected);
   if(idx>=0)play_game(idx,PAGE_LIBRARY);
  }
  if(i->back||i->refresh){
   state.page=PAGE_HOME;state.in_system=0;state.search[0]=0;
  }
  return;
 }
 if(state.page==PAGE_DETAIL){
  if(i->back){state.page=PAGE_LIBRARY;return;}
  if(i->favorite && state.detail>=0 && state.detail<library_state.count){
   config_game_load(&state.game_config,&config,&library_state.games[state.detail]);
   state.settings_for_game=1;state.settings_global=0;state.settings_return=PAGE_DETAIL;
   state.settings_row=2;state.page=PAGE_SETTINGS;return;
  }
  if(i->accept)play_game(state.detail,PAGE_DETAIL);
 }
}
/* Start the software VI framebuffer first. FAT/USB HID may block:
 * a visible boot-stage label gives us a precise failure boundary. */
int main(int argc,char**argv){
 (void)argc;(void)argv;
 if(!ui_init())return 1;
 ui_boot_message("VIDEO PREPARADO");
 VIDEO_WaitVSync();
 ui_boot_message("MONTANDO DISPOSITIVOS...");
 int storage_ready=mount_storage(1);
 ui_boot_message(storage_ready?
  (storage_usb?"USB MONTADO - CARREGANDO DADOS":"SD MONTADO - CARREGANDO DADOS"):
  "ERRO USB E SD - REINSIRA, START/SELECT REVER");
 config_load(&config);
 maizo_video_force_vi704(config.force_vi704);
 maizo_video_frontend_select(config.frontend_video_mode);
 maizo_video_set_calibration((int)config.video_x-50,(int)config.video_y-50,config.video_w,config.video_h);
 memset(&state,0,sizeof(state));
 state.system=SYSTEM_GC;
 state.covers_mask=0x7fu;
 state.info_return=PAGE_HOME;
 state.settings_return=PAGE_HOME;
 state.settings_global=1;
 state.in_system=0;
 state.storage_usb=storage_usb;state.storage_sd=storage_sd;
 /* Start in the carousel, without the first-run wizard. */
  state.page=PAGE_HOME;
 ui_boot_message("CARREGANDO MODULOS...");
 module_refresh_available();
 ui_boot_message("VERIFICANDO LISTA DE JOGOS...");
 if(media_ready)library_begin(&library_state);
 else memset(&library_state,0,sizeof(library_state));
 ui_boot_message("CARREGANDO CAPAS...");
 /* Boot splash makes initialization explicit; the carousel never waits
  * for storage during a redraw. Platform tiles are loaded once here. */
 if(media_ready){
  for(int system=0;system<SYSTEM_COUNT;system++){
   Game icon={0};icon.system=(System)system;
   snprintf(icon.id,sizeof(icon.id),"%s",icon_codes[system]);
   cover_prefetch(&icon);
   icon.id[0]='T';cover_prefetch(&icon);
  }
 }
 ui_boot_message("DETECTANDO CONTROLES...");
 input_init();
 input_storage_ready(media_ready);
 /* Show a complete home frame before the first USB HID probe. */
 ui_draw(&state,&library_state,&config,0);
 for(;;){
  Input in;
  input_poll(&in);
  handle(&in);
  retry_missing_storage();
  if(media_ready)library_step(&library_state,10);
  clamp_selection();
  ui_draw(&state,&library_state,&config,in.hid_present);
  if(state.video_preview_frames>0 && --state.video_preview_frames==0){
   config.frontend_video_mode=state.video_previous_mode;
   maizo_video_frontend_select(state.video_previous_mode);
  }
  if(state.page==PAGE_COVERS)maizo_cover_download_step(&library_state);
  else prefetch_covers();
  state.frame++;
 }
}
