/* MaizoNet Nintendont/Starlet ARM prototype: read-only SMB1 guest ISO
 * backend over IOS native SO sockets. No added server, no full ISO transfer.
 * Isolated: local Nintendont paths are unchanged when marker is absent.
 *
 * This requires a real Wii SMB guest-share acceptance test. A successful
 * ARM build does NOT mean a game will boot or reconnect after link loss.
 * Only trusted LAN, no passwords or SMB signing, raw ISO/GCM only.
 */
#include "Config.h"
#include "maizonet_nintendont_net.h"
#include "maizonet_smb1.h"
#include "debug.h"
#define SO_SOCKET 0x0f
#define SO_CONNECT 0x04
#define SO_CLOSE 0x03
#define SO_SEND 0x0d
#define SO_RECV 0x0c
#define SO_STARTUP 0x1f
#define SO_HOSTID 0x10
#define SO_OPT 0x09
static s32 mzfd=-1,mzsocket=-1;
static MaizoNetSmb1 mzclient ALIGNED(32);
static u8 send_buf[16384] ALIGNED(32);
static u8 recv_buf[16384] ALIGNED(32);
static u32 soc_in[3] ALIGNED(32);
static u32 req_in[2] ALIGNED(32);
static u32 resp[8] ALIGNED(32);
static ioctlv vec[3] ALIGNED(32);
static struct {
 u32 socket,flags,dest;
 u8 address[28];
} send_par ALIGNED(32);
static struct {
 u32 socket,has_addr;
 u8 address[28];
} conn_par ALIGNED(32);
static struct {
 u32 socket,level,optname,optlen;
 u8 optval[20];
} opt_par ALIGNED(32);
static u32 closing ALIGNED(32);
static char mzhost[64],mzshare[64],mzpath[256];
static unsigned mzport=445;
static int mzopened=0;
bool maizonet_nin_requested(const char*marker){
 return marker && !strncmp(marker,"MZSMB1|",7);
}
static int dec(const char*s,unsigned *v){
 unsigned n=0,t=0;
 if(!s||!*s)return 0;
 for(;*s;s++){
  if(*s<'0'||*s>'9'||n++>5)return 0;
  t=t*10+(unsigned)(*s-'0');
  if(t>65535)return 0;
 }
 *v=t;return t!=0;
}
static int split_marker(const char*s){
 /* MZSMB1|IPv4|PORT|SHARE|path/inside/share.iso */
 char fields[4][256];unsigned lens[4]={0,0,0,0};int part=0;
 memset(fields,0,sizeof(fields));
 if(!maizonet_nin_requested(s))return 0;
 s+=7;
 for(;*s;s++){
  if(*s=='|' && part<3){part++;continue;}
  if(*s=='|'||lens[part]>=sizeof(fields[part])-1)return 0;
  if((unsigned char)*s<32u||(unsigned char)*s>126u)return 0;
  fields[part][lens[part]++]=*s;
 }
 if(part!=3||lens[0]>=sizeof(mzhost)||lens[2]>=sizeof(mzshare)||
    !lens[3]||lens[3]>=sizeof(mzpath))return 0;
 if(!dec(fields[1],&mzport))return 0;
 if(strstr(fields[3],"..")||fields[3][0]=='/'||fields[3][0]=='\\')return 0;
 strcpy(mzhost,fields[0]);strcpy(mzshare,fields[2]);strcpy(mzpath,fields[3]);
 return 1;
}
static int ipv4(const char*s,u8*out){
 unsigned n=0,k=0,val=0;
 if(!s||!*s)return 0;
 for(;*s;s++){
  if(*s>='0'&&*s<='9'){
   if(n>=3)return 0;
   val=val*10+(unsigned)(*s-'0');n++;
   if(val>255)return 0;
  }else if(*s=='.'){
   if(!n||k>=3)return 0;
   out[k++]=(u8)val;val=0;n=0;
  }else return 0;
 }
 if(!n||k!=3)return 0;
 out[3]=(u8)val;return 1;
}
static int mzsend(void*ctx,const void*payload,unsigned n){
 unsigned part;int sent;
 (void)ctx;
 if(mzfd<0||mzsocket<0||!n)return -1;
 part=n;if(part>sizeof(send_buf))part=sizeof(send_buf);
 memcpy(send_buf,payload,part);
 memset(&send_par,0,sizeof(send_par));
 send_par.socket=mzsocket;
 vec[0].data=send_buf;vec[0].len=part;
 vec[1].data=&send_par;vec[1].len=sizeof(send_par);
 sync_after_write(send_buf,ALIGN_FORWARD(part,32));
 sync_after_write(&send_par,sizeof(send_par));
 sent=IOS_Ioctlv(mzfd,SO_SEND,2,0,vec);
 if(sent<0 || (unsigned)sent>part)return -1;
 return sent;
}
static int mzrecv(void*ctx,void*buffer,unsigned n){
 unsigned part;int got;
 (void)ctx;
 if(mzfd<0||mzsocket<0||!n)return -1;
 part=n;if(part>sizeof(recv_buf))part=sizeof(recv_buf);
 req_in[0]=mzsocket;req_in[1]=0;
 vec[0].data=req_in;vec[0].len=8;
 vec[1].data=recv_buf;vec[1].len=part;
 vec[2].data=NULL;vec[2].len=0;
 sync_after_write(req_in,sizeof(req_in));
 got=IOS_Ioctlv(mzfd,SO_RECV,1,2,vec);
 if(got<=0 || (unsigned)got>part)return -1;
 sync_before_read(recv_buf,ALIGN_FORWARD(got,32));
 memcpy(buffer,recv_buf,got);
 return got;
}
static int mzsocket_init(const char*host,unsigned port){
 u8 addr[4];
 s32 kd=-1,ret;unsigned k;
 if(!ipv4(host,addr))return 0;
 mzfd=IOS_Open("/dev/net/ip/top",0);
 if(mzfd<0)return 0;
 kd=IOS_Open("/dev/net/kd/request",0);
 if(kd<0)goto error;
 for(k=0;k<15;k++){
  memset(resp,0,sizeof(resp));
  ret=IOS_Ioctl(kd,6,NULL,0,resp,sizeof(resp));
  if(ret==0 && resp[0]!= (u32)-29 && resp[0]!=(u32)-15 && (s32)resp[0]<0){
   dbgprintf("MZ SMB NWC failed %d\r\n",(s32)resp[0]);goto error;
  }
  if(ret==0 && (resp[0]==0 || resp[0]==(u32)-15))break;
  mdelay(100);
 }
 IOS_Close(kd);kd=-1;
 if(k>=15)goto error;
 ret=IOS_Ioctl(mzfd,SO_STARTUP,NULL,0,NULL,0);
 if(ret<0)goto error;
 for(k=0;k<20;k++){
  ret=IOS_Ioctl(mzfd,SO_HOSTID,NULL,0,NULL,0);
  if(ret>0)break; /* DHCP IP assigned */
  mdelay(100);
 }
 if(ret<=0)goto error;
 soc_in[0]=2;soc_in[1]=1;soc_in[2]=0; /* AF_INET, SOCK_STREAM */
 mzsocket=IOS_Ioctl(mzfd,SO_SOCKET,soc_in,sizeof(soc_in),NULL,0);
 if(mzsocket<0)goto error;
 memset(&opt_par,0,sizeof(opt_par));
 opt_par.socket=mzsocket;opt_par.level=0xffff;
 opt_par.optname=0x1006;opt_par.optlen=8; /* SO_RCVTIMEO */
 ((u32*)opt_par.optval)[0]=3; /* seconds */
 IOS_Ioctl(mzfd,SO_OPT,&opt_par,sizeof(opt_par),NULL,0);
 opt_par.optname=0x1005; /* SO_SNDTIMEO */
 IOS_Ioctl(mzfd,SO_OPT,&opt_par,sizeof(opt_par),NULL,0);
 memset(&conn_par,0,sizeof(conn_par));
 conn_par.socket=mzsocket;conn_par.has_addr=1;
 conn_par.address[0]=8;conn_par.address[1]=2;
 conn_par.address[2]=(u8)(port>>8);conn_par.address[3]=(u8)port;
 memcpy(conn_par.address+4,addr,4);
 if(IOS_Ioctl(mzfd,SO_CONNECT,&conn_par,sizeof(conn_par),NULL,0)<0)goto error;
 return 1;
error:
 if(kd>=0)IOS_Close(kd);
 if(mzsocket>=0){closing=mzsocket;IOS_Ioctl(mzfd,SO_CLOSE,&closing,4,NULL,0);mzsocket=-1;}
 if(mzfd>=0){IOS_Close(mzfd);mzfd=-1;}
 return 0;
}
bool maizonet_nin_open(const char*marker){
 MaizoNetTransport t;
 if(mzopened || !split_marker(marker))return false;
 if(!mzsocket_init(mzhost,mzport))return false;
 t.receive=mzrecv;t.transmit=mzsend;t.user=0;
 if(!maizonet_smb1_connect_guest(&mzclient,t,mzhost,mzshare)){
  dbgprintf("MZ SMB1: session rejected %08x\r\n",mzclient.last_status);
  maizonet_nin_close();return false;
 }
 if(!maizonet_smb1_open(&mzclient,mzpath)){
  dbgprintf("MZ SMB1: ISO open rejected %08x\r\n",mzclient.last_status);
  maizonet_nin_close();return false;
 }
 mzopened=1;
 dbgprintf("MZ SMB1: remote ISO backend opened\r\n");
 return true;
}
bool maizonet_nin_read(void*out,u32 size,u64 offset){
 int ret;
 if(!mzopened||!out||!size)return false;
 ret=maizonet_smb1_read_at(&mzclient,offset,out,size);
 if(ret!=(int)size)return false;
 sync_after_write_align32(out,size);
 return true;
}
void maizonet_nin_close(void){
 if(mzopened)maizonet_smb1_close(&mzclient);
 mzopened=0;
 if(mzfd>=0&&mzsocket>=0){
  closing=mzsocket;
  IOS_Ioctl(mzfd,SO_CLOSE,&closing,4,NULL,0);
 }
 mzsocket=-1;
 if(mzfd>=0)IOS_Close(mzfd);
 mzfd=-1;
 memset(&mzclient,0,sizeof(mzclient));
}
