/* Native Wii cover downloader. Explicit action from MAIZO settings, NOT
 * synchronous I/O from the drawing path. libcurl handles TLS verification.
 * Only user-selected game artwork is downloaded; never ROM/BIOS data. */
#include "cover_download.h"
#include <curl/curl.h>
#include <png.h>
#include <wiisocket.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define ART_WIDTH 128
#define ART_HEIGHT 176
#define ART_PIXELS (ART_WIDTH*ART_HEIGHT*4u)
#define DOWNLOAD_LIMIT (2u*1024u*1024u)
typedef struct {unsigned char *bytes;size_t length;} Download;
/* Mozilla CA roots are embedded from the pinned Wii-curl sample bundle;
 * never disable peer or hostname verification to work around TLS errors. */
extern const unsigned char maizo_ca_bundle[],maizo_ca_bundle_end[];
static MaizoCoverDownloadStatus job;
static int network_attempted=0,network_ready=0;

static size_t receive_image(char *source,size_t size,size_t count,void *user){
 Download *d=(Download*)user;
 if(size && count>((size_t)-1)/size)return 0;
 size_t bytes=size*count;
 if(bytes>DOWNLOAD_LIMIT-d->length)return 0;
 unsigned char *new_bytes=(unsigned char*)realloc(d->bytes,d->length+bytes);
 if(!new_bytes)return 0;
 d->bytes=new_bytes;
 memcpy(d->bytes+d->length,source,bytes);
 d->length+=bytes;
 return bytes;
}
static int fetch_png(const char *url,Download *image){
 memset(image,0,sizeof(*image));
 CURL *curl=curl_easy_init();
 if(!curl)return 0;
 curl_easy_setopt(curl,CURLOPT_URL,url);
 curl_easy_setopt(curl,CURLOPT_USERAGENT,"MaizoWii/1.0 (Wii; cover-only)");
 curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);
 curl_easy_setopt(curl,CURLOPT_MAXREDIRS,3L);
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,5L);
 curl_easy_setopt(curl,CURLOPT_TIMEOUT,12L);
 curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
 struct curl_blob ca_info={
  .data=(void*)maizo_ca_bundle,
  .len=(size_t)(maizo_ca_bundle_end-maizo_ca_bundle)-1,
  .flags=CURL_BLOB_NOCOPY
 };
 curl_easy_setopt(curl,CURLOPT_CAINFO_BLOB,&ca_info);
 curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"HTTPS");
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive_image);
 curl_easy_setopt(curl,CURLOPT_WRITEDATA,image);
 CURLcode status=curl_easy_perform(curl);
 long http=0;
 curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&http);
 curl_easy_cleanup(curl);
 if(status!=CURLE_OK||http!=200||image->length<8||
    memcmp(image->bytes,"\x89PNG\r\n\x1a\n",8)){
  free(image->bytes);memset(image,0,sizeof(*image));return 0;
 }
 return 1;
}
/* The exact MZG1 RGBA8 4x4 tile format used by cover_draw and the Android
 * Pillow converter. Keep the whole CARD a perfect 8:11 rectangle; preserve
 * source artwork aspect by centering it on an opaque dark canvas. */
