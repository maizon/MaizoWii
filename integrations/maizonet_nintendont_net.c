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
#include "common.h"
#include "string.h"
#include "ff_utf8.h"
#include "maizonet_net_ipv4.h"
#define SO_SOCKET 0x0f
#define SO_CONNECT 0x04
#define SO_CLOSE 0x03
#define SO_SEND 0x0d
#define SO_RECV 0x0c
#define SO_STARTUP 0x1f
#define SO_HOSTID 0x10
#define SO_OPT 0x09
#define NCD_GETLINKSTATUS 0x07
#define SO_GETINTERFACEOPT 0x1c
static s32 mzfd=-1,mzsocket=-1;
static MaizoNetSmb1 mzclient ALIGNED(32);
static u8 send_buf[16384] ALIGNED(32);
static u8 recv_buf[16384] ALIGNED(32);
static u32 soc_in[3] ALIGNED(32);
static u32 req_in[2] ALIGNED(32);
static u32 resp[8] ALIGNED(32);
static u32 mz_ncd_link[8] ALIGNED(32);
static u32 mz_if_req[2] ALIGNED(32);
static u32 mz_if_addr[8] ALIGNED(32);
static u32 mz_if_length[8] ALIGNED(32);
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
static int mz_error=0;
static u32 mz_ntstatus=0;
int maizonet_nin_last_error(void){return mz_error?mz_error:199;}
#define MZ_TRACE_CAP 64u
typedef struct {u32 tick,phase,offhi,offlo,size; s32 result;} MzTrace;
static MzTrace mz_trace[MZ_TRACE_CAP];
static unsigned mz_trace_total=0;
static void mz_record(u32 phase,u64 offset,u32 size,s32 result){
 MzTrace *e=&mz_trace[mz_trace_total++ % MZ_TRACE_CAP];
 e->tick=read32(HW_TIMER);e->phase=phase;
 e->offhi=(u32)(offset>>32);e->offlo=(u32)offset;
 e->size=size;e->result=result;
}
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
 s32 kd=-1,ncd=-1,ret;unsigned k;
 mz_error=110;mz_record(110,0,port,0);
 if(!ipv4(host,addr))return 0;
 /* Query Wii NCD link status before KD/SO, as upstream libogc net_init
  * does. This is read-only and does not change network profiles/NAND. */
 mz_error=108;
 ncd=IOS_Open("/dev/net/ncd/manage",0);
 mz_record(108,0,0,ncd);
 if(ncd>=0){
  memset(mz_ncd_link,0,sizeof(mz_ncd_link));
  vec[0].data=mz_ncd_link;vec[0].len=sizeof(mz_ncd_link);
  ret=IOS_Ioctlv(ncd,NCD_GETLINKSTATUS,0,1,vec);
  sync_before_read(mz_ncd_link,sizeof(mz_ncd_link));
  mz_record(109,0,mz_ncd_link[0],ret);
  IOS_Close(ncd);ncd=-1;
 }
 mz_error=111;
 mzfd=IOS_Open("/dev/net/ip/top",0);
 mz_record(111,0,0,mzfd);
 if(mzfd<0)return 0;
 /* PPC loader already called libogc net_init before the virtual disc
  * opens. If IOS retains the assigned IP, reuse it: starting NWC24/SO
  * a second time on ARM could tear down that network interface. */
 ret=IOS_Ioctl(mzfd,SO_HOSTID,NULL,0,NULL,0);
 mz_record(123,0,0,ret);
 if(maizonet_ipv4_hostid_ok((u32)ret))goto network_ready;
 /* Physical Wii log: KD opens (fd=3), but NWC24 start returns -33.
  * A failed KD ioctl alone does not prove SO is unavailable. */
 mz_error=112;
 kd=IOS_Open("/dev/net/kd/request",0);
 mz_record(112,0,0,kd);
 if(kd>=0){
  for(k=0;k<15;k++){
   memset(resp,0,sizeof(resp));
   ret=IOS_Ioctl(kd,6,NULL,0,resp,sizeof(resp));
   if(ret<0){mz_record(113,0,0,ret);break;}
   if(ret==0 && resp[0]!= (u32)-29 && resp[0]!=(u32)-15 && (s32)resp[0]<0){
    mz_record(113,0,0,(s32)resp[0]);
    break; /* KD error does not prove SO unavailable. */
   }
   if(ret==0 && (resp[0]==0 || resp[0]==(u32)-15))break;
   mdelay(100);
  }
  if(k>=15)mz_record(113,0,0,-110);
  IOS_Close(kd);kd=-1;
 }
 /* The SO stack may already have been started by an earlier loader.
  * Probe SOGetHostID even if SO_STARTUP says 'already running'. */
 mz_error=114;
 ret=IOS_Ioctl(mzfd,SO_STARTUP,NULL,0,NULL,0);
 mz_record(114,0,0,ret);
 mz_error=115;
 for(k=0;k<4;k++){
  ret=IOS_Ioctl(mzfd,SO_HOSTID,NULL,0,NULL,0);
  mz_record(115,0,0,ret);
  if(maizonet_ipv4_hostid_ok((u32)ret))break;
  mdelay(120);
 }
 if(k>=4){
  /* Unknown IPv4: ask IOS for interface IP/netmask before stopping. */
  memset(mz_if_req,0,sizeof(mz_if_req));
  memset(mz_if_addr,0,sizeof(mz_if_addr));
  memset(mz_if_length,0,sizeof(mz_if_length));
  mz_if_req[0]=0xfffe;mz_if_req[1]=0x4003;
  mz_if_length[0]=sizeof(mz_if_addr);
  vec[0].data=mz_if_req;vec[0].len=sizeof(mz_if_req);
  vec[1].data=mz_if_addr;vec[1].len=sizeof(mz_if_addr);
  vec[2].data=mz_if_length;vec[2].len=sizeof(u32);
  ret=IOS_Ioctlv(mzfd,SO_GETINTERFACEOPT,1,2,vec);
  sync_before_read(mz_if_addr,sizeof(mz_if_addr));
  sync_before_read(mz_if_length,sizeof(mz_if_length));
  mz_record(118,0,mz_if_addr[0],ret);
  mz_record(119,0,mz_if_addr[1],(s32)mz_if_length[0]);
  /* A zero SO_HOSTID does not prove TCP cannot connect: do not make
   * this advisory API a mandatory boot gate. The real operation is
   * SO_SOCKET + SO_CONNECT to the configured SMB server. */
  mz_record(135,0,port,0);
 }
