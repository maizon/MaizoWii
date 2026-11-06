#ifndef MAIZO_COVER_DOWNLOAD_H
#define MAIZO_COVER_DOWNLOAD_H
#include "app.h"
typedef struct {
 int active,processed,total,downloaded,existing,failed,unavailable;
 char current[112],message[128];
} MaizoCoverDownloadStatus;
void maizo_cover_download_begin(const Library *library);
void maizo_cover_download_begin_filtered(const Library *library,unsigned mask,int update_existing);
void maizo_cover_download_step(const Library *library);
void maizo_cover_download_cancel(void);
MaizoCoverDownloadStatus maizo_cover_download_status(void);
#endif
