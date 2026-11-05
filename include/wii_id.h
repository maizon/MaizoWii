#ifndef MAIZO_WII_ID_H
#define MAIZO_WII_ID_H
#include <stddef.h>
/* True for a six character Nintendo Wii game identifier. */
int maizo_wii_id_valid(const char *id);
/* Reads the disc identifier from ISO offset 0 or WBFS embedded disc header
 * offset 0x200. Never invents an ID from a title or untrusted filename. */
int maizo_wii_id_from_file(const char *path,char id[7]);
#endif
