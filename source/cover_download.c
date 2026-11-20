/* Native Wii cover downloader. Explicit action from MAIZO settings, NOT
 * synchronous I/O from the drawing path. libcurl handles TLS verification.
 * Only user-selected game artwork is downloaded; never ROM/BIOS data. */
#include "cover_download.h"
#include "cover_transport.h"
#include "wii_id.h"
#include <curl/curl.h>
#include <png.h>
#include <wiisocket.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

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
static unsigned transport_setting=0;
/* Provider circuit breaker: 1 GameTDB, 2 Libretro CDN, 4 GitHub CDN.
 * Repeated TLS/connection failures should not stall all games. */
static unsigned unreachable_hosts=0;
static unsigned network_retry_frame=0;
static unsigned selected_mask=0x7fu;
static int update_existing=0,cursor=0;
static int wait_for_index=0;
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
static char last_transfer_error[100];
static int http_legacy=0;
static char requested_url[1024];
static char curl_error[CURL_ERROR_SIZE];
static struct curl_blob ca_bundle_blob={NULL,0,CURL_BLOB_NOCOPY};
void maizo_cover_download_set_transport(unsigned mode){transport_setting=mode<=2?mode:0;}
/* Optional user-managed updated Mozilla roots, without changing the TLS trust rules. */
static const char*maizo_ca_override(void){
 static const char *const paths[]={"usb:/maizo/cacert.pem","sd:/maizo/cacert.pem"};
 for(unsigned i=0;i<2;i++){
  FILE*f=fopen(paths[i],"rb");if(!f)continue;
  char probe[768]={0};
  size_t n=fread(probe,1,sizeof(probe)-1,f);
  fclose(f);
  if(n>128&&strstr(probe,"-----BEGIN CERTIFICATE-----"))return paths[i];
 }
 return NULL;
}

