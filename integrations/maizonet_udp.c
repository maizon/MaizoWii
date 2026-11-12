/* Portable MaizoNet UDP block transport. See maizonet_udp.h. */
#include "maizonet_udp.h"
#include <string.h>
static void w16(uint8_t*p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void w32(uint8_t*p,uint32_t v){w16(p,(uint16_t)(v>>16));w16(p+2,(uint16_t)v);}
static void w64(uint8_t*p,uint64_t v){w32(p,(uint32_t)(v>>32));w32(p+4,(uint32_t)v);}
static uint16_t r16(const uint8_t*p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static uint32_t r32(const uint8_t*p){return ((uint32_t)r16(p)<<16)|r16(p+2);}
static uint64_t r64(const uint8_t*p){return ((uint64_t)r32(p)<<32)|r32(p+4);}
static uint32_t crc32(const uint8_t*p,unsigned n){
 uint32_t c=0xffffffffu;unsigned i,j;
 for(i=0;i<n;i++){c^=p[i];for(j=0;j<8;j++)c=(c>>1)^(0xedb88320u&-(c&1u));}
 return c^0xffffffffu;
}
static unsigned request(MaizoUdp*c,uint8_t op,uint64_t off,unsigned len,const char*path){
 unsigned n=MZUDP_REQ_H;
 memset(c->tx,0,sizeof(c->tx));
 memcpy(c->tx,"MZUQ",4);c->tx[4]=1;c->tx[5]=op;
 w32(c->tx+8,++c->request);w32(c->tx+12,c->token);
 w64(c->tx+16,off);w16(c->tx+24,(uint16_t)len);
 if(path){unsigned m=(unsigned)strlen(path);memcpy(c->tx+n,path,m);n+=m;}
 return n;
}
/* Return 1 for well-formed reply to the current request, 0 for unrelated
 * packet/timeout. UDP datagrams MUST be received atomically. */
static int response(MaizoUdp*c,unsigned timeout){
 int n=c->io.receive(c->io.user,c->rx,sizeof(c->rx),timeout);
 if(n<(int)MZUDP_RES_H||n>(int)sizeof(c->rx))return 0;
 if(memcmp(c->rx,"MZUR",4)||c->rx[4]!=1||
    r32(c->rx+8)!=c->request)return 0;
 if((unsigned)n!=MZUDP_RES_H+r16(c->rx+28))return 0;
 return 1;
}
int maizonet_udp_open(MaizoUdp*c,MaizoUdpTransport t,const char*path){
 unsigned attempts,i,n;
 if(!c||!t.transmit||!t.receive||!path||!path[0])return 0;
 n=(unsigned)strlen(path);
 if(n>255||strstr(path,"..")||path[0]=='/'||path[0]=='\\')return 0;
 for(i=0;i<n;i++){unsigned char k=(unsigned char)path[i];if(k<32||k>126)return 0;}
 memset(c,0,sizeof(*c));c->io=t;
 n=request(c,1,0,0,path);
 for(attempts=0;attempts<7;attempts++){
  if(c->io.transmit(t.user,c->tx,n)!=(int)n)continue;
  if(!response(c,220))continue;
  if(c->rx[5]!=1)continue;
  if(c->rx[6]){c->error=c->rx[6];return 0;}
  c->token=r32(c->rx+12);c->size=r64(c->rx+36);
  if(!c->token||c->size<0x20000ULL){c->error=10;return 0;}
  c->opened=1;return 1;
 }
 c->error=11;return 0;
}
static int block(MaizoUdp*c,uint64_t offset,uint8_t*out,unsigned n){
 uint8_t seen[MZUDP_MAX_FRAG];unsigned got=0,attempts,fragments,i,p,amount,nreq;
 uint32_t crc;uint16_t count,index,off,bytes;
 if(!n||n>MZUDP_BLOCK)return 0;
 fragments=(n+MZUDP_PAYLOAD-1)/MZUDP_PAYLOAD;
 memset(seen,0,sizeof(seen));
 nreq=request(c,2,offset,n,NULL);
 for(attempts=0;attempts<7;attempts++){
  if(c->io.transmit(c->io.user,c->tx,nreq)!=(int)nreq)continue;
  /* At most 2 timeouts per attempt. Prevent an endless stream of invalid
   * datagrams from blocking the Wii DI thread. */
  for(i=0;i<fragments+2;i++){
   if(!response(c,180))break;
   if(c->rx[5]!=2||r32(c->rx+12)!=c->token||r64(c->rx+16)!=offset)continue;
   if(c->rx[6]){c->error=c->rx[6];return 0;}
   index=r16(c->rx+24);count=r16(c->rx+26);
   bytes=r16(c->rx+28);off=r16(c->rx+30);
   if(index>=fragments||count!=fragments||
      off!=(unsigned)index*MZUDP_PAYLOAD||off+bytes>n||
      !bytes||bytes> MZUDP_PAYLOAD||
      bytes!= (index+1==fragments?n-off:MZUDP_PAYLOAD))continue;
   crc=r32(c->rx+32);
   if(crc32(c->rx+MZUDP_RES_H,bytes)!=crc)continue;
   if(!seen[index]){
    memcpy(out+off,c->rx+MZUDP_RES_H,bytes);
    seen[index]=1;got++;
    if(got==fragments)return 1;
   }
  }
 }
 c->error=12;return 0;
}
int maizonet_udp_read(MaizoUdp*c,uint64_t off,void*buffer,unsigned n){
 unsigned done=0,part;uint8_t*out=(uint8_t*)buffer;
 if(!c||!c->opened||!buffer||!n||off>c->size||(uint64_t)n>c->size-off)return -1;
 while(done<n){
  part=n-done;
  if(part>MZUDP_BLOCK)part=MZUDP_BLOCK;
  if(!block(c,off+done,out+done,part))return -1;
  done+=part;
 }
 return (int)done;
}
void maizonet_udp_close(MaizoUdp*c){
 unsigned n;
 if(!c||!c->opened)return;
 n=request(c,3,0,0,NULL);
 c->io.transmit(c->io.user,c->tx,n);
 c->opened=0;c->token=0;
}
