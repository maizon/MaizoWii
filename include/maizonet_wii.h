#ifndef MAIZONET_WII_H
#define MAIZONET_WII_H
#include <stddef.h>
/* Diagnostic only: guest SMB1 transport does not launch GC games. */
void maizonet_wii_probe(char *result,size_t cap);
#endif
