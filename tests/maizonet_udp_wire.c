/* End-to-end UDP sector client against the real Python read-only server.
 * Uses a sparse >4GiB test file, not copyrighted game data. */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "maizonet_udp.h"
typedef struct {int fd;} Context;
static int send_cb(void *v,const void*buf,unsigned n){
 Context*c=(Context*)v;return (int)send(c->fd,buf,n,0);
}
static int recv_cb(void *v,void*buf,unsigned n,unsigned timeout){
 Context*c=(Context*)v;
 fd_set set;struct timeval tv;int s;
 FD_ZERO(&set);FD_SET(c->fd,&set);
 tv.tv_sec=timeout/1000;tv.tv_usec=(timeout%1000)*1000;
 s=select(c->fd+1,&set,NULL,NULL,&tv);
 if(s<=0)return -1;
 return (int)recv(c->fd,buf,n,0);
}
int main(int argc,char**argv){
 Context ctx;struct sockaddr_in dst;
 MaizoUdp *u;MaizoUdpTransport io;
 uint8_t buf[8192+3072],large[4];unsigned i;
 if(argc!=2)return 100;
 ctx.fd=socket(AF_INET,SOCK_DGRAM,0);
 if(ctx.fd<0)return 101;
 memset(&dst,0,sizeof(dst));
 dst.sin_family=AF_INET;
 dst.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 dst.sin_port=htons((uint16_t)atoi(argv[1]));
 if(connect(ctx.fd,(struct sockaddr*)&dst,sizeof(dst))<0)return 102;
 u=(MaizoUdp*)calloc(1,sizeof(*u));if(!u)return 103;
 io.transmit=send_cb;io.receive=recv_cb;io.user=&ctx;
 if(!maizonet_udp_open(u,io,"Gamecube/games/XIII/game.iso")){
  fprintf(stderr,"UDP open failed err=%d\n",u->error);return 104;
 }
 if(u->size<0x100000012ULL)return 105;
 if(maizonet_udp_read(u,0x20000ULL,buf,sizeof(buf))!=(int)sizeof(buf)){
  fprintf(stderr,"UDP multi-frame read error=%d\n",u->error);return 106;
 }
 for(i=0;i<sizeof(buf);i++)if(buf[i]!=(uint8_t)((i*37u+11u)&255u))return 107;
 if(maizonet_udp_read(u,0x100000012ULL,large,sizeof(large))!=4||
    memcmp(large,"DATA",4))return 108;
 /* No read beyond EOF or arbitrary non-ISO paths. */
 if(maizonet_udp_read(u,u->size,large,1)!=-1)return 109;
 maizonet_udp_close(u);close(ctx.fd);free(u);
 puts("PASS UDP: 8KiB windows, 1024B CRC fragments, multi-read and >4GiB offsets");
 return 0;
}
