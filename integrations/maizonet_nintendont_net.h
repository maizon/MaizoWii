#ifndef MAIZONET_NINTENDONT_NET_H
#define MAIZONET_NINTENDONT_NET_H
#include "global.h"
bool maizonet_nin_requested(const char *marker);
bool maizonet_nin_open(const char *marker);
bool maizonet_nin_read(void *out,u32 size,u64 offset);
void maizonet_nin_close(void);
#endif
