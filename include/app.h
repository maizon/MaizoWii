#ifndef MAIZO_APP_H
#define MAIZO_APP_H
#include <gccore.h>
#include <stddef.h>
#define MAIZO_MAX_GAMES 1500
#define MAIZO_PATH 512
#define MAIZO_DEPTH 8
typedef enum { SYSTEM_WII=0, SYSTEM_GC, SYSTEM_PS1, SYSTEM_SNES, SYSTEM_NES, SYSTEM_GBA, SYSTEM_GB, SYSTEM_MEDIA, SYSTEM_COUNT } System;
typedef enum { PAGE_HOME=0, PAGE_LIBRARY, PAGE_DETAIL, PAGE_SEARCH, PAGE_SETTINGS, PAGE_INFO } Page;
typedef struct { System system; char path[MAIZO_PATH],name[112],id[8];unsigned favorite; } Game;
typedef struct { Game games[MAIZO_MAX_GAMES];int count,scanning,root,depth,entries;char status[128];void *work; } Library;
typedef struct { unsigned theme,layout,wii_ios,wii_usb_port,gc_memcard,gc_wide,gc_cheats,gc_skip_ipl,gc_video,gc_mc_multi,gc_native_si,gc_rem_limit,ps1_interpreter,ps1_frameskip,ps1_audio,media_volume; } Config;
typedef struct { Page page;System system;int selected,scroll,settings_row,detail,search_index,favorites,frame;char search[40],info[160]; } State;
typedef struct { int up,down,left,right,accept,back,search,settings,tab_prev,tab_next,refresh,favorite;int hid_present; } Input;
typedef struct {
 unsigned vid,pid,reports,read_errors;
 int connected,profile_loaded;
} HidDiagnostics;
void library_begin(Library*);void library_step(Library*,int);void library_end(Library*);
int library_visible(const Library*,System,const char*,int,int);int library_total(const Library*,System,const char*,int);
const char* system_title(System);
int settings_count(System);const char*settings_label(System,int);
void settings_value(const Config*,System,int,char*,size_t);
int settings_change(Config*,System,int,int);
void config_defaults(Config*);void config_load(Config*);int config_save(const Config*);
void favorites_load(Library*);int favorites_save(const Library*);
void input_init(void);void input_poll(Input*);void input_shutdown(void);
void input_hid_diagnostics(HidDiagnostics*);
int cover_draw(const Game*,float,float,float,float);
int ui_init(void);void ui_draw(const State*,const Library*,const Config*,int);void ui_shutdown(void);
int module_launch(const Game*,const Config*,char*,size_t);int module_available(System);
const char* module_name(System);
#endif
