#ifndef MAIZO_COVER_DOWNLOAD_H
#define MAIZO_COVER_DOWNLOAD_H
#include "app.h"
typedef struct {
 int active,processed,total,downloaded,existing,failed,unavailable;
 char current[112],message[128];
} MaizoCoverDownloadStatus;
void maizo_cover_download_begin(const Library *library);
void maizo_cover_download_begin_filtered(const Library *library,unsigned mask,int update_existing);
/* provider: 0 auto, 1 GameTDB, 2 Libretro.
 * artwork: 0 frontal, 1 completa/snapshot, 2 3D/title.
 * region: 0 auto, 1 PT/BR, 2 US, 3 Europe, 4 Japan.
 * mode: 0 normal, 1 one selected game, 2 retry last failures. */
void maizo_cover_download_custom(const Library *,unsigned,int,int,int,int,int,int);
void maizo_cover_download_step(const Library *library);
void maizo_cover_download_cancel(void);
MaizoCoverDownloadStatus maizo_cover_download_status(void);
#endif
