#ifndef MAIZONET_WII_H
#define MAIZONET_WII_H
#include <stddef.h>
/* Remote GC boot remains experimental until physically validated. */
void maizonet_wii_probe(char *result,size_t cap);
/* Reads ONLY a 128-KiB game header into an SD/USB stub, never whole ISO. */
int maizonet_wii_prepare(char *stub,size_t stubcap,char *marker,size_t markcap,
                         char id6[7],char *error,size_t errorcap);
void maizonet_wii_set_marker(const char *marker);
const char *maizonet_wii_get_marker(void);
#endif
