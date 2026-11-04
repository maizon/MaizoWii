/* MAIZO per-game profile filename: portable stable FNV-1a 64-bit.
 * Path is always under media device:/maizo/game-profiles, independent of
 * the display name, to avoid filename traversal and title collisions.
 * GPL-2.0-or-later. This header also compiles in host regression tests.
 */
#ifndef MAIZO_GAME_PROFILE_H
#define MAIZO_GAME_PROFILE_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
static uint64_t maizo_game_hash(unsigned system,const char*path){
 uint64_t hash=UINT64_C(14695981039346656037);
 hash^=(uint64_t)(system&255u);hash*=UINT64_C(1099511628211);
 if(path)for(const unsigned char*p=(const unsigned char*)path;*p;p++){
  hash^=(uint64_t)*p;
  hash*=UINT64_C(1099511628211);
 }
 return hash;
}
static int maizo_game_profile_path(unsigned system,const char*path,
                                    char*dest,size_t length) {
 if(!dest||length<48||!path||!path[0])return 0;
 const char*device=NULL;
 if(!strncmp(path,"sd:/",4))device="sd:";
 else if(!strncmp(path,"usb:/",5))device="usb:";
 else return 0;
 unsigned long long value=(unsigned long long)maizo_game_hash(system,path);
 int written=snprintf(dest,length,"%s/maizo/game-profiles/%016llX.ini",
                      device,value);
 return written>0&&(size_t)written<length;
}
#endif
