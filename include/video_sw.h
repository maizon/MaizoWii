#ifndef MAIZO_VIDEO_SW_H
#define MAIZO_VIDEO_SW_H
/* CPU 2D rendering into the uncached YUYV VI framebuffer.
 * Other emulators continue using their own normal GX backends. */
int maizo_video_init(void);
int maizo_video_frontend_select(unsigned selection); /* frontend only */
void maizo_video_clear(void);
void maizo_video_rect(float,float,float,float,unsigned,unsigned);
void maizo_video_cover(const unsigned char *,unsigned,unsigned,float,float,float,float);
void maizo_video_present(void);
void maizo_video_shutdown(void);
#endif
