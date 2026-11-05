#ifndef MAIZO_PS1_CUE_H
#define MAIZO_PS1_CUE_H
#include <stddef.h>
/* Host-testable, read-only disc preflight. Never mutates games. */
int maizo_probe_ps1_image(const char *path,char *error,size_t cap);
#endif
