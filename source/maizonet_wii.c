/* MaizoNet SMB1 read-only diagnostic for Wii libogc.
 * A successful connection is NOT proof of Nintendont game streaming.
 * Invoked only by explicit menu action to avoid network waits during UI.
 * User configuration: usb:/maizo/network.ini or sd:/maizo/network.ini:
 *   host=192.168.1.100
 *   port=1445
 *   share=PS2
 *   path=GameCube/Example/game.iso
 * This experimental guest SMB1 client must stay on a trusted LAN.
 */
#include "maizonet_wii.h"
#include "../integrations/maizonet_smb1.h"
#include <network.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
/* Self-contained network module: do not depend on app.h path constants. */
#define MAIZONET_PATH_CAP 512u
typedef struct {
 char host[64],share[64],path[256];
 unsigned port;
} NetConfig;
typedef struct {int fd;} NetSocket;
static int recv_some(void *ctx,void *dst,unsigned n){
 NetSocket*s=(NetSocket*)ctx;return net_recv(s->fd,dst,n,0);
}
static int send_some(void *ctx,const void *src,unsigned n){
 NetSocket*s=(NetSocket*)ctx;return net_send(s->fd,src,n,0);
}
static int ipv4(const char *host,struct in_addr*out){
 unsigned part=0,n=0,octets=0;uint32_t addr=0;
 if(!host||!host[0])return 0;
 for(const char*c=host;;c++){
  if(*c>='0'&&*c<='9'){
   if(n>=3)return 0;
   part=part*10u+(unsigned)(*c-'0');n++;
   if(part>255)return 0;
  }else if(*c=='.'||*c==0){
   if(!n)return 0;
   addr=(addr<<8)|part;octets++;
   if(!*c)break;
   if(octets>=4)return 0;
   part=0;n=0;
  }else return 0;
 }
 if(octets!=4)return 0;
 /* Assign as network bytes, independent of Wii host endianness. */
 unsigned char *p=(unsigned char*)&out->s_addr;
 p[0]=(unsigned char)(addr>>24);p[1]=(unsigned char)(addr>>16);
 p[2]=(unsigned char)(addr>>8);p[3]=(unsigned char)addr;
 return 1;
}
static void read_ini(NetConfig*c){
 static const char *files[]={"usb:/maizo/network.ini","sd:/maizo/network.ini"};
 FILE*f=0;char line[400];
 memset(c,0,sizeof(*c));c->port=445;
 for(unsigned k=0;k<2&&!f;k++)f=fopen(files[k],"rb");
 if(!f)return;
 while(fgets(line,sizeof(line),f)){
  char *e=strchr(line,'=');if(!e)continue;
  *e++=0;e[strcspn(e,"\r\n")]=0;
  if(!strcmp(line,"host"))snprintf(c->host,sizeof(c->host),"%s",e);
  else if(!strcmp(line,"share"))snprintf(c->share,sizeof(c->share),"%s",e);
  else if(!strcmp(line,"path"))snprintf(c->path,sizeof(c->path),"%s",e);
  else if(!strcmp(line,"port")){
   char *end=0;unsigned long v=strtoul(e,&end,10);
   if(end&&!*end&&v>=1&&v<=65535)c->port=(unsigned)v;
  }
 }
 fclose(f);
}
void maizonet_wii_probe(char*result,size_t cap){
 NetConfig conf;NetSocket sock={-1};
 struct sockaddr_in addr;
 MaizoNetSmb1 *smb=NULL;
 MaizoNetTransport transport;
 char msg[112]="SMB1: ERRO DESCONHECIDO";
 unsigned char header[32]={0};int r,established=0;
 if(!result||!cap)return;
 read_ini(&conf);
 if(!conf.host[0]||!conf.share[0]){
  snprintf(result,cap,"CRIE /maizo/network.ini COM host, port E share");
  return;
 }
 memset(&addr,0,sizeof(addr));
 if(!ipv4(conf.host,&addr.sin_addr)){
  snprintf(result,cap,"HOST SMB1: IP INVALIDO");return;
 }
 if(net_init()<0){snprintf(result,cap,"WII: REDE NAO INICIALIZADA");return;}
 sock.fd=net_socket(AF_INET,SOCK_STREAM,0);
 if(sock.fd<0){snprintf(result,cap,"WII: SOCKET INDISPONIVEL");return;}
 addr.sin_family=AF_INET;addr.sin_port=htons((unsigned short)conf.port);
 /* Never allow a remote server to stall the UI indefinitely. */
 struct timeval tv;tv.tv_sec=3;tv.tv_usec=0;
 net_setsockopt(sock.fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
 net_setsockopt(sock.fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));
 if(net_connect(sock.fd,(struct sockaddr*)&addr,sizeof(addr))<0){
  snprintf(msg,sizeof(msg),"SMB1 %s:%u: SEM CONEXAO",conf.host,conf.port);
  goto done;
 }
 smb=(MaizoNetSmb1*)calloc(1,sizeof(*smb));
 if(!smb){snprintf(msg,sizeof(msg),"SEM MEMORIA SMB1");goto done;}
 transport.receive=recv_some;transport.transmit=send_some;transport.user=&sock;
 if(!maizonet_smb1_connect_guest(smb,transport,conf.host,conf.share)){
  snprintf(msg,sizeof(msg),"SMB1 FALHOU: %s",maizonet_smb1_error(smb->last_status));
  goto done;
 }
 established=1;
 snprintf(msg,sizeof(msg),"SMB1: CONECTADO EM %s (%u)",conf.share,conf.port);
 if(conf.path[0]){
  /* SMB1 filenames use backslashes; network.ini is user-facing and accepts
   * slash-separated paths. Match the launcher normalization in probe mode. */
  char canonical_path[sizeof(conf.path)];
  snprintf(canonical_path,sizeof(canonical_path),"%s",conf.path);
  for(char *p=canonical_path;*p;p++)if(*p=='/')*p='\\';
  if(!maizonet_smb1_open(smb,canonical_path)){
   snprintf(msg,sizeof(msg),"SMB1: ARQUIVO NAO ABRIU: %s (NT=%08lX)",maizonet_smb1_error(smb->last_status),(unsigned long)smb->last_status);
   goto done;
  }
  r=maizonet_smb1_read_at(smb,0,header,sizeof(header));
  if(r!=(int)sizeof(header)){
   snprintf(msg,sizeof(msg),"SMB1: LEITURA DE SETOR FALHOU");goto done;
  }
  snprintf(msg,sizeof(msg),"SMB1: ISO LIDA - ID %02X%02X%02X%02X",
    header[0],header[1],header[2],header[3]);
 }
done:
 if(smb){
  if(established)maizonet_smb1_close(smb);
  free(smb);
 }
 net_close(sock.fd);
 snprintf(result,cap,"%s",msg);
}