network_ready:
 mz_error=116;
 soc_in[0]=2;soc_in[1]=1;soc_in[2]=0;
 mzsocket=IOS_Ioctl(mzfd,SO_SOCKET,soc_in,sizeof(soc_in),NULL,0);
 mz_record(116,0,0,mzsocket);
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
 mz_error=117;
 ret=IOS_Ioctl(mzfd,SO_CONNECT,&conn_par,sizeof(conn_par),NULL,0);
 mz_record(117,0,port,ret);
 if(ret<0)goto error;
 mz_error=0;
 return 1;
error:
 if(ncd>=0)IOS_Close(ncd);
 if(kd>=0)IOS_Close(kd);
 if(mzsocket>=0){closing=mzsocket;IOS_Ioctl(mzfd,SO_CLOSE,&closing,4,NULL,0);mzsocket=-1;}
 if(mzfd>=0){IOS_Close(mzfd);mzfd=-1;}
 return 0;
}
/* Keep the remote session independent from normal FAT/USB disk handling.
 * One bounded reconnect is allowed after a failed sector read; requests are
 * retried from the original offset, never from a partially filled buffer. */
static bool mz_connect_backend(void){
 MaizoNetTransport t;
 if(!mzsocket_init(mzhost,mzport))return false;
 t.receive=mzrecv;t.transmit=mzsend;t.user=0;
 mz_error=120;
 mz_record(120,0,0,0);
 if(!maizonet_smb1_connect_guest(&mzclient,t,mzhost,mzshare)){
  mz_ntstatus=mzclient.last_status;
  mz_record(120,0,0,(s32)mz_ntstatus);
  dbgprintf("MZ SMB1: session rejected %08x\r\n",mzclient.last_status);
  maizonet_nin_close();return false;
 }
 mz_error=121;
 mz_record(121,0,0,0);
 if(!maizonet_smb1_open(&mzclient,mzpath)){
  mz_ntstatus=mzclient.last_status;
  mz_record(121,0,0,(s32)mz_ntstatus);
  dbgprintf("MZ SMB1: ISO open rejected %08x\r\n",mzclient.last_status);
  maizonet_nin_close();return false;
 }
 mzopened=1;mz_record(122,0,0,1);
 mz_error=0;
 return true;
}
bool maizonet_nin_open(const char*marker){
 mz_error=101;
 mz_ntstatus=0;
 if(mzopened || !split_marker(marker))return false;
 if(!mz_connect_backend())return false;
 dbgprintf("MZ SMB1: remote ISO backend opened\r\n");
 return true;
}
bool maizonet_nin_read(void*out,u32 size,u64 offset){
 int ret;
 if(!mzopened||!out||!size){mz_error=130;return false;}
 ret=maizonet_smb1_read_at(&mzclient,offset,out,size);
 mz_record(130,offset,size,ret);
 if(ret!=(int)size){
  dbgprintf("MZ SMB1: read failed at %08x:%08x len=%u status=%08x; reconnect\r\n",
     (u32)(offset>>32),(u32)offset,size,mzclient.last_status);
  /* Do not send SMB CLOSE on a dead TCP connection: that could block for
   * another full socket timeout. maizonet_nin_close still closes IOS fds. */
  mz_error=130;
  mz_ntstatus=mzclient.last_status;
  mzopened=0;
  maizonet_nin_close();
  mdelay(80);
  if(!mz_connect_backend())return false;
  mz_record(132,offset,size,0);
  ret=maizonet_smb1_read_at(&mzclient,offset,out,size);
  mz_record(133,offset,size,ret);
  if(ret!=(int)size){
   mz_error=131;
   mz_ntstatus=mzclient.last_status;
   dbgprintf("MZ SMB1: retry failed; fail closed\r\n");
   mzopened=0;
   maizonet_nin_close();
   return false;
  }
 }
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

/* Only reached after an unrecoverable remote ISO error.
 * Persist one bounded diagnostic record on the selected FAT volume while the
 * Nintendont kernel still has it mounted. Do not power down automatically;
 * normal Nintendont and local images never call this routine. */
void maizonet_nin_fatal(const char *step,u64 offset,u32 size){
 FIL log; UINT written=0;unsigned total,i,index;
 char line[192];
 int code=maizonet_nin_last_error();
 BootStatusError(-9,code);
 _sprintf(line,"MZNET_FATAL step=%s code=%d nt=%08X offset=%08X:%08X bytes=%u\n",
          step?step:"UNKNOWN",code,mz_ntstatus,(u32)(offset>>32),(u32)offset,size);
 if(f_open_char(&log,"/maizo/maizonet/diagnostic.log",
                FA_OPEN_ALWAYS|FA_WRITE)==FR_OK){
  f_lseek(&log,f_size(&log));
  f_write(&log,line,strlen(line),&written);
  total=mz_trace_total<MZ_TRACE_CAP?mz_trace_total:MZ_TRACE_CAP;
  for(i=0;i<total;i++){
   MzTrace *e;
   index=(mz_trace_total-total+i)%MZ_TRACE_CAP;
   e=&mz_trace[index];
   _sprintf(line,"MZNET_TRACE tick=%08X phase=%u result=%d offset=%08X:%08X size=%u\n",
            e->tick,e->phase,e->result,e->offhi,e->offlo,e->size);
   f_write(&log,line,strlen(line),&written);
  }
  f_sync(&log);
  f_close(&log);
 }
 dbgprintf("%s",line);
 /* Keep diagnostic state observable and prevent boot with corrupted sectors. */
 for(;;) mdelay(100);
}
