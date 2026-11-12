/* MaizoNintendont - independent SMBv1 menu for pinned Nintendont loader.
 * Read-only guest SMB on a trusted LAN. UI edits host/port/share in network.ini,
 * enumerates ISO/GCM files, validates the remote disc and launches through the
 * existing MaizoNet ARM sector backend. Stock SD/USB menus are untouched.
 */
#include <gccore.h>
#include <network.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <lwp_watchdog.h>
#include "global.h"
#include "menu.h"
#include "FPad.h"
#include "ff_utf8.h"
#include "maizonet_smb1.h"
#include "maizonet_loader.h"
#define MZ_DEV_SD 0
#define MZ_DEV_USB 1
extern u32 Shutdown;

typedef struct {char host[64],port[8],share[64],protocol[8],udp_port[8],path[196];} MzSettings;
typedef struct {int fd,last_rc;char last_op;} MzSock;
static MaizoNetDirEntry mz_entries[64];
static char mz_status[96];

static int mz_rx(void *ctx,void *buf,unsigned n){
 MzSock*s=(MzSock*)ctx;
 int rc=net_recv(s->fd,buf,n,0);
 if(rc<=0){s->last_rc=rc;s->last_op='R';}
 return rc;
}
static int mz_tx(void *ctx,const void *buf,unsigned n){
 MzSock*s=(MzSock*)ctx;
 int rc=net_send(s->fd,buf,n,0);
 if(rc<=0){s->last_rc=rc;s->last_op='W';}
 return rc;
}
static int mz_ip(const char *src,struct in_addr *dst){
 unsigned oct=0,v=0,digits=0;unsigned char *p=(unsigned char*)&dst->s_addr;
 if(!src)return 0;
 while(1){
  char ch=*src++;
  if(ch>='0'&&ch<='9'){
   if(++digits>3)return 0;
   v=v*10+(unsigned)(ch-'0');if(v>255)return 0;
  }else if(ch=='.'||ch==0){
   if(!digits||oct>=4)return 0;
   p[oct++]=(unsigned char)v;
   if(ch==0)break;
   digits=0;v=0;
  }else return 0;
 }
 return oct==4;
}
static const char* mz_ini_path(void){
 static char p[48];
 snprintf(p,sizeof(p),"%s:/maizo/network.ini",GetRootDevice());
 return p;
}
static void mz_defaults(MzSettings*c){
 memset(c,0,sizeof(*c));
 strcpy(c->host,"192.168.1.100");
 strcpy(c->port,"1445");
 strcpy(c->share,"PS2");
 strcpy(c->protocol,"smb");strcpy(c->udp_port,"17445");
}
static void mz_load_settings(MzSettings*c){
 FIL f;UINT size;char line[320],*cur,*e;int ch;unsigned k=0;
 mz_defaults(c);
 if(f_open_char(&f,mz_ini_path(),FA_READ|FA_OPEN_EXISTING)!=FR_OK)return;
 while(1){
  ch=0;size=0;
  if(f_read(&f,&ch,1,&size)!=FR_OK||size==0)break;
  if(ch=='\r')continue;
  if(ch!='\n'&&k<sizeof(line)-1){line[k++]=(char)ch;continue;}
  line[k]=0;k=0;
  cur=line;while(*cur==' '||*cur=='\t')cur++;
  e=strchr(cur,'=');if(!e)continue;
  *e++=0;e[strcspn(e,"\r\n")]=0;
  if(!strcmp(cur,"host"))snprintf(c->host,sizeof(c->host),"%s",e);
  else if(!strcmp(cur,"share"))snprintf(c->share,sizeof(c->share),"%s",e);
  else if(!strcmp(cur,"port"))snprintf(c->port,sizeof(c->port),"%s",e);
  else if(!strcmp(cur,"protocol"))snprintf(c->protocol,sizeof(c->protocol),"%s",e);
  else if(!strcmp(cur,"udp_port"))snprintf(c->udp_port,sizeof(c->udp_port),"%s",e);
  else if(!strcmp(cur,"path"))snprintf(c->path,sizeof(c->path),"%s",e);
 }
 f_close(&f);
}
static int mz_valid_settings(const MzSettings*c){
 struct in_addr addr;unsigned port=0,i;
 if(!mz_ip(c->host,&addr)||!c->share[0]||!c->port[0])return 0;
 for(i=0;c->port[i];i++){
  if(i>=5||c->port[i]<'0'||c->port[i]>'9')return 0;
  port=port*10u+(unsigned)(c->port[i]-'0');
 }
 if(port<1||port>65535)return 0;
 for(i=0;c->share[i];i++){
  if(i>=63||!(isalnum((unsigned char)c->share[i])||
      c->share[i]=='_'||c->share[i]=='-'||c->share[i]==' '))return 0;
 }
 if(strcmp(c->protocol,"smb") && strcmp(c->protocol,"udp"))return 0;
 if(!strcmp(c->protocol,"udp")){
  char *end=NULL;unsigned long u=strtoul(c->udp_port,&end,10);
  if(!end||*end||u<1||u>65535)return 0;
 }
 return 1;
}
/*
 * USB/SD write path. FatFs is mounted by the stock Nintendont loader.
 * Every success includes f_sync, close, reopen and a byte-for-byte readback.
 * Use bounded writes (8 KiB) for older Wii USB mass-storage bridges.
 */