/* Marker passed ONLY in Nintendont's unused CheatPath field when launching a
 * remote ISO, never during local GameCube gameplay. Every GameCube launch
 * clears the marker, including error paths. This is transient process state. */
static char active_marker[255];
void maizonet_wii_set_marker(const char*marker){
 if(marker)snprintf(active_marker,sizeof(active_marker),"%s",marker);
 else active_marker[0]=0;
}
const char *maizonet_wii_get_marker(void){return active_marker;}
static int iso_id_valid(const unsigned char *p){
 if(p[0x1c]!=0xc2||p[0x1d]!=0x33||p[0x1e]!=0x9f||p[0x1f]!=0x3d)return 0;
 for(int i=0;i<6;i++){
  if(!((p[i]>='A'&&p[i]<='Z')||(p[i]>='0'&&p[i]<='9')))return 0;
 }
 return 1;
}
int maizonet_wii_prepare(char*stub,size_t stubcap,char*marker,size_t markcap,
                         char id6[7],char*error,size_t errorcap){
 NetConfig conf;NetSocket sock={-1};struct sockaddr_in addr;
 MaizoNetSmb1*smb=NULL;MaizoNetTransport t;
 unsigned char *first=NULL;FILE*f=NULL;int ok=0;
 char path[256],temp[MAIZONET_PATH_CAP];
 if(!error||!errorcap||!stub||!stubcap||!marker||!markcap||!id6)return 0;
 error[0]=0;marker[0]=0;stub[0]=0;id6[0]=0;
 read_ini(&conf);
 if(!conf.host[0]||!conf.share[0]||!conf.path[0]){
  snprintf(error,errorcap,"CONFIGURE host,port,share,path EM maizo/network.ini");
  return 0;
 }
 if(strlen(conf.path)>=sizeof(path)){
  snprintf(error,errorcap,"CAMINHO SMB MUITO LONGO");return 0;
 }
 snprintf(path,sizeof(path),"%s",conf.path);
 for(char*p=path;*p;p++)if(*p=='/')*p='\\';
 memset(&addr,0,sizeof(addr));
 if(!ipv4(conf.host,&addr.sin_addr)){
  snprintf(error,errorcap,"IP DO SERVIDOR SMB INVALIDO");return 0;
 }
 int n=snprintf(marker,markcap,"MZSMB1|%s|%u|%s|%s",
      conf.host,conf.port,conf.share,path);
 if(n<0||(size_t)n>=markcap||n>=255){
  marker[0]=0;snprintf(error,errorcap,"MARCADOR DE REDE EXCEDE 254 BYTES");return 0;
 }
 const char *root=NULL;
 DIR*d=opendir("sd:/");if(d){closedir(d);root="sd:";}
 if(!root){d=opendir("usb:/");if(d){closedir(d);root="usb:";}}
 if(!root){snprintf(error,errorcap,"PRECISA DE SD/USB PARA DOL E STUB DE 128KB");return 0;}
 snprintf(stub,stubcap,"%s/maizo/maizonet/game.iso",root);
 snprintf(temp,sizeof(temp),"%s/maizo/maizonet/game.tmp",root);
 if(net_init()<0){snprintf(error,errorcap,"REDE WII NAO DISPONIVEL");return 0;}
 sock.fd=net_socket(AF_INET,SOCK_STREAM,0);
 if(sock.fd<0){snprintf(error,errorcap,"SEM SOCKET TCP NO WII");return 0;}
 struct timeval tv={3,0};
 net_setsockopt(sock.fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
 net_setsockopt(sock.fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));
 addr.sin_family=AF_INET;addr.sin_port=htons((unsigned short)conf.port);
 if(net_connect(sock.fd,(struct sockaddr*)&addr,sizeof(addr))<0){
  snprintf(error,errorcap,"SEM CONEXAO COM SERVIDOR SMB1");goto done;
 }
 smb=calloc(1,sizeof(*smb));first=malloc(131072u);
 if(!smb||!first){snprintf(error,errorcap,"MEMORIA INSUFICIENTE");goto done;}
 t.receive=recv_some;t.transmit=send_some;t.user=&sock;
 if(!maizonet_smb1_connect_guest(smb,t,conf.host,conf.share)){
  snprintf(error,errorcap,"SMB1: %s",maizonet_smb1_error(smb->last_status));goto done;
 }
 if(!maizonet_smb1_open(smb,path)){
  snprintf(error,errorcap,"ISO SMB: %s (NT=%08lX)",maizonet_smb1_error(smb->last_status),(unsigned long)smb->last_status);goto done;
 }
 if(maizonet_smb1_read_at(smb,0,first,131072u)!=131072){
  snprintf(error,errorcap,"FALHA AO LER 128KB DO GAMECUBE PELA REDE");goto done;
 }
 if(!iso_id_valid(first)){
  snprintf(error,errorcap,"ARQUIVO NAO TEM CABECALHO GAMECUBE VALIDO");goto done;
 }
 memcpy(id6,first,6);id6[6]=0;
 char folder[MAIZONET_PATH_CAP];
 snprintf(folder,sizeof(folder),"%s/maizo",root);mkdir(folder,0777);
 snprintf(folder,sizeof(folder),"%s/maizo/maizonet",root);mkdir(folder,0777);
 f=fopen(temp,"wb");
 if(!f){snprintf(error,errorcap,"NAO CONSEGUE GRAVAR STUB 128KB");goto done;}
 if(fwrite(first,1,131072u,f)!=131072u){
  snprintf(error,errorcap,"FALHA GRAVANDO CABECALHO GC");goto done;
 }
 if(fclose(f)!=0){f=NULL;snprintf(error,errorcap,"ERRO SALVANDO CABECALHO GC");goto done;}
 f=NULL;
 /* Reuse only this dedicated generated path; never overwrite actual ISOs. */
 remove(stub);
 if(rename(temp,stub)!=0){
  snprintf(error,errorcap,"NAO CONSEGUE ATIVAR STUB GC");goto done;
 }
 ok=1;
 /* Record frontend success independently from the experimental ARM kernel. */
 {
  char diag[MAIZONET_PATH_CAP];
  snprintf(diag,sizeof(diag),"%s/maizo/maizonet/diagnostic.log",root);
  FILE *lf=fopen(diag,"ab");
  if(lf){
   fprintf(lf,"MZNET_FRONTEND remote_iso_header_128kb=OK id=%s port=%u share=%s\n",
           id6,conf.port,conf.share);
   fclose(lf);
  }
 }
done:
 if(f)fclose(f);
 if(!ok){remove(temp);marker[0]=0;}
 if(smb){if(smb->session)maizonet_smb1_close(smb);free(smb);}
 free(first);
 net_close(sock.fd);
 return ok;
}
