#ifndef MAIZO_APP_H
#define MAIZO_APP_H
#include <gccore.h>
#include <stddef.h>
#include "wiimc_options.h"
#define MAIZO_MAX_GAMES 1500
#define MAIZO_PATH 512
#define MAIZO_DEPTH 8
typedef enum { SYSTEM_WII=0, SYSTEM_GC, SYSTEM_PS1, SYSTEM_SNES, SYSTEM_NES, SYSTEM_GBA, SYSTEM_GB, SYSTEM_MEDIA, SYSTEM_HB, SYSTEM_COUNT } System;
typedef enum { PAGE_HOME=0, PAGE_LIBRARY, PAGE_DETAIL, PAGE_SEARCH, PAGE_SETTINGS, PAGE_INFO, PAGE_HID, PAGE_CALIBRATION, PAGE_COVERS, PAGE_COVER_PICK, PAGE_WELCOME } Page;
typedef struct { System system; char path[MAIZO_PATH],name[112],id[8],description[96],region[12];unsigned short year;unsigned favorite; } Game;
typedef struct { Game games[MAIZO_MAX_GAMES];int count,scanning,root,depth,entries;char status[128];void *work; } Library;
typedef struct { unsigned theme,layout,cover_source,cover_art,cover_region,wii_ios,wii_usb_port,gc_memcard,gc_wide,gc_cheats,gc_skip_ipl,gc_video,gc_mc_multi,gc_native_si,gc_rem_limit,ps1_interpreter,ps1_frameskip,ps1_audio,media_volume,ps1_engine,ps1_native,ps1_video,ps1_aspect,ps1_deflicker,ps1_dither,ps1_fps,ps1_limit,gc_scale,gc_offset,video_x,video_y,video_w,video_h,media_player,media_autoboot,frontend_video_mode,game_x240,game_x480,game_w240,game_w480;unsigned wiimc_opts[MAIZO_WIIMC_OPTION_COUNT],wiimc_apply; } Config;
typedef struct { Page page;System system;int selected,scroll,settings_row,calibration_row,detail,search_index,favorites,frame,settings_for_game;Page info_return,settings_return;int storage_usb,storage_sd,video_preview_frames,settings_global,in_system;unsigned covers_mask;int covers_row,covers_update,covers_mode,covers_single;
int covers_total[7],covers_ready[7];unsigned video_previous_mode;Config game_config;char search[40],info[160]; } State;
typedef struct { int up,down,left,right,accept,back,search,settings,tab_prev,tab_next,refresh,favorite;int hid_present; } Input;
typedef struct {
 unsigned vid,pid,reports,read_errors,last_report_id;
 unsigned endpoint,packet_size,multiin,connected_ports;
 unsigned held[4];
 int stick_x[4],stick_y[4];
 int connected,profile_loaded; /* 0=unknown, 1=controller.ini, 2=HID descriptor */
} HidDiagnostics;
void library_begin(Library*);void library_step(Library*,int);void library_end(Library*);
int library_visible(const Library*,System,const char*,int,int);int library_total(const Library*,System,const char*,int);
const char* system_title(System);
int settings_count(System);const char*settings_label(System,int);
const char*settings_category(System,int);
void settings_value(const Config*,System,int,char*,size_t);
int settings_change(Config*,System,int,int);
void config_defaults(Config*);void config_load(Config*);int config_save(const Config*);int config_has_settings(void);
void config_game_load(Config*,const Config*,const Game*);int config_game_save(const Config*,const Game*);
void favorites_load(Library*);int favorites_save(const Library*);
void input_init(void);void input_poll(Input*);void input_shutdown(void);
void input_storage_ready(int);
void input_hid_diagnostics(HidDiagnostics*);
void module_refresh_available(void);
int cover_draw(const Game*,float,float,float,float);
int cover_draw_icon(const Game*,float,float,float,float);
void cover_prefetch(const Game*);
void cover_reset(void);
int cover_destination(const Game*,char*,size_t);
int cover_on_disk(const Game*);
void cover_forget(const Game*);
unsigned cover_tint(const Game*);
int ui_init(void);void ui_boot_message(const char*);
void ui_draw(const State*,const Library*,const Config*,int);void ui_shutdown(void);
void maizo_video_set_calibration(int,int,unsigned,unsigned);
int module_launch(const Game*,const Config*,char*,size_t);int module_available(System);int module_variant_available(System,const Config*);
const char* module_name(System);
#endif
