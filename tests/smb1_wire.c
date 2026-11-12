/* Synthetic SMB1 guest server (no network, no Wii hardware required).
 * Verifies framing, word offsets, MID tracking and 64-bit random-access reads.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "maizonet_smb1.h"
typedef struct {
 unsigned step, pos, size;
 uint8_t rx[40000];
 unsigned reqs, good_offsets, fail_mid;
} Fake;
static void w16(uint8_t*p,unsigned n){p[0]=n;p[1]=n>>8;}
static void w32(uint8_t*p,uint32_t n){w16(p,n);w16(p+2,n>>16);}
static int transmit(void*ctx,const void*ptr,unsigned n){
 Fake*f=ctx;const uint8_t*q=ptr,*p=q+4;uint8_t*out=f->rx+4;
 unsigned bytes=0,wn=0;
 uint8_t cmd=p[4];
 if(q[0]!=0||p[0]!=0xff||p[1]!='S'||p[2]!='M'||p[3]!='B'||
   n!=4u+((unsigned)q[1]<<16)+((unsigned)q[2]<<8)+q[3])return -1;
 if((f->step==0&&cmd!=0x72)||(f->step==1&&cmd!=0x73)||
    (f->step==2&&cmd!=0x75)||(f->step==3&&cmd!=0x2d)||
    (f->step>=4&&f->step<=6&&cmd!=0x2e)|| (f->step==7&&cmd!=0x04))return -1;
 if(cmd==0x72){
  if(p[32]!=0||p[33]!=12||p[35]!=2||
     memcmp(p+36,"NT LM 0.12",11))return -1;
  wn=17;w32(out+33+7,32768);w32(out+33+15,0xabcdef12);
 }else if(cmd==0x73){
  if(p[32]!=13||p[33]!=0xff||p[34]!=0||p[59]!=15||
   p[33+22]!=8||
   p[60]!=0)return -1;
  wn=3;
 }else if(cmd==0x75){
  if(p[32]!=4||p[33]!=0xff||p[39]!=1)return -1;
  wn=3;
 }else if(cmd==0x2d){
  const char *name="GameCube\\Sample\\game.iso";
  unsigned path_len=(unsigned)strlen(name)+1u;
  if(p[32]!=15||p[33]!=0xff||p[33+16]!=1||
     ((unsigned)p[63]|((unsigned)p[64]<<8))!=path_len||
     n<65u+path_len||memcmp(p+65,name,path_len)!=0)return -1;
  wn=15;w16(out+33+4,0x33);
 }else if(cmd==0x2e){
  unsigned requested=(unsigned)p[33+10]|((unsigned)p[33+11]<<8);
  unsigned expected=f->step==4?4u:(f->step==5?8192u:3072u);
  uint64_t reqoff=(uint64_t)p[33+6]|((uint64_t)p[33+7]<<8)|
       ((uint64_t)p[33+8]<<16)|((uint64_t)p[33+9]<<24)|
       ((uint64_t)p[33+20]<<32);
  uint64_t off=f->step==4?0x100000012ULL:
       (f->step==5?0x20000ULL:0x22000ULL);
  /* OPL-style 8-KiB ReadAndX, 64-bit offsets, no MinCount. */
  if(p[32]!=12||p[33]!=0xff||p[33+4]!=0x33||
     requested!=expected||p[33+12]!=0||p[33+13]!=0||
     reqoff!=off)return -1;
  wn=12;bytes=expected+1;
  f->good_offsets++;
 }else{
  wn=0;
 }
 /* Construct SMB frame; preserve command, request MID, and response IDs. */
 memset(out,0,33+wn*2+2+bytes);
 out[0]=0xff;out[1]='S';out[2]='M';out[3]='B';out[4]=cmd;
 out[9]=0x98;w16(out+10,0x4001);
 w16(out+24,cmd>=0x75?9:0);
 w16(out+28,cmd>=0x73?7:0);
 w16(out+30,(unsigned)p[30]|((unsigned)p[31]<<8));
 out[32]=wn;
 if(cmd==0x72){w32(out+33+7,32768);w32(out+33+15,0xabcdef12);}
 if(cmd==0x2d)w16(out+33+4,0x33);
 if(cmd==0x2e){
  unsigned count=f->step==4?4u:(f->step==5?8192u:3072u);
  w16(out+33+10,count);w16(out+33+12,60);
  out[59]=0;memset(out+60,'Q',count);
  if(f->step==4)memcpy(out+60,"DATA",4);
 }
 w16(out+33+wn*2,bytes);
 f->size=4+33+2*wn+2+bytes;
 f->rx[0]=0;f->rx[1]=(f->size-4)>>16;f->rx[2]=(f->size-4)>>8;
 f->rx[3]=f->size-4;
 f->pos=0;f->step++;f->reqs++;
 return (int)n;
}
static int receive(void*ctx,void*ptr,unsigned n){
 Fake*f=ctx;unsigned k;
 if(f->pos>=f->size)return -1;
 k=f->size-f->pos;if(k>n)k=n;if(k>7)k=7; /* fragmented TCP recv */
 memcpy(ptr,f->rx+f->pos,k);f->pos+=k;
 return (int)k;
}
int main(void){
 Fake fake={0};MaizoNetSmb1*c=calloc(1,sizeof(*c));
 MaizoNetTransport io={receive,transmit,&fake};
 char data[4]={0};
 char *large=malloc(8192u+3072u);
 if(!c||!large)return 1;
 if(!maizonet_smb1_connect_guest(c,io,"192.168.0.50","PS2")){
  fprintf(stderr,"guest connect failed step %u status %08x\n",fake.step,c->last_status);return 2;
 }
 if(!maizonet_smb1_open(c,"GameCube\\Sample\\game.iso"))return 3;
 if(maizonet_smb1_read_at(c,0x100000012ULL,data,4)!=4||
   memcmp(data,"DATA",4)!=0)return 4;
 if(c->max_chunk!=8192u)return 5;
 if(maizonet_smb1_read_at(c,0x20000ULL,large,8192u+3072u)!=8192+3072)
  return 6;
 for(unsigned i=0;i<8192u+3072u;i++)if(large[i]!='Q')return 7;
 maizonet_smb1_close(c);
 if(fake.step!=8||fake.good_offsets!=3)return 8;
 free(large);free(c);
 puts("PASS: OPL-style SMB1 8KiB chunks, TCP fragmentation and 64-bit reads");
 return 0;
}
