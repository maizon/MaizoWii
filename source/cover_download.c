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
#define DOWNLOAD_LIMIT (3u*1024u*1024u)
typedef struct {unsigned char *bytes;size_t length;} Download;
/* Mozilla CA roots are embedded from the pinned Wii-curl sample bundle;
 * never disable peer or hostname verification to work around TLS errors. */
extern const unsigned char maizo_ca_bundle[],maizo_ca_bundle_end[];
static MaizoCoverDownloadStatus job;
static int network_attempted=0,network_ready=0;
static unsigned selected_mask=0x7fu;
static int update_existing=0,cursor=0;
static int chosen_provider=0,chosen_art=0,chosen_region=0,chosen_mode=0,chosen_index=-1;
static unsigned char failed_indices[MAIZO_MAX_GAMES];

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
/* libcurl MULTI advances the HTTPS transfer one small step per UI frame.
 * A failed connection may still block briefly during DNS on Wii libc, but
 * no image download is performed synchronously in ui_draw(). */
static CURLM *multi=NULL;
static CURL *easy=NULL;
static Download received={0};
static Game current_game;
static char current_dest[256];
static int current_index=-1,attempt=0;
static void stop_transfer(void){
 if(easy){
  if(multi)curl_multi_remove_handle(multi,easy);
  curl_easy_cleanup(easy);easy=NULL;
 }
 free(received.bytes);memset(&received,0,sizeof(received));
}
static int start_transfer(const char *url){
 stop_transfer();
 easy=curl_easy_init();
 if(!easy)return 0;
 struct curl_blob ca_info={
  .data=(void*)maizo_ca_bundle,
  .len=(size_t)(maizo_ca_bundle_end-maizo_ca_bundle)-1,
  .flags=CURL_BLOB_NOCOPY
 };
 curl_easy_setopt(easy,CURLOPT_URL,url);
 curl_easy_setopt(easy,CURLOPT_USERAGENT,"MaizoWii/1.0 (Wii; cover-only)");
 curl_easy_setopt(easy,CURLOPT_FOLLOWLOCATION,1L);
 curl_easy_setopt(easy,CURLOPT_MAXREDIRS,3L);
 curl_easy_setopt(easy,CURLOPT_CONNECTTIMEOUT,5L);
 curl_easy_setopt(easy,CURLOPT_TIMEOUT,12L);
 curl_easy_setopt(easy,CURLOPT_NOSIGNAL,1L);
 curl_easy_setopt(easy,CURLOPT_CAINFO_BLOB,&ca_info);
 curl_easy_setopt(easy,CURLOPT_PROTOCOLS_STR,"HTTPS");
 curl_easy_setopt(easy,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(easy,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(easy,CURLOPT_WRITEFUNCTION,receive_image);
 curl_easy_setopt(easy,CURLOPT_WRITEDATA,&received);
 if(!multi)multi=curl_multi_init();
 if(!multi||curl_multi_add_handle(multi,easy)!=CURLM_OK){
  stop_transfer();return 0;
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
 if(rename(tmp,dest)!=0){
  /* libfat may not replace existing files via rename. Preserve the old
   * artwork until the replacement has been fully downloaded and written. */
  char backup[270];snprintf(backup,sizeof(backup),"%s.bak",dest);
  remove(backup);
  if(rename(dest,backup)!=0){remove(tmp);return 0;}
  if(rename(tmp,dest)!=0){rename(backup,dest);remove(tmp);return 0;}
  remove(backup);
 }
 return 1;
}
/* GameTDB indexes verified Wii/GC artwork by six-character disc ID.
 * Libretro distributes Boxarts, Snaps and Titles under a separate public
 * thumbnail namespace. Both providers use exact paths, never fuzzy IDs. */
static const char *retro_repo(const Game *g){
 static const char*name[SYSTEM_COUNT]={
  "Nintendo - Wii","Nintendo - GameCube","Sony - PlayStation",
  "Nintendo - Super Nintendo Entertainment System",
  "Nintendo - Nintendo Entertainment System","Nintendo - Game Boy Advance",
  "Nintendo - Game Boy",NULL,NULL};
 if(g->system==SYSTEM_GB){
  const char *ext=strrchr(g->path,'.');
  if(ext&&!strcasecmp(ext,".gbc"))return "Nintendo - Game Boy Color";
 }
 return g->system>=0&&g->system<SYSTEM_COUNT?name[g->system]:NULL;
}
/* Build one verifiable provider URL per call, without initiating I/O.
 * Each attempt is advanced by the UI event loop, which keeps B responsive. */
static int provider_url(const Game *g,int requested,char *url,size_t cap){
 int is_disc=(g->system==SYSTEM_WII||g->system==SYSTEM_GC)&&strlen(g->id)==6;
 int tdb_count=is_disc&&chosen_provider!=2?(chosen_region?3:7):0;
 if(requested<tdb_count){
  static const char *regions[]={"US","EN","PT","FR","DE","ES","JA"};
  int order[7]={0,1,2,3,4,5,6};
  if(chosen_region==1){order[0]=2;order[1]=1;order[2]=0;}
  if(chosen_region==3){order[0]=1;order[1]=3;order[2]=4;}
  if(chosen_region==4){order[0]=6;order[1]=0;}
  const char *kind=chosen_art==1?"coverfullHQ":chosen_art==2?"cover3D":"cover";
  int written=snprintf(url,cap,"https://art.gametdb.com/%s/%s/%s/%s.png",
    g->system==SYSTEM_WII?"wii":"gc",kind,regions[order[requested]],g->id);
  return written>0&&(size_t)written<cap;
 }
 if(chosen_provider==1)return 0;
 requested-=tdb_count;
 if(requested>=4)return 0;
 const char *repo=retro_repo(g);if(!repo)return 0;
 const char *kind=chosen_art==1?"Named_Snaps":chosen_art==2?"Named_Titles":"Named_Boxarts";
 char candidate[180];snprintf(candidate,sizeof(candidate),"%s",g->name);
 if(requested){
  char *tag=strchr(candidate,'['),*region=strstr(candidate," (");
  if(region&&(!tag||region<tag))tag=region;
  if(tag)*tag=0;
  size_t n=strlen(candidate);
  while(n&&candidate[n-1]==' ')candidate[--n]=0;
  const char *suffix=requested==1?" (USA)":requested==2?" (Europe)":" (Japan)";
  if(chosen_region==4&&requested==1)suffix=" (Japan)";
  if(chosen_region==3&&requested==1)suffix=" (Europe)";
  strncat(candidate,suffix,sizeof(candidate)-strlen(candidate)-1);
 }
 CURL *escape=curl_easy_init();if(!escape)return 0;
 char filename[210];snprintf(filename,sizeof(filename),"%s.png",candidate);
 for(char*ch=filename;*ch;ch++)if(strchr("&*/:\\\"<>?|",*ch))*ch='_';
 char *encoded_repo=curl_easy_escape(escape,repo,0);
 char *encoded_file=curl_easy_escape(escape,filename,0);
 int written=-1;
 if(encoded_repo&&encoded_file)
  written=snprintf(url,cap,"https://thumbnails.libretro.com/%s/%s/%s",
                   encoded_repo,kind,encoded_file);
 if(encoded_repo)curl_free(encoded_repo);
 if(encoded_file)curl_free(encoded_file);
 curl_easy_cleanup(escape);
 return written>0&&(size_t)written<cap;
}
static void finish_game(int success,const char *message){
 if(success){job.downloaded++;cover_forget(&current_game);}
 else {job.failed++;if(current_index>=0&&current_index<MAIZO_MAX_GAMES)
    failed_indices[current_index]=1;}
 job.processed++;
 current_index=-1;stop_transfer();
 snprintf(job.message,sizeof(job.message),"%s",message);
 if(job.processed>=job.total){
  job.active=0;
  if(!success)snprintf(job.message,sizeof(job.message),"CONCLUIDO COM FALHAS");
  else snprintf(job.message,sizeof(job.message),"DOWNLOAD CONCLUIDO");
 }
}
void maizo_cover_download_custom(const Library*library,unsigned mask,int update,
                                int provider,int art,int region,int mode,int index){
 memset(&job,0,sizeof(job));
 selected_mask=mask&0x7fu;update_existing=!!update;
 chosen_provider=provider<0||provider>2?0:provider;
 chosen_art=art<0||art>2?0:art;
 chosen_region=region<0||region>4?0:region;
 chosen_mode=mode<0||mode>2?0:mode;
 chosen_index=index;cursor=0;current_index=-1;stop_transfer();
 if(!network_ready)network_attempted=0;
 if(!library||library->scanning){
  snprintf(job.message,sizeof(job.message),"AGUARDE A INDEXACAO TERMINAR");
  return;
 }
 for(int i=0;i<library->count;i++){
  unsigned ss=(unsigned)library->games[i].system;
  if(ss>=7u||!(selected_mask&(1u<<ss)))continue;
  if(chosen_mode==1&&i!=chosen_index)continue;
  if(chosen_mode==2&&!failed_indices[i])continue;
  job.total++;
 }
 job.active=job.total>0;
 snprintf(job.message,sizeof(job.message),job.active?
   "PREPARANDO DOWNLOAD HTTPS...":"NENHUM JOGO CORRESPONDE A SELECAO");
}
void maizo_cover_download_begin_filtered(const Library*l,unsigned mask,int update){
 maizo_cover_download_custom(l,mask,update,0,0,0,0,-1);
}
void maizo_cover_download_begin(const Library*l){
 maizo_cover_download_begin_filtered(l,0x7fu,0);
}
void maizo_cover_download_cancel(void){
 stop_transfer();
 current_index=-1;
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
  maizo_cover_download_cancel();
  snprintf(job.message,sizeof(job.message),"SEM REDE OU TLS NO WII");return;
 }
 if(easy){
  int running=0;
  CURLMcode mc=curl_multi_perform(multi,&running);
  if(mc==CURLM_OK&&running)return;
  CURLcode result=CURLE_FAILED_INIT;
  long http=0;
  int remaining=0;
  CURLMsg *msg=NULL;
  while((msg=curl_multi_info_read(multi,&remaining))!=NULL){
   if(msg->msg==CURLMSG_DONE&&msg->easy_handle==easy)result=msg->data.result;
  }
  curl_easy_getinfo(easy,CURLINFO_RESPONSE_CODE,&http);
  int valid=mc==CURLM_OK&&result==CURLE_OK&&http==200 &&
     received.length>=8&&memcmp(received.bytes,"\x89PNG\r\n\x1a\n",8)==0;
  if(valid&&save_png_as_gxt(&received,current_dest)){
   finish_game(1,"CAPA VERIFICADA E SALVA");return;
  }
  stop_transfer();attempt++;
  snprintf(job.message,sizeof(job.message),"FONTE SEM CAPA / TENTANDO ALTERNATIVA");
  return;
 }
 if(current_index>=0){
  char url[1024];
  if(provider_url(&current_game,attempt,url,sizeof(url))){
   if(start_transfer(url))return;
   attempt++;return;
  }
  finish_game(0,"CAPA NAO ENCONTRADA NAS FONTES");return;
 }
 if(job.processed>=job.total){
  job.active=0;snprintf(job.message,sizeof(job.message),"DOWNLOAD CONCLUIDO");return;
 }
 while(cursor<library->count){
  const Game*g=&library->games[cursor];
  unsigned sys=(unsigned)g->system;
  if(sys<7u&&(selected_mask&(1u<<sys))&&
     (chosen_mode!=1||cursor==chosen_index)&&
     (chosen_mode!=2||failed_indices[cursor]))break;
  cursor++;
 }
 if(cursor>=library->count){
  job.active=0;snprintf(job.message,sizeof(job.message),"DOWNLOAD CONCLUIDO");return;
 }
 current_index=cursor++;
 current_game=library->games[current_index];
 failed_indices[current_index]=0;
 snprintf(job.current,sizeof(job.current),"%s",current_game.name);
 if(cover_on_disk(&current_game)&&!update_existing){
  job.existing++;job.processed++;current_index=-1;
  snprintf(job.message,sizeof(job.message),"CAPA EXISTENTE - IGNORADA");return;
 }
 if(!cover_destination(&current_game,current_dest,sizeof(current_dest))){
  finish_game(0,"CAMINHO DE CAPA INVALIDO");return;
 }
 const char *dev=!strncmp(current_game.path,"sd:/",4)?"sd:":"usb:";
 char folder[100];
 snprintf(folder,sizeof(folder),"%s/maizo",dev);mkdir(folder,0777);
 snprintf(folder,sizeof(folder),"%s/maizo/covers",dev);mkdir(folder,0777);
 static const char*directories[SYSTEM_COUNT]={"wii","gc","ps1","snes","nes","gba","gb","media","hb"};
 snprintf(folder,sizeof(folder),"%s/maizo/covers/%s",dev,directories[current_game.system]);
 mkdir(folder,0777);
 attempt=0;
 snprintf(job.message,sizeof(job.message),"BUSCANDO CAPA NAS FONTES HTTPS");
}
