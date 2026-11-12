/* Host-only SMB1 TRANS2_FIND_FIRST2 regression. This synthetic server is
 * not a replacement for a real Samba/Wii interoperability test. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "maizonet_smb1.h"
typedef struct {unsigned pos,len,calls;int bad;uint8_t rx[4096];} Fake;
static void w16(uint8_t*p,unsigned x){p[0]=x;p[1]=x>>8;}
static void w32(uint8_t*p,uint32_t x){w16(p,x);w16(p+2,x>>16);}
static int tx(void*ctx,const void*packet,unsigned n){
 Fake*f=(Fake*)ctx;
 const uint8_t*p=(const uint8_t*)packet+4;
 uint8_t*r=f->rx+4;unsigned start=56,offset=66,entrylen=100,len=205;
 if(n<74||p[4]!=0x32||p[32]!=15||p[33+26]!=1||p[33+28]!=1)return -1;
 if(p[33+20]!=66||p[65]!=0||p[66]!=0x16||
    memcmp(p+78,"Games\\*",7)!=0)return -1;
 memset(f->rx,0,sizeof(f->rx));
 r[0]=0xff;r[1]='S';r[2]='M';r[3]='B';r[4]=0x32;
 w16(r+30,(unsigned)p[30]|((unsigned)p[31]<<8));r[32]=10;
 /* TRANS2 response: one parameter buffer + one data buffer. */
 w16(r+33,10);w16(r+35,len);
 w16(r+33+6,10);w16(r+33+8,start);
 w16(r+33+12,len);w16(r+33+14,f->bad?4500:offset);
 w16(r+53,1+10+len);
 w16(r+start,0x0042);w16(r+start+2,2);w16(r+start+4,1);
 /* FILE_BOTH_DIRECTORY_INFO record with directory flag. */
 w32(r+offset,entrylen);
 w32(r+offset+56,0x10);w32(r+offset+60,5);
 memcpy(r+offset+94,"Games",5);
 /* Final regular .iso */
 w32(r+offset+entrylen,0);
 w32(r+offset+entrylen+56,0x20);
 w32(r+offset+entrylen+60,8);
 memcpy(r+offset+entrylen+94,"game.iso",8);
 f->len=4+55+1+10+len;f->pos=0;
 f->rx[0]=0;f->rx[1]=(f->len-4)>>16;
 f->rx[2]=(f->len-4)>>8;f->rx[3]=f->len-4;
 f->calls++;
 return n;
}
static int rx(void*ctx,void*buf,unsigned n){
 Fake*f=(Fake*)ctx;unsigned k;
 if(f->pos>=f->len)return -1;
 k=f->len-f->pos;if(k>n)k=n;if(k>11)k=11;
 memcpy(buf,f->rx+f->pos,k);f->pos+=k;return k;
}
int main(void){
 Fake f={0};MaizoNetSmb1*c=calloc(1,sizeof(*c));
 MaizoNetTransport t={rx,tx,&f};
 MaizoNetDirEntry entries[3];int count;
 if(!c)return 1;
 c->io=t;c->tree=1;
 count=maizonet_smb1_list_dir(c,"Games",entries,3);
 if(count!=2||!entries[0].is_dir||strcmp(entries[0].name,"Games")||
    entries[1].is_dir||strcmp(entries[1].name,"game.iso")){
  fprintf(stderr,"bad listing: count=%d, calls=%u\n",count,f.calls);return 2;
 }
 f.bad=1;
 if(maizonet_smb1_list_dir(c,"Games",entries,3)!=-1)return 3;
 free(c);puts("PASS: SMB1 directory listing, bounded parse and offset rejection");
 return 0;
}
