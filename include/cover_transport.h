#ifndef MAIZO_COVER_TRANSPORT_H
#define MAIZO_COVER_TRANSPORT_H
#include <stddef.h>
/* Modes: 0 auto HTTPS with image-only compatibility on cert error;
 * 1 HTTPS verified only; 2 USB Loader GX HTTP images, HTTPS for Libretro. */
int maizo_cover_gametdb_http(const char *secure,char *out,size_t cap);
int maizo_cover_allow_legacy_retry(unsigned mode,int curl_error,const char *url);
#endif
