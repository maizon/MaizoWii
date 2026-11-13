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
 c->last_command=command;
 c->last_reason=0;
 if(!c->io.receive||!c->io.transmit||wn&1u||wn/2>255u||
    NB+H+1+wn+2+dn>sizeof(c->tx)){c->last_reason=1;return -1;}
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
 if(!exact_write(c,c->tx,n+NB)){c->last_reason=2;return -1;}
 if(!exact_read(c,c->rx,4)){c->last_reason=3;return -1;}
 /* TCP session keepalives can appear between requests. */
 while(c->rx[0]==0x85 && c->rx[1]==0 && c->rx[2]==0&&c->rx[3]==0){
  if(!exact_read(c,c->rx,4)){c->last_reason=4;return -1;}
 }
 if(c->rx[0]!=0){c->last_reason=5;return -1;}
 c->rxlen=((unsigned)c->rx[1]<<16)|((unsigned)c->rx[2]<<8)|c->rx[3];
 if(c->rxlen<H+3||c->rxlen>MAIZONET_SMB1_RX_CAP){c->last_reason=5;return -1;}
 if(!exact_read(c,c->rx+NB,c->rxlen)){c->last_reason=3;return -1;}
 p=c->rx+NB;
 if(p[0]!=0xff||p[1]!='S'||p[2]!='M'||p[3]!='B'||
    p[4]!=command||r16(p+30)!=mid){c->last_reason=6;return -1;}
 c->last_status=r32(p+5);
 if(c->last_status){c->last_reason=7;return 0;}
 n=H+1u+2u*(unsigned)p[H];
 if(n+2>c->rxlen||n+2+r16(p+n)>c->rxlen){c->last_reason=8;return -1;}
 return 1;
}
int maizonet_smb1_connect_guest(MaizoNetSmb1*c,MaizoNetTransport t,
                                 const char*ip,const char*share){
 uint8_t words[26],data[288];uint8_t*p;unsigned n,limit;
 if(!c||!t.receive||!t.transmit||!strfit(ip,63)||!strfit(share,63))return 0;
 memset(c,0,sizeof(*c));c->io=t;c->max_chunk=MAIZONET_SMB1_MAX_IO;
 /* SMB_COM_NEGOTIATE: NT LM 0.12, oldest dialect that supports ReadAndX. */
 c->last_phase=1;
 data[0]=2;memcpy(data+1,"NT LM 0.12",11);
 if(exchange(c,0x72,0,0,data,12)!=1)return 0;
 p=c->rx+NB;
 if(p[H]!=17||r16(p+33)!=0){c->last_reason=9;return 0;} /* dialect index 0 */
 /* Maximum buffer is negotiated, not assumed. */
 limit=r32(p+33+7);
 if(limit<2048){c->last_reason=10;return 0;}
 if(limit<MAIZONET_SMB1_MAX_IO+128)c->max_chunk=(uint16_t)(limit-128);
 /* Traditional guest SessionSetupAndX with null credentials and no
  * extended security. Some Samba servers require SPNEGO/NTLMSSP;
  * those are unsupported and must fail without sending passwords. */
 c->last_phase=2;
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
 c->last_phase=3;
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
 /* OPEN_ANDX carries SMB_STRING directly (without the 0x04 legacy
  * BufferFormat used by SMB_COM_OPEN). Preserve the terminator. */
 memcpy(data,path,n);
 data[n]=0;
 if(exchange(c,0x2d,words,30,data,n+1)!=1)return 0;
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
  w16(words+10,part);
  /* OPL's SMB ReadAndX sets MaxCount without requiring MinCount=part.
   * Accept a short response and continue until the requested size. */
  w16(words+12,0);
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
void maizonet_smb1_close_file(MaizoNetSmb1*c){
 uint8_t words[6];
 if(c&&c->opened){memset(words,0,sizeof(words));w16(words,c->fid);
  exchange(c,0x04,words,6,0,0);c->opened=0;}
}
void maizonet_smb1_close(MaizoNetSmb1*c){
 if(!c)return;
 maizonet_smb1_close_file(c);
 c->tree=0;c->session=0;
}
const char *maizonet_smb1_error(uint32_t status){
 if(status==0xC0000022u)return "SMB: access denied";
 if(status==0xC0000034u)return "SMB: file not found";
 if(status==0xC00000CCu)return "SMB: share not found";
 if(status==0xC000006Du)return "SMB: authentication rejected";
 if(status==0xC0000016u)return "SMB: extended authentication required";
 if(status==0xC000003Au)return "SMB: directory path not found";
 if(status==0xC000000Du)return "SMB: invalid request parameter";
 if(status==0xC00000BBu)return "SMB: operation not supported";
 return "SMB: incompatible server or transport error";
}

/* SMB_COM_TRANSACTION2 / TRANS2_FIND_FIRST2 and FIND_NEXT2 (read-only).
 * DOS/non-Unicode filenames on the trusted NT1 guest share. The list is
 * bounded, search handles are closed, and malformed offsets fail closed.
 * This deliberately does not implement NTLMSSP or SMB2.
 */
static int trans2_find(MaizoNetSmb1*c,int next,uint16_t sid,const char*pattern,
                       unsigned *count,unsigned *end,unsigned *handle,
                       const uint8_t **entries,unsigned *entry_bytes){
 uint8_t words[30],data[330],*p,*par;unsigned plen,datalen;
 unsigned param_off,param_len,data_off,data_len,base,byte_count;
 memset(words,0,sizeof(words));memset(data,0,sizeof(data));
 if(next){
  /* SMB_FIND_FILE_BOTH_DIRECTORY_INFO, resume from the server cursor. */
  plen=13;w16(data+1,sid);w16(data+3,24);w16(data+5,0x0104);
  w32(data+7,0);w16(data+11,0x000A);data[13]=0;plen=13;
 }else{
  size_t size=strlen(pattern);
  if(size>250)return -1;
  plen=12u+(unsigned)size+1u;
  w16(data+1,0x0016); /* hidden/system/directories included */
  w16(data+3,24); /* one bounded page of 8KiB */
  w16(data+5,0x0002); /* close search automatically at end */
  w16(data+7,0x0104); /* SMB_FIND_FILE_BOTH_DIRECTORY_INFO */
  w32(data+9,0);
  memcpy(data+13,pattern,size+1);
 }
 /* One pad byte makes the parameter block offset 66 from SMB header. */
 datalen=plen+1u;
 w16(words+0,plen);
 w16(words+4,10);
 w16(words+6,8192);
 w16(words+18,plen);
 w16(words+20,H+1+30+2+1);
 words[26]=1;w16(words+28,next?2:1);
 if(exchange(c,0x32,words,sizeof(words),data,datalen)!=1)return -1;
 p=c->rx+NB;
 if(p[H]<10)return -1;
 base=H+1+2u*(unsigned)p[H]+2u;
 if(base>c->rxlen)return -1;
 byte_count=r16(p+base-2u);
 param_len=r16(p+33+6);
 param_off=r16(p+33+8);
 data_len=r16(p+33+12);
 data_off=r16(p+33+14);
 if(r16(p+33+10)!=0 || r16(p+33+16)!=0 ||
    r16(p+33)!=param_len || r16(p+33+2)!=data_len ||
    param_off<base || data_off<base ||
    param_off>base+byte_count || data_off>base+byte_count ||
    param_len>base+byte_count-param_off ||
    data_len>base+byte_count-data_off || 
    param_off+param_len>c->rxlen || data_off+data_len>c->rxlen)
  return -1; /* secondary/multipart responses intentionally unsupported */
 par=p+param_off;
 if(param_len<(next?8u:10u))return -1;
 if(next){
  *count=r16(par);*end=r16(par+2);
 }else{
  *handle=r16(par);*count=r16(par+2);*end=r16(par+4);
 }
 if(*count>24)return -1;
 *entries=p+data_off;*entry_bytes=data_len;
 return 0;
}

/* Samba may return level 0x0104 filenames in UTF-16LE even for an
 * eight-bit SMB1 request. Accept printable 8-bit or UTF-16LE ASCII.
 * Non-ASCII is skipped until the entire wire path supports Unicode. */
static int mz_find_name(char *out,unsigned cap,const uint8_t *s,unsigned n){
 unsigned i,k=0,zeros=0,samples=0,wide=0;
 while(n && s[n-1]==0)n--; /* Samba 4 can include string terminator in FileNameLength. */
 if(!n)return 0;
 if(!(n&1u)){
  for(i=1;i<n&&samples<12;i+=2,samples++)if(s[i]==0)zeros++;
  if(samples && zeros*2>=samples)wide=1;
 }
 if(wide){
  for(i=0;i<n;i+=2){
   unsigned ch=(unsigned)s[i]|((unsigned)s[i+1]<<8);
   if(ch<32||ch>126||ch=='/'||ch=='\\'||ch=='|'||k+1>=cap)return 0;
   out[k++]=(char)ch;
  }
 }else{
  for(i=0;i<n;i++){
   unsigned ch=s[i];
   if(ch<32||ch>126||ch=='/'||ch=='\\'||ch=='|'||k+1>=cap)return 0;
   out[k++]=(char)ch;
  }
 }
 out[k]=0;
 return strcmp(out,".")!=0&&strcmp(out,"..")!=0;
}
int maizonet_smb1_list_dir(MaizoNetSmb1*c,const char*dir,
                            MaizoNetDirEntry*out,unsigned cap){
 char pattern[260];size_t len;uint16_t sid=0;
 unsigned count=0,end=0,handle=0,total=0,page;
 const uint8_t *entries;unsigned bytes;
 int begun=0,success=0;
 if(!c||!c->tree||!out||cap==0||!dir||strlen(dir)>245||
    strstr(dir,"..")||strchr(dir,':')||dir[0]=='/'||dir[0]=='\\')return -1;
 len=strlen(dir);
 memcpy(pattern,dir,len);
 if(len && pattern[len-1]!='\\')pattern[len++]='\\';
 pattern[len++]='*';pattern[len]=0;
 for(page=0;page<16 && total<cap;page++){
  unsigned pos=0,i;
  if(trans2_find(c,begun,sid,pattern,&count,&end,&handle,&entries,&bytes)!=0)
   goto done;
  if(!begun){sid=(uint16_t)handle;begun=1;}
  if(!count){if(end)success=1;goto done;}
  for(i=0;i<count;i++){
   uint32_t next,nlen,attr;
   const uint8_t *entry;
   if(bytes-pos<94)goto done;
   entry=entries+pos;next=r32(entry);
   attr=r32(entry+56);nlen=r32(entry+60);
   if(nlen==0||nlen>127||nlen>bytes-pos-94 ||
       (next && (next<94u+nlen||next>bytes-pos)))goto done;
   if(total>=cap)break;
   if(!mz_find_name(out[total].name,sizeof(out[total].name),entry+94,nlen))
    goto advance;
   out[total].is_dir=(attr&0x10u)!=0;
   total++;
advance:
   if(!next)break;
   pos+=next;
  }
  if(end||total>=cap){success=1;goto done;}
 }
 success=1;
done:
 if(begun && !end){
  uint8_t closewords[2];w16(closewords,sid);
  /* Never leak a FIND search cursor on cancel/truncation. */
  exchange(c,0x34,closewords,2,0,0);
 }
 return success?(int)total:-1;
}
