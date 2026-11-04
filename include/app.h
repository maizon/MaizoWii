#ifndef MAIZO_APP_H
#define MAIZO_APP_H
#include <gccore.h>
#include <stddef.h>
#define MAIZO_MAX_GAMES 1500
#define MAIZO_PATH 512
#define MAIZO_DEPTH 8
typedef enum { SYSTEM_GC=0, SYSTEM_PS1=1, SYSTEM_MEDIA=2, SYSTEM_COUNT=3 } System;
typedef enum { PAGE_HOME=0, PAGE_LIBRARY, PAGE_DETAIL, PAGE_SEARCH, PAGE_SETTINGS, PAGE_INFO } Page;
typedef struct { System system; char path[MAIZO_PATH],name[112],id[8];unsigned favorite; } Game;
typedef struct { Game games[MAIZO_MAX_GAMES];int count,scanning,root,depth,entries;char status[128];void *work; } Library;
typedef struct { unsigned theme,layout,gc_memcard,gc_wide,gc_cheats,gc_skip_ipl,gc_video,ps1_interpreter,ps1_frameskip,ps1_audio,media_volume; } Config;
typedef struct { Page page;System system;int selected,scroll,settings_row,detail,search_index,favorites,frame;char search[40],info[160]; } State;
typedef struct { int up,down,left,right,accept,back,search,settings,tab_prev,tab_next,refresh,favorite;int hid_present; } Input;
void library_begin(Library*);void library_step(Library*,int);void library_end(Library*);
int library_visible(const Library*,System,const char*,int,int);int library_total(const Library*,System,const char*,int);
void config_defaults(Config*);void config_load(Config*);int config_save(const Config*);
void favorites_load(Library*);int favorites_save(const Library*);
void input_init(void);void input_poll(Input*);void input_shutdown(void);
int cover_draw(const Game*,float,float,float,float);
int ui_init(void);void ui_draw(const State*,const Library*,const Config*,int);void ui_shutdown(void);
int module_launch(const Game*,const Config*,char*,size_t);int module_available(System);
const char* module_name(System);
#endif
