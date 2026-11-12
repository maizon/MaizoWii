/* MaizoNet: minimal read-only SMB1 block reader.
 * Uses only a caller-supplied bounded stream transport, to permit both
 * desktop protocol tests and future Wii/ARM backends without socket ABI
 * dependencies. Samba NT1 public shares only; never open SMB to WAN.
 * SMB1 is unauthenticated/insecure. Never use for sensitive files.
 */
#include "maizonet_smb1.h"
#include <string.h>
#define H 32u
#define NB 4u
static void w16(uint8_t*p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void w32(uint8_t*p,uint32_t n){w16(p,n);w16(p+2,n>>16);}
static unsigned r16(const uint8_t*p){return (unsigned)p[0]|((unsigned)p[1]<<8);}
static uint32_t r32(const uint8_t*p){return (uint32_t)r16(p)|((uint32_t)r16(p+2)<<16);}
static int strfit(const char*s,unsigned cap){
 unsigned i;
 if(!s||!s[0])return 0;
 for(i=0;s[i];i++){
  if(i>=cap)return 0;
  if((unsigned char)s[i]<32u ||(unsigned char)s[i]>126u)return 0;
 }
 return 1;
}
static int exact_read(MaizoNetSmb1*c,uint8_t*p,unsigned n){
 while(n){
  int r=c->io.receive(c->io.user,p,n);
  if(r<=0 ||(unsigned)r>n)return 0;
  p+=(unsigned)r;n-=(unsigned)r;
 }
 return 1;
}
static int exact_write(MaizoNetSmb1*c,const uint8_t*p,unsigned n){
 while(n){
  int r=c->io.transmit(c->io.user,p,n);
  if(r<=0 ||(unsigned)r>n)return 0;
  p+=(unsigned)r;n-=(unsigned)r;
 }
 return 1;
}
/* Build one request, require matching SMB1 command/MID and NTSTATUS 0.
 * Return -1 for IO/format failure, 0 for SMB response with NT error,
 * 1 for success. NTSTATUS is retained even on errors.
 */
static int exchange(MaizoNetSmb1*c,uint8_t command,const uint8_t*words,
                    unsigned wn,const uint8_t*data,unsigned dn){
 uint8_t *p=c->tx+NB;unsigned n;uint16_t mid;
 if(!c->io.receive||!c->io.transmit||wn&1u||wn/2>255u||
    NB+H+1+wn+2+dn>sizeof(c->tx))return -1;
 memset(p,0,H);
 p[0]=0xff;p[1]='S';p[2]='M';p[3]='B';p[4]=command;
 p[9]=0x18;w16(p+10,0x4001); /* long names, NTSTATUS */
 w16(p+24,c->tid);w16(p+26,0x4d5a);w16(p+28,c->uid);
 mid=++c->mid;w16(p+30,mid);
 n=H;p[n++]=(uint8_t)(wn/2);
 if(wn){memcpy(p+n,words,wn);n+=wn;}
 w16(p+n,dn);n+=2;
 if(dn){memcpy(p+n,data,dn);n+=dn;}
 c->tx[0]=0;c->tx[1]=(uint8_t)(n>>16);c->tx[2]=(uint8_t)(n>>8);
 c->tx[3]=(uint8_t)n;
 if(!exact_write(c,c->tx,n+NB))return -1;
 if(!exact_read(c,c->rx,4))return -1;
 /* TCP session keepalives can appear between requests. */
 while(c->rx[0]==0x85 && c->rx[1]==0 && c->rx[2]==0&&c->rx[3]==0){
  if(!exact_read(c,c->rx,4))return -1;
 }
 if(c->rx[0]!=0)return -1;
 c->rxlen=((unsigned)c->rx[1]<<16)|((unsigned)c->rx[2]<<8)|c->rx[3];
 if(c->rxlen<H+3||c->rxlen>MAIZONET_SMB1_RX_CAP)return -1;
 if(!exact_read(c,c->rx+NB,c->rxlen))return -1;
 p=c->rx+NB;
 if(p[0]!=0xff||p[1]!='S'||p[2]!='M'||p[3]!='B'||
    p[4]!=command||r16(p+30)!=mid)return -1;
 c->last_status=r32(p+5);
 if(c->last_status)return 0;
 n=H+1u+2u*(unsigned)p[H];
 if(n+2>c->rxlen||n+2+r16(p+n)>c->rxlen)return -1;
 return 1;
}
int maizonet_smb1_connect_guest(MaizoNetSmb1*c,MaizoNetTransport t,
                                 const char*ip,const char*share){
 uint8_t words[26],data[288];uint8_t*p;unsigned n,limit;
 if(!c||!t.receive||!t.transmit||!strfit(ip,63)||!strfit(share,63))return 0;
 memset(c,0,sizeof(*c));c->io=t;c->max_chunk=MAIZONET_SMB1_MAX_IO;
 /* SMB_COM_NEGOTIATE: NT LM 0.12, oldest dialect that supports ReadAndX. */
 data[0]=2;memcpy(data+1,"NT LM 0.12",11);
 if(exchange(c,0x72,0,0,data,12)!=1)return 0;
 p=c->rx+NB;
 if(p[H]!=17||r16(p+33)!=0)return 0; /* dialect index 0 */
 /* Maximum buffer is negotiated, not assumed. */
 limit=r32(p+33+7);
 if(limit<2048)return 0;
 if(limit<MAIZONET_SMB1_MAX_IO+128)c->max_chunk=(uint16_t)(limit-128);
 /* Traditional guest SessionSetupAndX with null credentials and no
  * extended security. Some Samba servers require SPNEGO/NTLMSSP;
  * those are unsupported and must fail without sending passwords. */
 memset(words,0,sizeof(words));words[0]=0xff;
 w16(words+4,32768);w16(words+6,2);
 w32(words+10,r32(p+33+15)); /* negotiate session key */
 /* Advertise large-file offsets: GameCube disc offsets must not truncate. */
 w32(words+22,0x00000008u); /* CAP_LARGE_FILES */
 data[0]=0;data[1]=0; /* anonymous account, primary domain */
 memcpy(data+2,"MaizoNet",9);
 memcpy(data+11,"Wii",4);
 if(exchange(c,0x73,words,26,data,15)!=1)return 0;
 c->uid=r16(c->rx+NB+28);c->session=1;
 /* SMB_COM_TREE_CONNECT_ANDX; shared guest directory, read-only. */
 memset(words,0,8);words[0]=0xff;w16(words+6,1);
 n=0;data[n++]=0;data[n++]='\\';data[n++]='\\';
 for(p=(uint8_t*)ip;*p;p++)data[n++]=*p;
 data[n++]='\\';
 for(p=(uint8_t*)share;*p;p++)data[n++]=*p;
 data[n++]=0;memcpy(data+n,"?????",6);n+=6;
 if(exchange(c,0x75,words,8,data,n)!=1){c->session=0;return 0;}
 c->tid=r16(c->rx+NB+24);c->tree=1;
 return 1;
}
int maizonet_smb1_open(MaizoNetSmb1*c,const char*path){
 uint8_t words[30],data[300];unsigned n;
 if(!c||!c->tree||c->opened||!strfit(path,255))return 0;
 /* Reject relative parent/absolute share escapes; only one opened ISO. */
 if(strstr(path,"..")||strchr(path,':')||path[0]=='\\')return 0;
 n=(unsigned)strlen(path);
 if(n+2>sizeof(data))return 0;
 memset(words,0,sizeof(words));words[0]=0xff;
 w16(words+6,0); /* access read */
 w16(words+16,1); /* open existing only */
 data[0]=4;
 memcpy(data+1,path,n);
 data[n+1]=0;
 if(exchange(c,0x2d,words,30,data,n+2)!=1)return 0;
 if(c->rx[NB+H]<3)return 0;
 c->fid=(uint16_t)r16(c->rx+NB+33+4);
 c->opened=1;
 return 1;
}
int maizonet_smb1_read_at(MaizoNetSmb1*c,uint64_t offset,void*buffer,unsigned len){
 uint8_t words[24],*out=(uint8_t*)buffer;
 unsigned finished=0,got,off;uint8_t*p;
 if(!c||!c->opened||(!out&&len))return -1;
 if(offset+len<offset)return -1;
 while(finished<len){
  unsigned part=len-finished;
  if(part>c->max_chunk)part=c->max_chunk;
  memset(words,0,sizeof(words));words[0]=0xff;
  w16(words+4,c->fid);
  w32(words+6,(uint32_t)(offset+finished));
  w16(words+10,part);w16(words+12,part);
  w32(words+20,(uint32_t)((offset+finished)>>32));
  if(exchange(c,0x2e,words,24,0,0)!=1)return -1;
  p=c->rx+NB;
  if(p[H]<12)return -1;
  got=r16(p+33+10)|((unsigned)r16(p+33+14)<<16);
  off=r16(p+33+12);
  /* DataOffset is relative to the start of the SMB header, not NetBIOS.
   * The claimed bytes must reside within ByteCount, never in SMB metadata. */
  {
   unsigned bytes_at=H+1u+2u*(unsigned)p[H]+2u;
   unsigned bytes=r16(p+bytes_at-2u);
   if(off<bytes_at || off>bytes_at+bytes ||
      got>bytes_at+bytes-off || got>part || off+got>c->rxlen)
    return -1;
  }
  if(got==0)break;
  memcpy(out+finished,p+off,got);
  finished+=got;
 }
 return (int)finished; /* caller rejects short reads for disk sectors */
}
void maizonet_smb1_close(MaizoNetSmb1*c){
 uint8_t words[6];
 if(!c)return;
 if(c->opened){memset(words,0,sizeof(words));w16(words,c->fid);
  exchange(c,0x04,words,6,0,0);}
 c->opened=0;c->tree=0;c->session=0;
}
const char *maizonet_smb1_error(uint32_t status){
 if(status==0xC0000022u)return "SMB: access denied";
 if(status==0xC0000034u)return "SMB: file not found";
 if(status==0xC00000CCu)return "SMB: share not found";
 if(status==0xC000006Du)return "SMB: authentication rejected";
 if(status==0xC0000016u)return "SMB: extended authentication required";
 return "SMB: incompatible server or transport error";
}
