/* MaizoDev Agent - Wii hardware probe, independent of Nintendont.
 * Only accepts fixed diagnostic commands; never executes arbitrary code.
 */
#include <gccore.h>
#include <network.h>
#include <ogc/lwp_watchdog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
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
static void reply(int fd,const char *s){send(fd,s,strlen(s),0);}
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
 int init=net_init();
 char ip[32]="SEM REDE",peer[32]="-";
 if(init<0){logmsg("ERRO: net_init");}
 else{
  struct in_addr a;a.s_addr=net_gethostip();
  snprintf(ip,sizeof(ip),"%s",inet_ntoa(a));
  logmsg("Rede inicializada");
 }
 int listener=-1;
 if(init>=0){
  listener=socket(AF_INET,SOCK_STREAM,0);
  if(listener>=0){
   int yes=1;setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
   struct sockaddr_in addr;memset(&addr,0,sizeof(addr));
   addr.sin_family=AF_INET;addr.sin_port=htons(PORT);addr.sin_addr.s_addr=INADDR_ANY;
   if(bind(listener,(struct sockaddr*)&addr,sizeof(addr))<0||listen(listener,1)<0){
    logmsg("ERRO: bind/listen");close(listener);listener=-1;
   }else{fcntl(listener,F_SETFL,O_NONBLOCK);logmsg("Aguardando Android TCP:17446");}
  }
 }
 char pending[256];unsigned used=0;char current[80]="-";
 while(running){
  WPAD_ScanPads();
  if(WPAD_ButtonsDown(0)&WPAD_BUTTON_HOME)running=0;
  if(client<0 && listener>=0){
   struct sockaddr_in remote;socklen_t len=sizeof(remote);
   int fd=accept(listener,(struct sockaddr*)&remote,&len);
   if(fd>=0){client=fd;fcntl(client,F_SETFL,O_NONBLOCK);
    snprintf(peer,sizeof(peer),"%s",inet_ntoa(remote.sin_addr));logmsg("Android conectado");used=0;
    reply(client,"MAIZODEV/1 READY\n");
   }
  }
  if(client>=0){
   char buf[96];int n=recv(client,buf,sizeof(buf),0);
   if(n==0){close(client);client=-1;logmsg("Android desconectado");}
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
   }else if(errno!=EAGAIN && errno!=EWOULDBLOCK){
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
