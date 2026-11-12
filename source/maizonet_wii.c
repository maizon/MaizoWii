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
  if(!maizonet_smb1_open(smb,conf.path)){
   snprintf(msg,sizeof(msg),"SMB1: ARQUIVO NAO ABRIU: %s",maizonet_smb1_error(smb->last_status));
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
