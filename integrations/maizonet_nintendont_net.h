#ifndef MAIZONET_NINTENDONT_NET_H
#define MAIZONET_NINTENDONT_NET_H
#include "global.h"
bool maizonet_nin_requested(const char *marker);
bool maizonet_nin_open(const char *marker);
bool maizonet_nin_read(void *out,u32 size,u64 offset);
void maizonet_nin_close(void);
/* Error codes and final-state log are available only in experimental mode. */
int maizonet_nin_last_error(void);
bool maizonet_nin_failed(void);
void maizonet_nin_fatal(const char *step,u64 offset,u32 size);

#endif
