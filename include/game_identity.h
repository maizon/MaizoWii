#ifndef MAIZO_GAME_IDENTITY_H
#define MAIZO_GAME_IDENTITY_H
#include <stddef.h>
/* Read-only native ROM/media header probing. Systems match include/app.h.
 * Returns 1 only when a known header validates. Never invent a disc ID.
 * GBA product code isn't a Libretro title index: it is diagnostic metadata. */
int maizo_game_probe_identity(const char *path,int system,
                             char *product,size_t product_cap,
                             char *internal,size_t internal_cap);
#endif