static int save_png_as_gxt(const Download *download,const char *dest){
 png_image png;memset(&png,0,sizeof(png));png.version=PNG_IMAGE_VERSION;
 if(!png_image_begin_read_from_memory(&png,download->bytes,download->length))return 0;
 if(!png.width||!png.height||png.width>2048||png.height>2048||
    (uint64_t)png.width*(uint64_t)png.height>1024u*1024u){
  png_image_free(&png);return 0;
 }
 png.format=PNG_FORMAT_RGBA;
 unsigned char *rgba=(unsigned char*)malloc(PNG_IMAGE_SIZE(png));
 if(!rgba){png_image_free(&png);return 0;}
 int read_ok=png_image_finish_read(&png,NULL,rgba,0,NULL);
 const unsigned sw=png.width,sh=png.height;
 png_image_free(&png);
 if(!read_ok){free(rgba);return 0;}
 unsigned target_w=ART_WIDTH,target_h=ART_HEIGHT;
 if((uint64_t)sw*ART_HEIGHT>(uint64_t)sh*ART_WIDTH)
  target_h=(unsigned)((uint64_t)sh*ART_WIDTH/sw);
 else target_w=(unsigned)((uint64_t)sw*ART_HEIGHT/sh);
 if(!target_w)target_w=1;
 if(!target_h)target_h=1;
 unsigned xoff=(ART_WIDTH-target_w)/2,yoff=(ART_HEIGHT-target_h)/2;
 unsigned char *tiles=(unsigned char*)malloc(8+ART_PIXELS);
 if(!tiles){free(rgba);return 0;}
 memcpy(tiles,"MZG1\0\x80\0\xb0",8);
 unsigned char *out=tiles+8;
 for(unsigned y=0;y<ART_HEIGHT;y+=4)for(unsigned x=0;x<ART_WIDTH;x+=4){
  unsigned char pixels[16][4];
  for(unsigned dy=0;dy<4;dy++)for(unsigned dx=0;dx<4;dx++){
   const unsigned xx=x+dx,yy=y+dy,k=dy*4+dx;
   pixels[k][0]=19;pixels[k][1]=24;pixels[k][2]=33;pixels[k][3]=255;
   if(xx<xoff||yy<yoff||xx>=xoff+target_w||yy>=yoff+target_h)continue;
   unsigned sx=(unsigned)(((uint64_t)(xx-xoff)*sw)/target_w);
   unsigned sy=(unsigned)(((uint64_t)(yy-yoff)*sh)/target_h);
   const unsigned char *src=rgba+((size_t)sy*sw+sx)*4;
   memcpy(pixels[k],src,4);pixels[k][3]=255;
  }
  for(unsigned i=0;i<16;i++){*out++=pixels[i][3];*out++=pixels[i][0];}
  for(unsigned i=0;i<16;i++){*out++=pixels[i][1];*out++=pixels[i][2];}
 }
 free(rgba);
 char tmp[270];int valid=snprintf(tmp,sizeof(tmp),"%s.part",dest);
 if(valid<0||(size_t)valid>=sizeof(tmp)){free(tiles);return 0;}
 FILE *f=fopen(tmp,"wb");if(!f){free(tiles);return 0;}
 int good=fwrite(tiles,1,8+ART_PIXELS,f)==8+ART_PIXELS;
 if(fclose(f))good=0;
 free(tiles);
 if(!good){remove(tmp);return 0;}
 if(rename(tmp,dest)!=0){remove(tmp);return 0;}
 return 1;
}
static int provider_download(const Game *g,const char *output){
 Download image;
 char url[1024];int ok=0;
 if((g->system==SYSTEM_WII||g->system==SYSTEM_GC)&&strlen(g->id)==6){
  for(int region=0;region<2&&!ok;region++){
   const char *system=g->system==SYSTEM_WII?"wii":"gc";
   snprintf(url,sizeof(url),"https://art.gametdb.com/%s/cover/%s/%s.png",
            system,region==0?"US":"EN",g->id);
   if(fetch_png(url,&image)){
    ok=save_png_as_gxt(&image,output);free(image.bytes);
   }
  }
 }else if(g->system==SYSTEM_PS1||g->system==SYSTEM_SNES||
          g->system==SYSTEM_NES||g->system==SYSTEM_GBA||g->system==SYSTEM_GB){
  static const char *repos[SYSTEM_COUNT]={
   NULL,NULL,"Sony_-_PlayStation",
   "Nintendo_-_Super_Nintendo_Entertainment_System",
   "Nintendo_-_Nintendo_Entertainment_System",
   "Nintendo_-_Game_Boy_Advance","Nintendo_-_Game_Boy",NULL
  };
  const char *repo=repos[g->system];
  if(g->system==SYSTEM_GB){
   const char *ext=strrchr(g->path,'.');
   if(ext&&!strcasecmp(ext,".gbc"))repo="Nintendo_-_Game_Boy_Color";
  }
  char title[128];snprintf(title,sizeof(title),"%s",g->name);
  /* Respect Libretro's exact filenames first; then common region suffixes. */
  for(int attempt=0;attempt<3&&!ok;attempt++){
   char candidate[160];snprintf(candidate,sizeof(candidate),"%s",title);
   if(attempt){
    char *bracket=strchr(candidate,'[');if(bracket)*bracket=0;
    size_t len=strlen(candidate);
    while(len&&candidate[len-1]==' ')candidate[--len]=0;
    if(attempt==1)strncat(candidate," (USA)",sizeof(candidate)-strlen(candidate)-1);
    else strncat(candidate," (Europe)",sizeof(candidate)-strlen(candidate)-1);
   }
   CURL *escape_curl=curl_easy_init();
   if(!escape_curl)break;
   char filename[190];snprintf(filename,sizeof(filename),"%s.png",candidate);
   char *escaped=curl_easy_escape(escape_curl,filename,0);
   if(escaped){
    snprintf(url,sizeof(url),
       "https://raw.githubusercontent.com/libretro-thumbnails/%s/master/Named_Boxarts/%s",
       repo,escaped);
    curl_free(escaped);
    if(fetch_png(url,&image)){ok=save_png_as_gxt(&image,output);free(image.bytes);}
   }
   curl_easy_cleanup(escape_curl);
  }
 }
 return ok;
}
void maizo_cover_download_begin(const Library *library){
 memset(&job,0,sizeof(job));
 if(!library||library->scanning){
  snprintf(job.message,sizeof(job.message),"AGUARDE A VARREDURA USB/SD TERMINAR");
  return;
 }
 job.total=library->count;
 job.active=job.total>0;
 snprintf(job.message,sizeof(job.message),job.active?
          "PREPARANDO CONEXAO HTTPS...":"BIBLIOTECA VAZIA");
}
void maizo_cover_download_cancel(void){
 job.active=0;
 snprintf(job.message,sizeof(job.message),"INTERROMPIDO - CAPAS JA BAIXADAS PRESERVADAS");
}
MaizoCoverDownloadStatus maizo_cover_download_status(void){return job;}
void maizo_cover_download_step(const Library *library){
 if(!job.active||!library)return;
 if(!network_attempted){
  network_attempted=1;
  if(wiisocket_init()==0 && curl_global_init(CURL_GLOBAL_DEFAULT)==CURLE_OK)
   network_ready=1;
 }
 if(!network_ready){
  job.active=0;snprintf(job.message,sizeof(job.message),
                       "SEM REDE OU TLS DISPONIVEL NO WII");return;
 }
 if(job.processed>=job.total||job.processed>=library->count){
  job.active=0;snprintf(job.message,sizeof(job.message),"DOWNLOAD CONCLUIDO");return;
 }
 const Game *g=&library->games[job.processed++];
 snprintf(job.current,sizeof(job.current),"%s",g->name);
 if(g->system==SYSTEM_MEDIA){
  job.unavailable++;snprintf(job.message,sizeof(job.message),
    "MIDIA: ADICIONE POSTER LOCAL (SEM PROVEDOR PUBLICO)");return;
 }
 if(cover_on_disk(g)){
  job.existing++;snprintf(job.message,sizeof(job.message),"CAPA EXISTENTE - IGNORADA");
 }else{
  char path[240];
  if(!cover_destination(g,path,sizeof(path))){
   job.failed++;snprintf(job.message,sizeof(job.message),"CAMINHO DA CAPA INVALIDO");
  }else{
   const char *device=!strncmp(g->path,"sd:/",4)?"sd:":"usb:";
   char folder[100];
   snprintf(folder,sizeof(folder),"%s/maizo",device);mkdir(folder,0777);
   snprintf(folder,sizeof(folder),"%s/maizo/covers",device);mkdir(folder,0777);
   static const char *subdirs[SYSTEM_COUNT]={
    "wii","gc","ps1","snes","nes","gba","gb","media"
   };
   snprintf(folder,sizeof(folder),"%s/maizo/covers/%s",device,subdirs[g->system]);
   mkdir(folder,0777);
   if(provider_download(g,path)){
    job.downloaded++;cover_forget(g);
    snprintf(job.message,sizeof(job.message),"CAPA BAIXADA E CONVERTIDA: OK");
   }else{
    job.failed++;snprintf(job.message,sizeof(job.message),"NAO ENCONTRADA / ERRO HTTPS");
   }
  }
 }
 if(job.processed>=job.total){
  job.active=0;
  snprintf(job.message,sizeof(job.message),"FINALIZADO: CONFIRA O RESULTADO");
 }
}
