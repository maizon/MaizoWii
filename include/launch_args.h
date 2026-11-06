#ifndef MAIZO_LAUNCH_ARGS_H
#define MAIZO_LAUNCH_ARGS_H
#include <stddef.h>
/* Portable, testable argv contract: system values mirror include/app.h.
 * Each argument is NUL terminated in data; length includes final NUL.
 * No dependencies on libogc, IOS or filesystem. */
enum {
 MAIZO_ARG_WII=0, MAIZO_ARG_GC, MAIZO_ARG_PS1, MAIZO_ARG_SNES,
 MAIZO_ARG_NES, MAIZO_ARG_GBA, MAIZO_ARG_GB, MAIZO_ARG_MEDIA, MAIZO_ARG_HB,
 MAIZO_ARG_SYSTEM_COUNT
};
#define MAIZO_COMMAND_CAP 2048u
typedef struct {
 int system;
 const char *executable, *path, *id;
 unsigned wii_ios, wii_usb_port;
 unsigned ps1_interpreter, ps1_frameskip, ps1_audio;
 unsigned ps1_engine,ps1_native,ps1_video,ps1_aspect,ps1_deflicker,ps1_dither,ps1_fps,ps1_limit;
 unsigned media_autoboot;
} MaizoLaunchSpec;
typedef struct {
 char data[MAIZO_COMMAND_CAP];
 unsigned argc;
 size_t length;
} MaizoLaunchArgs;
/* Returns 1 on success, 0 on rejection. Error is always set on failure if cap>0. */
int maizo_build_args(const MaizoLaunchSpec*, MaizoLaunchArgs*, char*error, size_t cap);
#endif