static void stop_transfer(void){
 if(easy){
  if(multi)curl_multi_remove_handle(multi,easy);
  curl_easy_cleanup(easy);easy=NULL;
 }
 free(received.bytes);memset(&received,0,sizeof(received));
}
static int start_transfer(const char*url){
 stop_transfer();
 if(!url)return 0;
 http_legacy=!strncmp(url,"http://",7);
 if(http_legacy){
  /* Reject arbitrary HTTP redirects, hosts or file names. */
  char secure[1024],whitelisted[1024];
  int n=snprintf(secure,sizeof(secure),"https://%s",url+7);
  if(n<1||(size_t)n>=sizeof(secure)||
     !maizo_cover_gametdb_http(secure,whitelisted,sizeof(whitelisted))||
     strcmp(url,whitelisted))return 0;
 }
 easy=curl_easy_init();
 if(!easy)return 0;
 snprintf(requested_url,sizeof(requested_url),"%s",url);
 memset(curl_error,0,sizeof(curl_error));
 ca_bundle_blob.data=(void*)maizo_ca_bundle;
 ca_bundle_blob.len=(size_t)(maizo_ca_bundle_end-maizo_ca_bundle)-1;
 ca_bundle_blob.flags=CURL_BLOB_NOCOPY;
 curl_easy_setopt(easy,CURLOPT_ERRORBUFFER,curl_error);
 curl_easy_setopt(easy,CURLOPT_URL,url);
 curl_easy_setopt(easy,CURLOPT_USERAGENT,"MaizoWii/1.1 (Wii cover art)");
 curl_easy_setopt(easy,CURLOPT_FOLLOWLOCATION,http_legacy?0L:1L);
 curl_easy_setopt(easy,CURLOPT_MAXREDIRS,3L);
 /* Real Wii networking took >8 seconds to connect. Use a finite 15s
  * connection budget, 32s total, and skip a failed host afterwards. */
 curl_easy_setopt(easy,CURLOPT_CONNECTTIMEOUT,15L);
 curl_easy_setopt(easy,CURLOPT_TIMEOUT,32L);
 curl_easy_setopt(easy,CURLOPT_IPRESOLVE,CURL_IPRESOLVE_V4);
 curl_easy_setopt(easy,CURLOPT_DNS_CACHE_TIMEOUT,600L);
 curl_easy_setopt(easy,CURLOPT_NOSIGNAL,1L);
 curl_easy_setopt(easy,CURLOPT_LOW_SPEED_LIMIT,64L);
 curl_easy_setopt(easy,CURLOPT_LOW_SPEED_TIME,10L);
 curl_easy_setopt(easy,CURLOPT_PROTOCOLS_STR,http_legacy?"HTTP":"HTTPS");
 curl_easy_setopt(easy,CURLOPT_REDIR_PROTOCOLS_STR,"HTTPS");
 curl_easy_setopt(easy,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(easy,CURLOPT_SSL_VERIFYHOST,2L);
 if(!http_legacy){
  const char*pem=maizo_ca_override();
  CURLcode ca=pem?curl_easy_setopt(easy,CURLOPT_CAINFO,pem):
                  curl_easy_setopt(easy,CURLOPT_CAINFO_BLOB,&ca_bundle_blob);
  if(ca!=CURLE_OK){
   snprintf(job.message,sizeof(job.message),"CERTIFICADOS: CURL %d",(int)ca);
   stop_transfer();return 0;
  }
 }
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
static unsigned maizo_cover_height(System system){
  /* Full disc cases and classic carts remain portrait. PS1 front is a
   * square jewel case; GB/GBC cartons are less elongated than Wii cases. */
  return system==SYSTEM_PS1?128u:system==SYSTEM_GB?160u:ART_HEIGHT;
}
static int save_png_as_gxt(const Download *download,const char *dest,System system){
 const unsigned art_height=maizo_cover_height(system);
 const size_t art_bytes=(size_t)ART_WIDTH*art_height*4u;
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
 unsigned target_w=ART_WIDTH,target_h=art_height;
 if((uint64_t)sw*art_height>(uint64_t)sh*ART_WIDTH)
  target_h=(unsigned)((uint64_t)sh*ART_WIDTH/sw);
 else target_w=(unsigned)((uint64_t)sw*art_height/sh);
 if(!target_w)target_w=1;
 if(!target_h)target_h=1;
 unsigned xoff=(ART_WIDTH-target_w)/2,yoff=(art_height-target_h)/2;
 unsigned char *tiles=(unsigned char*)malloc(8+art_bytes);
 if(!tiles){free(rgba);return 0;}
 memcpy(tiles,"MZG1",4);tiles[4]=0;tiles[5]=ART_WIDTH;tiles[6]=0;tiles[7]=art_height;
 unsigned char *out=tiles+8;
 for(unsigned y=0;y<art_height;y+=4)for(unsigned x=0;x<ART_WIDTH;x+=4){
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
 int good=fwrite(tiles,1,8+art_bytes,f)==8+art_bytes;
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
static const char *retro_repo_raw(const Game*g){
 static const char*const repos[SYSTEM_COUNT]={
  "Nintendo_-_Wii","Nintendo_-_GameCube","Sony_-_PlayStation",
  "Nintendo_-_Super_Nintendo_Entertainment_System",
  "Nintendo_-_Nintendo_Entertainment_System",
  "Nintendo_-_Game_Boy_Advance","Nintendo_-_Game_Boy",NULL,NULL
 };
 if(!g||g->system<0||g->system>=SYSTEM_COUNT)return NULL;
 if(g->system==SYSTEM_GB){
  const char*ext=strrchr(g->path,'.');
  if(ext&&!strcasecmp(ext,".gbc"))return "Nintendo_-_Game_Boy_Color";
 }
 return repos[g->system];
}
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
 int is_disc=(g->system==SYSTEM_WII||g->system==SYSTEM_GC)&&maizo_wii_id_valid(g->id);
 int regions_per_art=chosen_region?3:7;
 int tdb_count=is_disc&&chosen_provider!=2?regions_per_art*(chosen_art?2:1):0;
 if(requested<tdb_count){
  static const char *regions[]={"US","EN","PT","FR","DE","ES","JA"};
  int order[7]={0,1,2,3,4,5,6};
  if(chosen_region==1){order[0]=2;order[1]=1;order[2]=0;}
  if(chosen_region==3){order[0]=1;order[1]=3;order[2]=4;}
  if(chosen_region==4){order[0]=6;order[1]=0;}
  const int variant=requested/regions_per_art;
  const int ix=requested%regions_per_art;
  const char *kind=chosen_art==1?(variant?"coverfull":"coverfullHQ"):
                   chosen_art==2?(variant?"cover":"cover3D"):"cover";
  int written=snprintf(url,cap,"https://art.gametdb.com/%s/%s/%s/%s.png",
    g->system==SYSTEM_WII?"wii":"gc",kind,regions[order[ix]],g->id);
  return written>0&&(size_t)written<cap;
 }
 if(chosen_provider==1)return 0;
 requested-=tdb_count;
 /* Three non-fuzzy title sources: original ROM basename (No-Intro),
  * localized/display database title, then internal cartridge header title.
  * Each gets CDN first, then the official Libretro GitHub mirror.
  * No claim of hash-exact identification without a No-Intro DAT database. */
 if(requested>=24)return 0;
 const int source=requested/8;
 const int raw=((requested%8)/4)!=0;
 requested%=4;
 const char *repo=raw?retro_repo_raw(g):retro_repo(g);
 if(!repo)return 0;
 const char *kind=chosen_art==1?"Named_Snaps":
                   chosen_art==2?"Named_Titles":"Named_Boxarts";
 char candidate[180]={0};
 if(source==0){
  const char *name=strrchr(g->path,'/');
  name=name?name+1:g->path;
  if(!strcasecmp(name,"game.iso")||!strcasecmp(name,"disc2.iso")||
     !strcasecmp(name,"game.ciso")||!strcasecmp(name,"game.gcm")){
   /* Nintendont's filename is generic: use the game folder title. */
   const char *parent=g->path;
   const char *cut=name>parent?name-1:name;
   size_t size=(size_t)(cut-parent);
   const char *begin=parent;
   for(const char *p=parent;p<cut;p++)if(*p=='/')begin=p+1;
   size=(size_t)(cut-begin);
   if(size>=sizeof(candidate))size=sizeof(candidate)-1;
   memcpy(candidate,begin,size);candidate[size]=0;
  }else snprintf(candidate,sizeof(candidate),"%s",name);
  char *ext=strrchr(candidate,'.');if(ext)*ext=0;
 }else if(source==1)snprintf(candidate,sizeof(candidate),"%s",g->name);
 else snprintf(candidate,sizeof(candidate),"%s",g->lookup_title);
 if(!candidate[0])return 0;
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
 char *encoded_repo=raw?NULL:curl_easy_escape(escape,repo,0);
 char *encoded_file=curl_easy_escape(escape,filename,0);
 int written=-1;
 if(encoded_file&&(raw||encoded_repo)){
  if(raw)written=snprintf(url,cap,
        "https://raw.githubusercontent.com/libretro-thumbnails/%s/master/%s/%s",
        repo,kind,encoded_file);
  else written=snprintf(url,cap,"https://thumbnails.libretro.com/%s/%s/%s",
                        encoded_repo,kind,encoded_file);
 }
 if(encoded_repo)curl_free(encoded_repo);
 if(encoded_file)curl_free(encoded_file);
 curl_easy_cleanup(escape);
 return written>0&&(size_t)written<cap;
}
static void finish_game(int success,const char *message){
 if(success){job.downloaded++;if(http_legacy)job.legacy_downloads++;cover_forget(&current_game);}
 else {job.failed++;if(current_index>=0&&current_index<MAIZO_MAX_GAMES)
    failed_indices[current_index]=1;}
 job.processed++;
 current_index=-1;stop_transfer();
 snprintf(job.message,sizeof(job.message),"%s",message);
 if(job.processed>=job.total){
  job.active=0;
  if(!success)snprintf(job.message,sizeof(job.message),
            job.tls_errors?"FALHAS SSL/CA: CONFIRA RELOGIO DO WII":
                           "CONCLUIDO COM CAPAS AUSENTES");
  else snprintf(job.message,sizeof(job.message),"DOWNLOAD CONCLUIDO");
 }
}
void maizo_cover_download_custom(const Library*library,unsigned mask,int update,
                                int provider,int art,int region,int mode,int index){
 memset(&job,0,sizeof(job));
 job.transport=transport_setting;
 selected_mask=mask&0x7fu;update_existing=!!update;
 chosen_provider=provider<0||provider>2?0:provider;
 chosen_art=art<0||art>2?0:art;
 chosen_region=region<0||region>4?0:region;
 chosen_mode=mode<0||mode>2?0:mode;
 chosen_index=index;cursor=0;current_index=-1;stop_transfer();
 unreachable_hosts=0;
 if(!network_ready){network_attempted=0;network_retry_frame=0;}
 wait_for_index=0;
 if(!library){
  snprintf(job.message,sizeof(job.message),"BIBLIOTECA INDISPONIVEL");
  return;
 }
 if(library->scanning){
  wait_for_index=1;job.active=1;
  snprintf(job.message,sizeof(job.message),"INDEXANDO JOGOS ANTES DAS CAPAS...");
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
   "PREPARANDO FONTES DE CAPAS...":"NENHUM JOGO CORRESPONDE A SELECAO");
}
void maizo_cover_download_begin_filtered(const Library*l,unsigned mask,int update){
 maizo_cover_download_custom(l,mask,update,0,0,0,0,-1);
}
void maizo_cover_download_begin(const Library*l){
 maizo_cover_download_begin_filtered(l,0x7fu,0);
}
void maizo_cover_download_cancel(void){
 stop_transfer();
 wait_for_index=0;
 current_index=-1;
 job.active=0;
 snprintf(job.message,sizeof(job.message),"INTERROMPIDO - CAPAS JA BAIXADAS PRESERVADAS");
}
MaizoCoverDownloadStatus maizo_cover_download_status(void){return job;}
void maizo_cover_download_step(const Library *library){
 if(!job.active||!library)return;
 if(wait_for_index){
  if(library->scanning)return;
  wait_for_index=0;cursor=0;job.total=0;
  for(int i=0;i<library->count;i++){
   unsigned sys=(unsigned)library->games[i].system;
   if(sys>=7u||!(selected_mask&(1u<<sys)))continue;
   if(chosen_mode==1&&i!=chosen_index)continue;
   if(chosen_mode==2&&!failed_indices[i])continue;
   job.total++;
  }
  job.active=job.total>0;
  snprintf(job.message,sizeof(job.message),job.active?
   "INDEXACAO FINALIZADA - CONECTANDO HTTPS":"NENHUM JOGO NA SELECAO");
  if(!job.active)return;
 }
 if(!network_ready){
  /* Priiloader cold boot may reach the menu before DHCP is ready.
   * Retry three times at ~2 second frame intervals. */
  if(network_retry_frame++%120u==0 && network_attempted<3){
   network_attempted++;
   if(wiisocket_init()==0 && curl_global_init(CURL_GLOBAL_DEFAULT)==CURLE_OK)
    network_ready=1;
  }
  if(!network_ready){
   if(network_attempted>=3 && network_retry_frame>=360u){
    maizo_cover_download_cancel();
    snprintf(job.message,sizeof(job.message),"REDE INDISPONIVEL: CONFIRA WI-FI / DNS");
   }else snprintf(job.message,sizeof(job.message),
                   "AGUARDANDO REDE DO WII (%d/3)",network_attempted);
   return;
  }
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
  if(valid){
   if(save_png_as_gxt(&received,current_dest,current_game.system)){
    finish_game(1,"CAPA VERIFICADA E SALVA");return;
   }
   finish_game(0,"FALHA AO GRAVAR CAPA NO USB/SD");return;
  }
  /* libcurl error 60 is a peer CA/hostname/time failure, NOT an SSH
   * issue. Secure transfers stay verified. USB Loader GX-like HTTP retry
   * is restricted to immutable GameTDB image endpoints and clearly marked. */
  unsigned host=strstr(requested_url,"://art.gametdb.com/")?1u:
                strstr(requested_url,"://thumbnails.libretro.com/")?2u:
                strstr(requested_url,"://raw.githubusercontent.com/")?4u:0u;
  if(result==CURLE_PEER_FAILED_VERIFICATION){
   job.tls_errors++;
   /* An expired leaf cert or invalid Wii system clock cannot be repaired
    * by switching trusted CA roots. Never skip certificate verification.
    * AUTO tries a DIFFERENT HTTPS provider, not slow HTTP redirects. */
   if(host)unreachable_hosts|=host;
   const time_t now=time(NULL);
   snprintf(last_transfer_error,sizeof(last_transfer_error),
      now<1704067200 || now>2051222400?
       "CURL 60: HORA/DATA DO WII INCORRETA":
       "CURL 60: CERTIFICADO EXPIRADO - HORA/CA/SERVIDOR");
  }else if(result==CURLE_OPERATION_TIMEDOUT && http==0){
   /* HTTP 0 = no HTTP headers arrived, not a 404. Fail over once per host
    * instead of retrying that unreachable server for every cover. */
   if(host)unreachable_hosts|=host;
   snprintf(last_transfer_error,sizeof(last_transfer_error),
            "CURL 28: TIMEOUT DE CONEXAO - FONTE ALTERNATIVA");
  }
  if(http_legacy&&http>=300&&http<400){
   /* GameTDB may redirect HTTP to HTTPS. Do not abort the entire batch;
    * continue to the next region/art and ultimately Libretro HTTPS. */
   snprintf(last_transfer_error,sizeof(last_transfer_error),
            "GAMETDB HTTP %ld REDIRECIONOU - TENTANDO LIBRETRO",http);
   stop_transfer();attempt++;return;
  }
  const char *why=mc!=CURLM_OK?"FALHA CURL MULTI":
                   curl_error[0]?curl_error:curl_easy_strerror(result);
  snprintf(job.message,sizeof(job.message),"%s %ld / CURL %d: %.42s",
           http_legacy?"HTTP GX":"HTTPS",http,(int)result,why);
  if(result!=CURLE_PEER_FAILED_VERIFICATION &&
     !(result==CURLE_OPERATION_TIMEDOUT&&http==0))
   snprintf(last_transfer_error,sizeof(last_transfer_error),
            "HTTP %ld / CURL %d: %.66s",http,(int)result,why);
  stop_transfer();
  attempt++;
  return;
 }
 if(current_index>=0){
  /* Skip providers that already failed TLS/connection in this batch:
   * no repeated 15s timeout for each region of each title. */
  for(unsigned skips=0;skips<40;skips++){
   char url[1024];
   if(!provider_url(&current_game,attempt,url,sizeof(url))){
    finish_game(0,last_transfer_error[0]?last_transfer_error:
                "SEM CAPA EM GAMETDB OU LIBRETRO");return;
   }
   unsigned host=strstr(url,"://art.gametdb.com/")?1u:
                 strstr(url,"://thumbnails.libretro.com/")?2u:4u;
   if(unreachable_hosts&host){attempt++;continue;}
   char plain[1024];
   /* Explicit GX HTTP mode only; AUTO stays on authenticated HTTPS.
    * An HTTP redirect or timeout cannot fix an expired TLS cert. */
   if(transport_setting==2 &&
      maizo_cover_gametdb_http(url,plain,sizeof(plain))){
    if(start_transfer(plain))return;
   }else if(start_transfer(url))return;
   attempt++;return;
  }
  finish_game(0,"SEM SERVIDOR DISPONIVEL - CONFIRA DATA/REDE");
  return;
 }
 if((unreachable_hosts&6u)==6u && !easy){
  /* All HTTPS mirrors unavailable. Do not mark hundreds of ROMs as
   * 'missing cover' when the Wii clock/TLS or its internet is broken. */
  job.active=0;
  snprintf(job.message,sizeof(job.message),
           "FONTE HTTPS INACESSIVEL: WII DATA/HORA, INTERNET OU CA");
  return;
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
 last_transfer_error[0]=0;
 failed_indices[current_index]=0;
 snprintf(job.current,sizeof(job.current),"%.76s [%s]",current_game.name,
          current_game.id[0]?current_game.id:
          current_game.product_code[0]?current_game.product_code:"SEM ID");
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
 snprintf(job.message,sizeof(job.message),"BUSCANDO GAMETDB / LIBRETRO");
}
