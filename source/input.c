#include "app.h"
#include <wiiuse/wpad.h>
#include <ogc/usb.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
/* Scope: known 0079:0006 report layout. Other HID layouts require descriptors/profiles.
 * Poll asynchronously: never issue blocking interrupt reads on the GX/UI thread. */
enum { B_UP=1,B_DOWN=2,B_LEFT=4,B_RIGHT=8,B_A=16,B_B=32,B_X=64,B_Y=128,B_L=256,B_R=512,B_START=1024,B_BACK=2048 };
static s32 hid_fd=-1;static u8 hid_ep=0x81;static u16 hid_len=8;
static unsigned char hid_data[64] ATTRIBUTE_ALIGN(32);
static volatile unsigned hid_mask=0;static volatile int running=0,connected=0;
static u16 active_vid=0,active_pid=0;static unsigned tick=0;
static volatile int broken=0;
static volatile unsigned report_count=0,read_error_count=0;
static u16 last_seen_vid=0,last_seen_pid=0;
static int last_profile_loaded=0;
void input_hid_diagnostics(HidDiagnostics*out){
 if(!out)return;
 out->vid=last_seen_vid;out->pid=last_seen_pid;
 out->reports=report_count;out->read_errors=read_error_count;
 out->connected=connected;out->profile_loaded=last_profile_loaded;
}
typedef struct {
 int enabled,x,y,hat,hat_shift,hat_mask,nin_dpad;
 int off[8];unsigned mask[8];
 int dir_off[8];unsigned dir_mask[8];
} HidProfile;
static HidProfile profile;
static int byte_in(int i,int n){return i>=0&&i<n;}
static int parse_num(const char*s,long*out,int base){
 char*end=NULL;long value=strtol(s,&end,base);
 if(end==s||(*end!='\0'&&*end!='\n'&&*end!='\r'))return 0;
 *out=value;return 1;
}
static int profile_read(u16 vid,u16 pid){
 char p[144];FILE*f=NULL;int nintendont=0;
 for(int root=0;root<2&&!f;root++){
  const char*unit=root?"sd":"usb";
  snprintf(p,sizeof(p),"%s:/maizo/controllers/%04X_%04X.ini",unit,vid,pid);
  f=fopen(p,"rb");if(f)break;
  snprintf(p,sizeof(p),"%s:/controllers/%04X_%04X.ini",unit,vid,pid);
  f=fopen(p,"rb");if(f){nintendont=1;break;}
 }
 memset(&profile,0,sizeof(profile));
 if(!f)return 0;
 profile.x=-1;profile.y=-1;profile.hat=-1;profile.hat_mask=15;
 profile.nin_dpad=-1;
 for(int i=0;i<8;i++){profile.off[i]=-1;profile.dir_off[i]=-1;}
 if(!nintendont){profile.x=0;profile.y=1;}
 int valid=0,wrongid=0;
 char line[160];
 const char*local[]={"a","b","x_button","y_button","l","r","start","select"};
 const char*ninbuttons[]={"A","B","X","Y","L","R","S","Power"};
 const char*dirs[]={"Up","Right","Down","Left","RightUp","DownRight","DownLeft","UpLeft"};
 while(fgets(line,sizeof(line),f)){
  char*key=line;while(*key==' '||*key=='\t')key++;
  if(!*key||*key=='#'||*key=='[')continue;
  char*eq=strchr(key,'=');if(!eq)continue;*eq++=0;
  key[strcspn(key," \r\n\t")]=0;
  eq[strcspn(eq," \r\n\t")]=0;
  if(!nintendont){
   long value;if(!parse_num(eq,&value,0))continue;
   if(!strcmp(key,"x_offset"))profile.x=value;
   else if(!strcmp(key,"y_offset"))profile.y=value;
   else if(!strcmp(key,"hat_offset"))profile.hat=value;
   else if(!strcmp(key,"hat_mask"))profile.hat_mask=value&255;
   else if(!strcmp(key,"hat_shift"))profile.hat_shift=value>7?0:value;
   for(int i=0;i<8;i++){
    char tmp[48];snprintf(tmp,sizeof(tmp),"%s_offset",local[i]);
    if(!strcmp(key,tmp))profile.off[i]=value;
    snprintf(tmp,sizeof(tmp),"%s_mask",local[i]);
    if(!strcmp(key,tmp)){profile.mask[i]=(unsigned)value&255;valid++;}
   }
   continue;
  }
  long value;
  if(!strcmp(key,"VID")&&parse_num(eq,&value,16)){wrongid|=(value!=vid);continue;}
  if(!strcmp(key,"PID")&&parse_num(eq,&value,16)){wrongid|=(value!=pid);continue;}
  if(!strcmp(key,"DPAD")&&parse_num(eq,&value,0)){profile.nin_dpad=(int)value;continue;}
  if(!strcmp(key,"StickX")&&parse_num(eq,&value,0)){profile.x=value;continue;}
  if(!strcmp(key,"StickY")&&parse_num(eq,&value,0)){profile.y=value;continue;}
  char*comma=strchr(eq,',');if(!comma)continue;*comma++=0;
  long off,mask;
  if(!parse_num(eq,&off,0)||!parse_num(comma,&mask,16)||off<0||off>=64||mask<0||mask>255)
   continue;
  for(int i=0;i<8;i++){
   if(!strcmp(key,ninbuttons[i])){
    profile.off[i]=(int)off;profile.mask[i]=(unsigned)mask;valid++;
   }
   if(!strcmp(key,dirs[i])){
    profile.dir_off[i]=(int)off;profile.dir_mask[i]=(unsigned)mask;
   }
  }
 }
 fclose(f);
 if(wrongid)return 0;
 if(nintendont){
  if(valid<2)return 0;
 }else if(valid<2 && profile.hat<0)return 0;
 profile.enabled=1;return 1;
}
static unsigned decode_profile(const unsigned char*b,int n){
 unsigned m=0;
 if(byte_in(profile.x,n)){
  if(b[profile.x]<48)m|=B_LEFT;
  if(b[profile.x]>208)m|=B_RIGHT;
 }
 if(byte_in(profile.y,n)){
  if(b[profile.y]<48)m|=B_UP;
  if(b[profile.y]>208)m|=B_DOWN;
 }
 if(profile.nin_dpad>=0){
  const unsigned directions[]={
   B_UP,B_RIGHT,B_DOWN,B_LEFT,
   B_UP|B_RIGHT,B_DOWN|B_RIGHT,B_DOWN|B_LEFT,B_UP|B_LEFT};
  for(int i=0;i<8;i++){
   if(!byte_in(profile.dir_off[i],n))continue;
   unsigned val=b[profile.dir_off[i]];
   if(profile.nin_dpad==1){
    if((val&15)==(profile.dir_mask[i]&15))m|=directions[i];
   }else if(i<4&&profile.dir_mask[i]&&
            (val&profile.dir_mask[i])==profile.dir_mask[i])m|=directions[i];
  }
 }else if(byte_in(profile.hat,n)){
  int hat=(b[profile.hat]&profile.hat_mask)>>profile.hat_shift;
  if(hat==0||hat==1||hat==7)m|=B_UP;
  if(hat==1||hat==2||hat==3)m|=B_RIGHT;
  if(hat==3||hat==4||hat==5)m|=B_DOWN;
  if(hat==5||hat==6||hat==7)m|=B_LEFT;
 }
 const unsigned bits[]={B_A,B_B,B_X,B_Y,B_L,B_R,B_START,B_BACK};
 for(int i=0;i<8;i++)
  if(byte_in(profile.off[i],n)&&profile.mask[i]&&
     (b[profile.off[i]]&profile.mask[i])==profile.mask[i])m|=bits[i];
 return m;
}
static unsigned decode(const unsigned char*b,int n){
 if(profile.enabled)return decode_profile(b,n);
 if(active_vid!=0x0079||active_pid!=0x0006||n<7)return 0;
 unsigned m=0;int hat=b[5]&15;
 if(hat==0||hat==1||hat==7)m|=B_UP;
 if(hat==1||hat==2||hat==3)m|=B_RIGHT;
 if(hat==3||hat==4||hat==5)m|=B_DOWN;
 if(hat==5||hat==6||hat==7)m|=B_LEFT;
 if(b[0]<48)m|=B_LEFT;if(b[0]>208)m|=B_RIGHT;
 if(b[1]<48)m|=B_UP;if(b[1]>208)m|=B_DOWN;
 if(b[5]&0x40)m|=B_A;if(b[5]&0x80)m|=B_B;
 if(b[5]&0x20)m|=B_X;if(b[5]&0x10)m|=B_Y;
 if(b[6]&0x01)m|=B_L;if(b[6]&0x02)m|=B_R;
 if(b[6]&0x20)m|=B_START;if(b[6]&0x10)m|=B_BACK;
 return m;
}
static s32 on_report(s32 result,void*unused){
 if(!running)return 0;
 if(result>=0){report_count++;hid_mask=decode(hid_data,hid_len);connected=1;
  if(USB_ReadIntrMsgAsync(hid_fd,hid_ep,hid_len,hid_data,on_report,NULL)<0){read_error_count++;connected=0;broken=1;}
 }else{read_error_count++;connected=0;hid_mask=0;broken=1;}
 return 0;
}
static void search_hid(void){
 if(hid_fd>=0)return;
 usb_device_entry found[32] ATTRIBUTE_ALIGN(32);u8 count=0;
 if(USB_GetDeviceList(found,32,USB_CLASS_HID,&count)<0){read_error_count++;return;}
 for(int i=0;i<count;i++){
  /* Accept any actual USB HID device with a matching Nintendont/MAIZO
   * VID_PID profile. Never guess a report map by VID alone. */
  int defined=profile_read(found[i].vid,found[i].pid);
  last_seen_vid=found[i].vid;last_seen_pid=found[i].pid;
  last_profile_loaded=defined || (found[i].vid==0x0079&&found[i].pid==0x0006);
  if(!last_profile_loaded)continue;
  s32 fd=-1;if(USB_OpenDevice(found[i].device_id,found[i].vid,found[i].pid,&fd)<0||fd<0){read_error_count++;continue;}
  usb_devdesc desc;memset(&desc,0,sizeof(desc));int good=0;
  if(USB_GetDescriptors(fd,&desc)>=0){
   for(int a=0;a<desc.bNumConfigurations&&!good;a++){
    usb_configurationdesc*c=&desc.configurations[a];
    for(int b=0;b<c->bNumInterfaces&&!good;b++){
     usb_interfacedesc*f=&c->interfaces[b];
     if(f->bInterfaceClass!=USB_CLASS_HID)continue;
     for(int k=0;k<f->bNumEndpoints;k++){
      usb_endpointdesc*ep=&f->endpoints[k];
      if((ep->bEndpointAddress&USB_ENDPOINT_IN)&&((ep->bmAttributes&3)==USB_ENDPOINT_INTERRUPT)){
       hid_ep=ep->bEndpointAddress;hid_len=ep->wMaxPacketSize;
       if(hid_len<7)hid_len=8;if(hid_len>sizeof(hid_data))hid_len=sizeof(hid_data);
       good=1;break;
      }
     }
    }
   }
   USB_FreeDescriptors(&desc);
  }
  if(!good){read_error_count++;USB_CloseDevice(&fd);continue;}
  hid_fd=fd;active_vid=found[i].vid;active_pid=found[i].pid;broken=0;
  if(USB_ReadIntrMsgAsync(fd,hid_ep,hid_len,hid_data,on_report,NULL)<0){read_error_count++;connected=0;hid_fd=-1;USB_CloseDevice(&fd);}
  return;
 }
}
void input_init(void){PAD_Init();WPAD_Init();for(int i=0;i<4;i++)WPAD_SetDataFormat(i,WPAD_FMT_BTNS_ACC_IR);USB_Initialize();running=1;}
void input_shutdown(void){running=0;hid_mask=0;connected=0;if(hid_fd>=0){USB_CloseDevice(&hid_fd);hid_fd=-1;}USB_Deinitialize();WPAD_Shutdown();}
void input_poll(Input*out){
 memset(out,0,sizeof(*out));PAD_ScanPads();WPAD_ScanPads();
 unsigned m=hid_mask;
 if(broken&&hid_fd>=0){s32 fd=hid_fd;hid_fd=-1;USB_CloseDevice(&fd);broken=0;connected=0;hid_mask=0;}
 for(int port=0;port<4;port++){
 u32 gc=PAD_ButtonsHeld(port),wi=WPAD_ButtonsHeld(port);
 if(gc&PAD_BUTTON_UP||wi&WPAD_BUTTON_UP)m|=B_UP;
 if(gc&PAD_BUTTON_DOWN||wi&WPAD_BUTTON_DOWN)m|=B_DOWN;
 if(gc&PAD_BUTTON_LEFT||wi&WPAD_BUTTON_LEFT)m|=B_LEFT;
 if(gc&PAD_BUTTON_RIGHT||wi&WPAD_BUTTON_RIGHT)m|=B_RIGHT;
 if(gc&PAD_BUTTON_A||wi&WPAD_BUTTON_A)m|=B_A;
 if(gc&PAD_BUTTON_B||wi&WPAD_BUTTON_B)m|=B_B;
 if(gc&PAD_BUTTON_X||wi&WPAD_BUTTON_1)m|=B_X;
 if(gc&PAD_BUTTON_Y||wi&WPAD_BUTTON_2)m|=B_Y;
 if(gc&PAD_TRIGGER_L||wi&WPAD_BUTTON_MINUS)m|=B_L;
 if(gc&PAD_TRIGGER_R||wi&WPAD_BUTTON_PLUS)m|=B_R;
 if(gc&PAD_BUTTON_START||wi&WPAD_BUTTON_HOME)m|=B_START;
 if(gc&PAD_TRIGGER_Z)m|=B_BACK;
 if(PAD_StickX(port)<-55)m|=B_LEFT;
 if(PAD_StickX(port)>55)m|=B_RIGHT;
 if(PAD_StickY(port)<-55)m|=B_DOWN;
 if(PAD_StickY(port)>55)m|=B_UP;
 }
 static unsigned previous=0;static unsigned repeat=0;
 unsigned pressed=m&~previous;
 if((m&(B_UP|B_DOWN|B_LEFT|B_RIGHT))&&(m&(B_UP|B_DOWN|B_LEFT|B_RIGHT))==(previous&(B_UP|B_DOWN|B_LEFT|B_RIGHT))){
  repeat++;if(repeat>=22&&repeat%6==0)pressed|=m&(B_UP|B_DOWN|B_LEFT|B_RIGHT);
 }else repeat=0;
 previous=m;
 out->up=!!(pressed&B_UP);out->down=!!(pressed&B_DOWN);
 out->left=!!(pressed&B_LEFT);out->right=!!(pressed&B_RIGHT);
 out->accept=!!(pressed&B_A);out->back=!!(pressed&B_B);
 out->favorite=!!(pressed&B_Y);out->search=!!(pressed&B_X);
 out->settings=!!(pressed&B_START);out->refresh=!!(pressed&B_BACK);
 out->tab_prev=!!(pressed&B_L);out->tab_next=!!(pressed&B_R);
 out->hid_present=connected;tick++;
 if(hid_fd<0 && tick%180==1)search_hid();
}
