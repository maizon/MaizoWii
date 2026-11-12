/* Real localhost Samba NT1 interoperability smoke test (CI).
 * No Wii or gameplay claims; checks guest login, TRANS2 listing, OPEN_ANDX,
 * and READ_ANDX at the offset that is beyond the local 128KiB boot stub.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "maizonet_smb1.h"
typedef struct {int fd;} Net;
static int rd(void *v,void*p,unsigned n){return recv(((Net*)v)->fd,p,n,0);}
static int wr(void *v,const void*p,unsigned n){return send(((Net*)v)->fd,p,n,0);}
int main(int argc,char**argv){
 struct sockaddr_in addr;MaizoNetSmb1*c;MaizoNetTransport io;
 MaizoNetDirEntry files[16];Net n;int count,i,r;uint8_t buf[64];
 if(argc!=2)return 1;
 n.fd=socket(AF_INET,SOCK_STREAM,0);
 if(n.fd<0)return 2;
 memset(&addr,0,sizeof(addr));addr.sin_family=AF_INET;
 addr.sin_port=htons(1445);addr.sin_addr.s_addr=inet_addr("127.0.0.1");
 if(connect(n.fd,(struct sockaddr*)&addr,sizeof(addr))<0){perror("connect");return 3;}
 c=calloc(1,sizeof(*c));if(!c)return 4;
 io.receive=rd;io.transmit=wr;io.user=&n;
 if(!maizonet_smb1_connect_guest(c,io,"127.0.0.1","PS2")){
  fprintf(stderr,"SMB guest failed: status %08x\n",c->last_status);return 5;
 }
 count=maizonet_smb1_list_dir(c,"",files,16);
 fprintf(stderr,"ROOT LIST=%d nt=%08x\\n",count,c->last_status);
 for(i=0;i<count;i++)fprintf(stderr,"  ROOT %s (dir=%u)\\n",files[i].name,(unsigned)files[i].is_dir);
 count=maizonet_smb1_list_dir(c,"Games",files,16);
 fprintf(stderr,"GAMES LIST=%d nt=%08x\\n",count,c->last_status);
 { unsigned dataoff=(unsigned)c->rx[4+47]|((unsigned)c->rx[4+48]<<8);
   unsigned datalen=(unsigned)c->rx[4+45]|((unsigned)c->rx[4+46]<<8);
   unsigned pos=0;
   fprintf(stderr,"OFF=%u LEN=%u\\n",dataoff,datalen);
   for(unsigned ix=0;ix<4&&pos<datalen;ix++){
    const uint8_t *p=c->rx+4+dataoff+pos;
    unsigned off=p[0]|((unsigned)p[1]<<8);
    unsigned nlen=p[60]|((unsigned)p[61]<<8);
    fprintf(stderr,"NAME at %u next=%u len=%u bytes=",pos,off,nlen);
    for(unsigned k=0;k<nlen&&k<20;k++)fprintf(stderr," %02x",p[94+k]);
    fprintf(stderr,"\\n");
    if(!off)break;
    pos+=off;
   }
 }

 for(i=0;i<count;i++)fprintf(stderr,"  GAME %s (dir=%u)\\n",files[i].name,(unsigned)files[i].is_dir);
 {unsigned k;
  for(k=0;k<c->rxlen && k<390;k++){if(k%16==0)fprintf(stderr,"\\n%03u:",k);
    fprintf(stderr," %02x",c->rx[k+4]);}
  fprintf(stderr,"\\n");
 }

 if(count<0){fprintf(stderr,"SMB list failed %08x\n",c->last_status);return 6;}
 for(i=0;i<count;i++)if(!strcmp(files[i].name,"game.iso"))break;
 if(i==count){fprintf(stderr,"No game.iso in %d entries\n",count);return 7;}
 if(!maizonet_smb1_open(c,"Games\\game.iso")){
  fprintf(stderr,"SMB open failed: %08x\n",c->last_status);return 8;
 }
 memset(buf,0,sizeof(buf));
 r=maizonet_smb1_read_at(c,0x20000,buf,sizeof(buf));
 if(r!=sizeof(buf)||memcmp(buf,argv[1],strlen(argv[1]))){
  fprintf(stderr,"SMB sector failed: result=%d status %08x\n",r,c->last_status);return 9;
 }
 maizonet_smb1_close(c);free(c);close(n.fd);
 puts("PASS: real Samba guest/list/open/random-sector SMB1 read");
 return 0;
}
