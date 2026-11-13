/* MaizoDev Agent - Wii hardware probe, independent of Nintendont.
 * Only accepts fixed diagnostic commands; never executes arbitrary code.
 */
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <network.h>
#include <ogc/lwp_watchdog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 17446
static char loglines[12][96];
static int logpos;
static volatile int running=1;
static int client=-1;
static unsigned commands=0;
static void logmsg(const char *s) {
 snprintf(loglines[logpos++%12],96,"%s",s);
}
static void display(const char *ip,const char *peer,const char *state,const char *cmd) {
 printf("\x1b[2J\x1b[H");
 printf("MAIZODEV AGENT | Wii hardware station\n\n");
 printf("Wii IP:     %s\nAndroid:    %s\nConexao:    %s\n",ip,peer,state);
 printf("Porta TCP: %d\nComandos:  %u\nAtual:     %s\n\n",PORT,commands,cmd);
 printf("LOG EM TEMPO REAL\n");
 for(int i=0;i<12;i++){int n=(logpos+i)%12;if(loglines[n][0])printf("%s\n",loglines[n]);}
 printf("\nHOME: sair  |  Apenas comandos de diagnostico\n");
}
static void reply(int fd,const char *s){net_send(fd,s,strlen(s),0);}
static void command(int fd,char *line,const char *ip) {
 char answer[160];
 commands++;
 if(!strcmp(line,"PING")) reply(fd,"PONG\n");
 else if(!strcmp(line,"STATUS")){
  snprintf(answer,sizeof(answer),"STATUS ip=%s commands=%u\n",ip,commands);
  reply(fd,answer);
 } else if(!strcmp(line,"MEM")){
  snprintf(answer,sizeof(answer),"MEM arena1=%lu arena2=%lu\n",
   (unsigned long)((char*)SYS_GetArena1Hi()-(char*)SYS_GetArena1Lo()),
   (unsigned long)((char*)SYS_GetArena2Hi()-(char*)SYS_GetArena2Lo()));
  reply(fd,answer);
 } else reply(fd,"ERR unknown-command\n");
 char msg[96];snprintf(msg,sizeof(msg),"CMD: %.78s",line);logmsg(msg);
}
int main(void){
 VIDEO_Init();
 GXRModeObj *mode=VIDEO_GetPreferredMode(NULL);
 void *fb=MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
 console_init(fb,20,20,mode->fbWidth,mode->xfbHeight,mode->fbWidth*VI_DISPLAY_PIX_SZ);
 VIDEO_Configure(mode);VIDEO_SetNextFramebuffer(fb);VIDEO_SetBlack(FALSE);VIDEO_Flush();VIDEO_WaitVSync();
 WPAD_Init();
 logmsg("Inicializando rede...");
 display("-","-","INICIANDO","-");
 int init=-1;
 for(int attempt=0;attempt<40;attempt++){
  init=net_init();
  if(init>=0 && net_gethostip()!=0)break;
  char status[96];snprintf(status,sizeof(status),"Rede: tentativa %d/40 (ret=%d)",attempt+1,init);
  logmsg(status);display("-","-","DHCP / WIFI","-");
  sleep(1);
 }
 if(init>=0 && net_gethostip()==0)init=-1;
 char ip[32]="SEM REDE",peer[32]="-";
 if(init<0){char error[96];snprintf(error,sizeof(error),"ERRO: net_init / DHCP = %d",init);logmsg(error);}
 else{
  struct in_addr a;a.s_addr=net_gethostip();
  snprintf(ip,sizeof(ip),"%s",inet_ntoa(a));
  logmsg("Rede inicializada");
 }
 int listener=-1;
 if(init>=0){
  listener=net_socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
  if(listener>=0){
   char status[96];snprintf(status,sizeof(status),"socket OK: fd=%d",listener);logmsg(status);
   int yes=1;net_setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
   struct sockaddr_in addr;memset(&addr,0,sizeof(addr));
   addr.sin_family=AF_INET;addr.sin_port=htons(PORT);addr.sin_addr.s_addr=INADDR_ANY;
   int br=net_bind(listener,(struct sockaddr*)&addr,sizeof(addr));
   int be=errno;
   int lr=br==0?net_listen(listener,1):-1;
   int le=errno;
   if(br<0||lr<0){
    char error[96];snprintf(error,sizeof(error),"ERRO: bind=%d errno=%d listen=%d errno=%d",br,be,lr,le);
    logmsg(error);net_close(listener);listener=-1;
   }else{u32 nonblock=1;net_ioctl(listener,FIONBIO,&nonblock);logmsg("LISTEN OK 0.0.0.0:17446");}
  }else{char error[96];snprintf(error,sizeof(error),"ERRO socket errno=%d",errno);logmsg(error);}
 }
 char pending[256];unsigned used=0;char current[80]="-";
 while(running){
  WPAD_ScanPads();
  if(WPAD_ButtonsDown(0)&WPAD_BUTTON_HOME)running=0;
  if(client<0 && listener>=0){
   struct sockaddr_in remote;socklen_t len=sizeof(remote);
   int fd=net_accept(listener,(struct sockaddr*)&remote,&len);
   if(fd>=0){client=fd;u32 nonblock=1;net_ioctl(client,FIONBIO,&nonblock);
    snprintf(peer,sizeof(peer),"%s",inet_ntoa(remote.sin_addr));logmsg("Android conectado");used=0;
    reply(client,"MAIZODEV/1 READY\n");
   }
  }
  if(client>=0){
   char buf[96];int n=net_recv(client,buf,sizeof(buf),0);
   if(n==0){net_close(client);client=-1;logmsg("Android desconectado");}
   else if(n>0){
    for(int i=0;i<n;i++){
     if(buf[i]=='\n'){
      pending[used]=0;
      if(used && pending[used-1]=='\r')pending[--used]=0;
      snprintf(current,sizeof(current),"%.79s",pending);
      command(client,pending,ip);used=0;
     }else if(used<sizeof(pending)-1)pending[used++]=buf[i];
     else{used=0;reply(client,"ERR line-too-long\n");}
    }
   }else if(errno!=EAGAIN && errno!=EWOULDBLOCK && n!=-EAGAIN && n!=-EWOULDBLOCK){
    close(client);client=-1;logmsg("Erro de conexao");
   }
  }
  display(ip,peer,client>=0?"CONECTADO":"AGUARDANDO",current);
  VIDEO_WaitVSync();usleep(100000);
 }
 if(client>=0)close(client);
 if(listener>=0)close(listener);
 net_deinit();return 0;
}
