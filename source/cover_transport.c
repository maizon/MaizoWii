/* GameTDB cover-only HTTP compatibility, inspired by USB Loader GX.
 * Do NOT use this for executable code, metadata, credentials or arbitrary URLs.
 * HTTPS certificate checks remain enabled on every TLS connection.
 */
#include "cover_transport.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
static int art_kind(const char *name,size_t n){
 static const char *const kinds[]={"cover","cover3D","coverfull","coverfullHQ","disc"};
 for(unsigned i=0;i<sizeof(kinds)/sizeof(kinds[0]);i++)
  if(strlen(kinds[i])==n&&!strncmp(name,kinds[i],n))return 1;
 return 0;
}
int maizo_cover_gametdb_http(const char *secure,char *out,size_t cap){
 static const char prefix[]="https://art.gametdb.com/";
 if(!secure||!out||cap<64||strncmp(secure,prefix,sizeof(prefix)-1))return 0;
 const char *s=secure+sizeof(prefix)-1;
 if(!strncmp(s,"wii/",4))s+=4;
 else if(!strncmp(s,"gc/",3))s+=3;
 else return 0;
 const char *slash=strchr(s,'/');
 if(!slash||!art_kind(s,(size_t)(slash-s)))return 0;
 const char *region=slash+1;
 /* Check complete remaining string before indexing game ID or region. */
 if(strlen(region)!=2+1+6+4)return 0;
 if(!isupper((unsigned char)region[0])||!isupper((unsigned char)region[1])||
    region[2]!='/')return 0;
 const char *id=region+3;
 for(int i=0;i<6;i++)if(!((id[i]>='A'&&id[i]<='Z')||
                             (id[i]>='0'&&id[i]<='9')))return 0;
 if(strcmp(id+6,".png"))return 0;
 int n=snprintf(out,cap,"http://%s",secure+8);
 return n>0&&(size_t)n<cap;
}
int maizo_cover_allow_legacy_retry(unsigned mode,int curl_error,const char *url){
 char result[280];
 /* 60 = CURLE_PEER_FAILED_VERIFICATION. Automatic mode never downgrades
  * to insecure HTTP. Compatibility is opt-in and cover-only. */
 return mode==2&&curl_error==60&&
        maizo_cover_gametdb_http(url,result,sizeof(result));
}