static int mz_make_dir(const char*path,const char*tag){
 FRESULT ret=f_mkdir_char(path);
 if(ret==FR_OK||ret==FR_EXIST)return 1;
 snprintf(mz_status,sizeof(mz_status),"%s: mkdir FAT=%u",tag,(unsigned)ret);
 return 0;
}
static int mz_write_checked(const char*path,const void*data,unsigned len,const char*tag){
 const u8 *src=(const u8*)data;
 u8 *readback=NULL;
 FIL f;
 FRESULT ret,closed;
 UINT moved=0;
 unsigned off=0;
 ret=f_open_char(&f,path,FA_CREATE_ALWAYS|FA_WRITE);
 if(ret!=FR_OK){
  snprintf(mz_status,sizeof(mz_status),"%s: %s open FAT=%u",
           tag,GetRootDevice(),(unsigned)ret);
  return 0;
 }
 while(off<len){
  const UINT want=(UINT)((len-off)>8192u?8192u:(len-off));
  moved=0;
  ret=f_write(&f,src+off,want,&moved);
  if(ret!=FR_OK||moved!=want){
   snprintf(mz_status,sizeof(mz_status),"%s: write FAT=%u at=%u n=%u/%u",
            tag,(unsigned)ret,off,(unsigned)moved,(unsigned)want);
   f_close(&f);
   return 0;
  }
  off+=moved;
 }
 ret=f_sync(&f);
 closed=f_close(&f);
 if(ret!=FR_OK||closed!=FR_OK){
  snprintf(mz_status,sizeof(mz_status),"%s: %s sync=%u close=%u",
           tag,GetRootDevice(),(unsigned)ret,(unsigned)closed);
  return 0;
 }
 ret=f_open_char(&f,path,FA_READ|FA_OPEN_EXISTING);
 if(ret!=FR_OK){
  snprintf(mz_status,sizeof(mz_status),"%s: verify open FAT=%u",
           tag,(unsigned)ret);
  return 0;
 }
 if(f_size(&f)!=(FSIZE_t)len){
  snprintf(mz_status,sizeof(mz_status),"%s: size verify !=%u",tag,len);
  f_close(&f);return 0;
 }
 readback=memalign(32,8192);
 if(!readback){
  f_close(&f);snprintf(mz_status,sizeof(mz_status),"%s: verify OOM",tag);
  return 0;
 }
 off=0;
 while(off<len){
  const UINT want=(UINT)((len-off)>8192u?8192u:(len-off));
  moved=0;
  ret=f_read(&f,readback,want,&moved);
  if(ret!=FR_OK||moved!=want||memcmp(readback,src+off,want)){
   snprintf(mz_status,sizeof(mz_status),"%s: verify FAT=%u at=%u %u/%u",
            tag,(unsigned)ret,off,(unsigned)moved,(unsigned)want);
   free(readback);f_close(&f);return 0;
  }
  off+=moved;
 }
 free(readback);
 closed=f_close(&f);
 if(closed!=FR_OK){
  snprintf(mz_status,sizeof(mz_status),"%s: verify close FAT=%u",
           tag,(unsigned)closed);
  return 0;
 }
 return 1;
}
static void mz_remove_ascii_file(const char*path){
 WCHAR wpath[80];unsigned i=0;
 while(path[i]&&i<sizeof(wpath)/sizeof(wpath[0])-1){
  wpath[i]=(WCHAR)(unsigned char)path[i];i++;
 }
 if(path[i])return;
 wpath[i]=0;
 f_unlink(wpath); /* Best-effort cleanup of a successful probe. */
}
static int mz_probe_current_volume(void){
 char dir[48],path[64];u8 test[64];
 unsigned i;
 snprintf(dir,sizeof(dir),"%s:/maizo",GetRootDevice());
 if(!mz_make_dir(dir,"PROBE"))return 0;
 snprintf(path,sizeof(path),"%s:/maizo/mztest.bin",GetRootDevice());
 for(i=0;i<sizeof(test);i++)test[i]=(u8)(0x5au+(i*13u));
 if(!mz_write_checked(path,test,sizeof(test),"PROBE"))return 0;
 mz_remove_ascii_file(path);
 return 1;
}
static int mz_choose_writable_volume(void){
 extern FATFS *devices[2];
 int selected=UseSD?MZ_DEV_SD:MZ_DEV_USB;
 int order[2]={selected,selected==MZ_DEV_SD?MZ_DEV_USB:MZ_DEV_SD};
 int i;
 char failure[96]="";
 for(i=0;i<2;i++){
  int which=order[i];
  if(!devices[which])continue;
  UseSD=(which==MZ_DEV_SD);
  if(mz_probe_current_volume()){
   if(UseSD)ncfg->Config&=~NIN_CFG_USB;
   else ncfg->Config|=NIN_CFG_USB;
   return 1;
  }
  if(!failure[0])snprintf(failure,sizeof(failure),"%s",mz_status);
 }
 UseSD=(selected==MZ_DEV_SD);
 if(failure[0])snprintf(mz_status,sizeof(mz_status),"%s",failure);
 else strcpy(mz_status,"PROBE: nenhum FAT USB/SD montado");
 return 0;
}
static int mz_save_settings(const MzSettings*c,const char*last_path){
 char line[480],folder[48];const char*save_path=last_path?last_path:c->path;int count;
 if(!mz_valid_settings(c)){strcpy(mz_status,"INI: IP/porta/share invalido");return 0;}
 if(strchr(save_path,'\n')||strchr(save_path,'\r')){
  strcpy(mz_status,"INI: caminho invalido");return 0;
 }
 snprintf(folder,sizeof(folder),"%s:/maizo",GetRootDevice());
 if(!mz_make_dir(folder,"INI"))return 0;
 count=snprintf(line,sizeof(line),
          "host=%s\nport=%s\nshare=%s\npath=%s\nprotocol=%s\nudp_port=%s\n",
          c->host,c->port,c->share,save_path,c->protocol,c->udp_port);
 if(count<0||count>=(int)sizeof(line)){
  strcpy(mz_status,"INI: config grande demais");return 0;
 }
 if(!mz_write_checked(mz_ini_path(),line,(unsigned)count,"INI"))return 0;
 snprintf(mz_status,sizeof(mz_status),"INI OK (%s) %u bytes",
          GetRootDevice(),(unsigned)count);
 return 1;
}
static void mz_draw(const char*title,const char*hint){
 ClearScreen();PrintInfo();
 PrintFormat(DEFAULT_SIZE,BLACK,MENU_POS_X,MENU_POS_Y+20*3,"MAIZONINTENDONT  |  %s",title);
 PrintFormat(DEFAULT_SIZE,BLACK,MENU_POS_X,MENU_POS_Y+20*20,"%s",hint);
 if(mz_status[0])
  PrintFormat(DEFAULT_SIZE,MAROON,MENU_POS_X,MENU_POS_Y+20*18,"%.65s",mz_status);
 GRRLIB_Render();ClearScreen();
}
static int mz_edit(char*dst,unsigned cap,const char*title,const char*alphabet){
 char tmp[64];unsigned cursor=0,k,idx=0,n,alen=strlen(alphabet);
 snprintf(tmp,sizeof(tmp),"%s",dst);
 while(1){
  VIDEO_WaitVSync();FPAD_Update();
  if(Shutdown)LoaderShutdown();
  if(FPAD_Cancel(0))return 0;
  if(FPAD_Start(0)){snprintf(dst,cap,"%s",tmp);return 1;}
  n=strlen(tmp);if(cursor>n)cursor=n;
  if(FPAD_Left(0)&&cursor)cursor--;
  if(FPAD_Right(0)&&cursor+1<cap)cursor++;
  n=strlen(tmp);
  if(cursor>=n&&cursor+1<cap){
   tmp[cursor]=alphabet[0];tmp[cursor+1]=0;
   n++;
  }
  if(FPAD_Up(0)||FPAD_Down(0)){
   int dir=FPAD_Up(1)?1:-1;char *p=strchr(alphabet,tmp[cursor]);
   idx=p?(unsigned)(p-alphabet):0;
   tmp[cursor]=alphabet[(idx+alen+(dir>0?1:alen-1))%alen];
  }
  if(FPAD_X(0)){
   n=strlen(tmp);
   if(cursor<n){memmove(tmp+cursor,tmp+cursor+1,n-cursor);if(cursor&&cursor==n-1)cursor--;}
  }
  ClearScreen();PrintInfo();
  PrintFormat(DEFAULT_SIZE,BLACK,MENU_POS_X,MENU_POS_Y+20*5,"Editar %s",title);
  PrintFormat(DEFAULT_SIZE,BLACK,MENU_POS_X,MENU_POS_Y+20*7,"%.60s",tmp);
  for(k=0;k<cursor&&k<60;k++){}
  PrintFormat(DEFAULT_SIZE,MAROON,MENU_POS_X+(int)cursor*10,MENU_POS_Y+20*8,"^");
  PrintFormat(DEFAULT_SIZE,BLACK,MENU_POS_X,MENU_POS_Y+20*12,"Esq/Dir: posicao | Cima/Baixo: caractere");
  PrintFormat(DEFAULT_SIZE,BLACK,MENU_POS_X,MENU_POS_Y+20*13,"X: apagar | START: salvar | B: cancelar");
  GRRLIB_Render();
 }
}
static MaizoNetSmb1* mz_connect(const MzSettings*c,MzSock*s){
 struct sockaddr_in addr;struct timeval tv;MaizoNetSmb1*m;MaizoNetTransport t;
 unsigned port;int rc,nonblock=1,nodelay=1,connected=0;
 u64 started;
 s->fd=-1;s->last_rc=1;s->last_op='-';
 if(!mz_valid_settings(c)){
  strcpy(mz_status,"Configure IP, porta e share primeiro");return NULL;
 }
 ShowMessageScreen("MAIZONET: INICIANDO REDE...");
 rc=net_init();
 if(rc<0){
  snprintf(mz_status,sizeof(mz_status),"Wii: net_init=%d",rc);return NULL;
 }
 memset(&addr,0,sizeof(addr));
 mz_ip(c->host,&addr.sin_addr);
 port=(unsigned)strtoul(c->port,NULL,10);
 addr.sin_family=AF_INET;addr.sin_port=htons((unsigned short)port);
 s->fd=net_socket(AF_INET,SOCK_STREAM,0);
 if(s->fd<0){
  snprintf(mz_status,sizeof(mz_status),"SMB: socket=%d",s->fd);return NULL;
 }
 /* TinySMB reference: nonblocking connect bounded by monotonic Wii clock. */
 rc=net_ioctl(s->fd,FIONBIO,&nonblock);
 if(rc<0){
  snprintf(mz_status,sizeof(mz_status),"SMB: FIONBIO=%d",rc);
  net_close(s->fd);s->fd=-1;return NULL;
 }
 net_setsockopt(s->fd,IPPROTO_TCP,TCP_NODELAY,&nodelay,sizeof(nodelay));
 ShowMessageScreen("MAIZONET: CONECTANDO SMB...");
 started=ticks_to_millisecs(gettime());
 do{
  rc=net_connect(s->fd,(struct sockaddr*)&addr,sizeof(addr));
  if(rc>=0||rc==-EISCONN){connected=1;break;}
  if(rc!=-EINPROGRESS&&rc!=-EALREADY&&rc!=-EWOULDBLOCK&&rc!=-EAGAIN)break;
  usleep(10000);
 }while(ticks_to_millisecs(gettime())-started<6000u);
 nonblock=0;
 if(!connected){
  snprintf(mz_status,sizeof(mz_status),"SMB: connect=%d 6s max",rc);
  net_close(s->fd);s->fd=-1;return NULL;
 }
 rc=net_ioctl(s->fd,FIONBIO,&nonblock);
 if(rc<0){
  snprintf(mz_status,sizeof(mz_status),"SMB: FIONBIO reset=%d",rc);
  net_close(s->fd);s->fd=-1;return NULL;
 }
 tv.tv_sec=3;tv.tv_usec=0;
 net_setsockopt(s->fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
 net_setsockopt(s->fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));
 m=calloc(1,sizeof(*m));
 if(!m){
  strcpy(mz_status,"Memoria SMB insuficiente");
  net_close(s->fd);s->fd=-1;return NULL;
 }
 t.receive=mz_rx;t.transmit=mz_tx;t.user=s;
 if(!maizonet_smb1_connect_guest(m,t,c->host,c->share)){
  if(s->last_rc<=0){
   snprintf(mz_status,sizeof(mz_status),"SMB: handshake socket %c=%d",
            s->last_op,s->last_rc);
  }else{
   snprintf(mz_status,sizeof(mz_status),"SMB1: NT=%08lX %s",
            (unsigned long)m->last_status,maizonet_smb1_error(m->last_status));
  }
  free(m);net_close(s->fd);s->fd=-1;return NULL;
 }
 strcpy(mz_status,"SMB1 conectado, listando arquivos...");
 return m;
}
static void mz_disconnect(MaizoNetSmb1*m,MzSock*s){
 if(m){if(m->opened||m->session)maizonet_smb1_close(m);free(m);}
 if(s->fd>=0){net_close(s->fd);s->fd=-1;}
}
static int mz_is_disc(const char*name){
 size_t n=strlen(name);
 return (n>4&&(!strcasecmp(name+n-4,".iso")||!strcasecmp(name+n-4,".gcm")));
}
static int mz_boot_prepare(MaizoNetSmb1*m,const MzSettings*c,const char*path){
 u8 *buf=NULL;unsigned port;
 char full[128],marker[255];int ok=0,midlen;
 const unsigned size=131072u;u8 verify[32];
 if(strlen(path)>195){strcpy(mz_status,"Caminho remoto longo demais");return 0;}
 if(!maizonet_smb1_open(m,path)){
  snprintf(mz_status,sizeof(mz_status),"ISO: %s",maizonet_smb1_error(m->last_status));return 0;
 }
 buf=memalign(32,size);
 if(!buf){strcpy(mz_status,"Sem memoria para cabecalho ISO");goto done;}
 if(maizonet_smb1_read_at(m,0,buf,size)!=(int)size ||
    maizonet_smb1_read_at(m,0x20000,verify,sizeof(verify))!=(int)sizeof(verify)){
  strcpy(mz_status,"ISO: falha leitura remota 0 ou 0x20000");goto done;
 }
 if(buf[0x1c]!=0xC2||buf[0x1d]!=0x33||buf[0x1e]!=0x9F||buf[0x1f]!=0x3D){
  strcpy(mz_status,"Arquivo nao e ISO GameCube valida");goto done;
 }
 {
  unsigned i;
  for(i=0;i<6;i++)if(!((buf[i]>='A'&&buf[i]<='Z')||(buf[i]>='0'&&buf[i]<='9'))){
   strcpy(mz_status,"ID GameCube invalido");goto done;
  }
 }
 port=(unsigned)strtoul(strcmp(c->protocol,"udp")==0?c->udp_port:c->port,NULL,10);
 midlen=snprintf(marker,sizeof(marker),"%s|%s|%u|%s|%s",
               strcmp(c->protocol,"udp")==0?"MZUDP1":"MZSMB1",
               c->host,port,c->share,path);
 if(midlen<0||midlen>=255){strcpy(mz_status,"Marcador SMB excede CheatPath");goto done;}
 if(!mz_choose_writable_volume())goto done;
 snprintf(full,sizeof(full),"%s:/maizo",GetRootDevice());
 if(!mz_make_dir(full,"STUB"))goto done;
 snprintf(full,sizeof(full),"%s:/maizo/maizonet",GetRootDevice());
 if(!mz_make_dir(full,"STUB"))goto done;
 snprintf(full,sizeof(full),"%s:/maizo/maizonet/game.iso",GetRootDevice());
 if(!mz_write_checked(full,buf,size,"STUB"))goto done;
 /* The isolated kernel recognizes this marker and reads all remaining
  * sectors from SMB. Never persist the marker into normal nincfg.bin. */
 snprintf(ncfg->GamePath,sizeof(ncfg->GamePath),"/maizo/maizonet/game.iso");
 snprintf(ncfg->CheatPath,sizeof(ncfg->CheatPath),"%s",marker);
 memcpy(&ncfg->GameID,buf,4);
 ncfg->Config &= ~(NIN_CFG_CHEATS|NIN_CFG_CHEAT_PATH|NIN_CFG_AUTO_BOOT);
 DCFlushRange(ncfg,sizeof(NIN_CFG));
 /* network.ini is only a convenience for the NEXT launch. The active
  * remote session is already encoded in NIN_CFG's per-run transport marker.
  * Do not block a verified ISO if removable media rejects a config write.
  * The separate 128-KiB stub still MUST be written successfully above.
  */
 if(!mz_save_settings(c,path)){
  gprintf("MAIZONET: network.ini save failed; keeping runtime SMB config\r\n");
 }else{
  strcpy(mz_status,"ISO verificada: boot direto em andamento");
 }
 ok=1;
done:
 free(buf);
 if(!ok)maizonet_smb1_close_file(m);
 /* Closing this PPC session is safe. Kernel opens an independent SMB socket. */
 return ok;
}
static int mz_browse(MzSettings*c){
 MzSock sock;MaizoNetSmb1*m;
 char dir[246]="",full[260];
 int cursor=0,scroll=0,count,selected=0,done=0,ok=0;
 m=mz_connect(c,&sock);
 if(!m)return 0;
 while(!done){
  int i;
  count=maizonet_smb1_list_dir(m,dir,mz_entries,64);
  if(count<0){
   if(sock.last_rc<=0)
    snprintf(mz_status,sizeof(mz_status),"SMB: listar socket %c=%d",sock.last_op,sock.last_rc);
   else
    snprintf(mz_status,sizeof(mz_status),"SMB: listar NT=%08lX",(unsigned long)m->last_status);
   break;
  }
  cursor=scroll=0;
  while(1){
   VIDEO_WaitVSync();FPAD_Update();
   if(Shutdown)LoaderShutdown();
   if(FPAD_Start(0)){done=1;break;}
   if(FPAD_Cancel(0)){
    char *p;
    if(!dir[0]){done=1;break;}
    p=strrchr(dir,'\\');if(p)*p=0;else dir[0]=0;
    break;
   }
   if(FPAD_Down(0)&&cursor<count-1)cursor++;
   if(FPAD_Up(0)&&cursor>0)cursor--;
   if(cursor<scroll)scroll=cursor;
   if(cursor>=scroll+13)scroll=cursor-12;
   if(FPAD_OK(0)&&count>0){
    const MaizoNetDirEntry*e=&mz_entries[cursor];
    if(snprintf(full,sizeof(full),"%s%s%s",
       dir,dir[0]?"\\":"",e->name)>=sizeof(full)){
     strcpy(mz_status,"Caminho remoto longo demais");continue;
    }
    if(e->is_dir){
     if(strlen(full)>=sizeof(dir)){strcpy(mz_status,"Pasta profunda demais");continue;}
     strcpy(dir,full);break;
    }
    if(!mz_is_disc(e->name)){strcpy(mz_status,"Selecione um jogo .iso ou .gcm");continue;}
    selected=mz_boot_prepare(m,c,full);
    if(selected){ok=1;done=1;break;}
   }
   ClearScreen();PrintInfo();
   PrintFormat(DEFAULT_SIZE,BLACK,MENU_POS_X,MENU_POS_Y+20*3,"SMB / REDE : %.45s",dir[0]?dir:"(raiz)");
   for(i=scroll;i<count&&i<scroll+13;i++)
    PrintFormat(DEFAULT_SIZE,i==cursor?MAROON:BLACK,MENU_POS_X,
       MENU_POS_Y+20*(i-scroll+5),"%c %s %.47s",
       i==cursor?'>':' ',mz_entries[i].is_dir?"[DIR]":"     ",mz_entries[i].name);
   if(count==0)PrintFormat(DEFAULT_SIZE,BLACK,MENU_POS_X,MENU_POS_Y+20*7,"Pasta vazia");
   PrintFormat(DEFAULT_SIZE,BLACK,MENU_POS_X,MENU_POS_Y+20*19,"A: abrir | B: subir | HOME: voltar");
   if(mz_status[0])
    PrintFormat(DEFAULT_SIZE,MAROON,MENU_POS_X,MENU_POS_Y+20*20,"%.62s",mz_status);
   GRRLIB_Render();ClearScreen();
  }
 }
 mz_disconnect(m,&sock);
 return ok;
}
bool MaizoNetSelectGame(void){
 MzSettings cfg;int pos=0,previous_usb;
 extern FATFS *devices[2];
 static const char*items[]={
   "SMB: navegar pastas e jogos","Editar IP do servidor",
   "Editar porta","Editar compartilhamento","Kernel: alternar SMB / UDP",
   "Editar porta UDP","Salvar network.ini","Voltar para SD/USB"
 };
 const char *alpha_host="0123456789.";
 const char *alpha_port="0123456789";
 const char *alpha_share="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_- ";
 mz_status[0]=0;
 /* Respect the chosen FAT volume; fallback to the other mounted one. */
 previous_usb=(ncfg->Config & NIN_CFG_USB)!=0;
 if(!devices[previous_usb?MZ_DEV_USB:MZ_DEV_SD]){
  if(devices[MZ_DEV_SD]){UseSD=true;ncfg->Config&=~NIN_CFG_USB;}
  else if(devices[MZ_DEV_USB]){UseSD=false;ncfg->Config|=NIN_CFG_USB;}
  else {strcpy(mz_status,"Precisa de SD ou USB para stub");return false;}
 }else UseSD=!previous_usb;
 mz_load_settings(&cfg);
 if(!mz_choose_writable_volume()){
  /* Keep menu accessible for inspection, but browsing cannot boot. */
 }
 while(1){
  unsigned i;
  VIDEO_WaitVSync();FPAD_Update();
  if(Shutdown)LoaderShutdown();
  if(FPAD_Start(0)||FPAD_Cancel(0))break;
  if(FPAD_Down(0))pos=(pos+1)%8;
  if(FPAD_Up(0))pos=(pos+7)%8;
  if(FPAD_OK(0)){
   if(pos==0){if(mz_choose_writable_volume() && mz_browse(&cfg))return true;}
   else if(pos==1)mz_edit(cfg.host,sizeof(cfg.host),"IP",alpha_host);
   else if(pos==2)mz_edit(cfg.port,sizeof(cfg.port),"PORTA",alpha_port);
   else if(pos==3)mz_edit(cfg.share,sizeof(cfg.share),"SHARE",alpha_share);
   else if(pos==4){
    strcpy(cfg.protocol,strcmp(cfg.protocol,"udp")==0?"smb":"udp");
    snprintf(mz_status,sizeof(mz_status),"Kernel escolhido: %s",cfg.protocol);
   }
   else if(pos==5)mz_edit(cfg.udp_port,sizeof(cfg.udp_port),"PORTA UDP",alpha_port);
   else if(pos==6){if(mz_choose_writable_volume())mz_save_settings(&cfg,NULL);}
   else break;
  }
  ClearScreen();PrintInfo();
  PrintFormat(DEFAULT_SIZE,BLACK,MENU_POS_X,MENU_POS_Y+20*3,"MAIZONINDENDONT - SMB1 DIRETO r5-NET");
  for(i=0;i<8;i++)PrintFormat(DEFAULT_SIZE,i==(unsigned)pos?MAROON:BLACK,
       MENU_POS_X,MENU_POS_Y+20*(i+5),"%c %s",
       i==(unsigned)pos?'>':' ',items[i]);
  PrintFormat(DEFAULT_SIZE,BLACK,MENU_POS_X,MENU_POS_Y+20*14,
       "Servidor: %s:%s",cfg.host,cfg.port);
  PrintFormat(DEFAULT_SIZE,BLACK,MENU_POS_X,MENU_POS_Y+20*15,
       "Share: %s | Kernel: %s:%s",cfg.share,cfg.protocol,
       strcmp(cfg.protocol,"udp")==0?cfg.udp_port:cfg.port);
  PrintFormat(DEFAULT_SIZE,BLACK,MENU_POS_X,MENU_POS_Y+20*17,
       "A: selecionar | B/HOME: voltar");
  if(mz_status[0])PrintFormat(DEFAULT_SIZE,MAROON,MENU_POS_X,MENU_POS_Y+20*19,
       "%.65s",mz_status);
  GRRLIB_Render();ClearScreen();
 }
 return false;
}
