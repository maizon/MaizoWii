/*
 * MAIZO Wii - per-DOL USB HID shim (GPL-2.0-or-later).
 * Adapter for standalone libogc applications, NOT a persistent IOS service.
 * Standard gamepads may use parsed HID report descriptors; Nintendont-style
 * controller.ini maps provide fallback for unusual layouts. Not every USB
 * receiver implements class HID or provides a conventional report descriptor.
 *
 * API is intentionally static: this file is included exactly once into the
 * module's input translation unit. No Makefile change or symbol collision.
 * Kernel HID settings/polling can differ from IOS libogc; Wii testing needed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <ogc/usb.h>
#include "maizo_report.h"
#include <malloc.h>
#ifndef USB_CLASS_HID
#define USB_CLASS_HID 3
#endif

typedef struct { int offset; unsigned mask; } MaizoBit;
typedef struct {
 int valid, nin, dpad_mode, multiin, multivalue, stickx, sticky, cstickx, csticky;
 MaizoBit buttons[9]; /* A B X Y L R Start Select Z */
 MaizoBit dirs[8];    /* U R D L UR DR DL UL */
} MaizoProfile;

static MaizoProfile mh_profile;
static MzrMap mh_auto;
static int mh_auto_active=0;
static volatile unsigned mh_report_count=0,mh_read_errors=0;
static volatile unsigned mh_last_report_id=0;
static s32 mh_fd=-1;
static u8 mh_ep=0x81, mh_len=8;
static u16 mh_vid=0,mh_pid=0,mh_seen_vid=0,mh_seen_pid=0;
static int mh_profile_source=0;
static unsigned char mh_report[64] ATTRIBUTE_ALIGN(32);
static volatile u32 mh_report_buttons[4]={0};
static volatile s8 mh_ax[4]={0},mh_ay[4]={0},mh_rx[4]={0},mh_ry[4]={0};
static volatile int mh_live[4]={0};
static volatile int mh_dead=0,mh_running=0;
static u32 mh_buttons[4]={0},mh_previous[4]={0},mh_frame=0;

static int mh_load_profile(u16 vid,u16 pid) {
 char path[132]; FILE *f=NULL; int nin=0;
 memset(&mh_profile,0,sizeof(mh_profile));
 for(int i=0;i<9;i++)mh_profile.buttons[i].offset=-1;
 for(int i=0;i<8;i++)mh_profile.dirs[i].offset=-1;
 mh_profile.stickx=mh_profile.sticky=mh_profile.cstickx=mh_profile.csticky=-1;
 for(int r=0;r<2&&!f;r++) {
  const char *dev=r?"sd":"usb";
  snprintf(path,sizeof(path),"%s:/controllers/%04X_%04X.ini",dev,vid,pid);
  f=fopen(path,"rb");if(f){nin=1;break;}
  snprintf(path,sizeof(path),"%s:/maizo/controllers/%04X_%04X.ini",dev,vid,pid);
  f=fopen(path,"rb");if(f){nin=0;break;}
 }
 if(!f && vid==0x0079 && pid==0x0006) {
  /* Built-in layout equals the common Nintendont controller config.
   * DPAD = one 3-bit hat in the low nibble of report[5]. */
  mh_profile.valid=1;mh_profile.nin=1;mh_profile.dpad_mode=1;
  const int offs[]={5,5,5,5,6,6,6,6,6};
  const unsigned masks[]={0x40,0x80,0x20,0x10,1,2,0x20,0x10,0x08};
  for(int i=0;i<9;i++){mh_profile.buttons[i].offset=offs[i];mh_profile.buttons[i].mask=masks[i];}
  const unsigned hat[]={0,2,4,6,1,3,5,7};
  for(int i=0;i<8;i++){mh_profile.dirs[i].offset=5;mh_profile.dirs[i].mask=hat[i];}
  mh_profile.stickx=0;mh_profile.sticky=1;mh_profile.cstickx=3;mh_profile.csticky=4;
  return 1;
 }
 if(!f)return 0;
 mh_profile.nin=nin;mh_profile.dpad_mode=1;
 char line[148];int got=0,badid=0,gotvid=0,gotpid=0;
 const char*button_keys[]={"A","B","X","Y","L","R","S","Power","Z"};
 const char*dir_keys[]={"Up","Right","Down","Left","RightUp","DownRight","DownLeft","UpLeft"};
 while(fgets(line,sizeof(line),f)){
  char *p=line;while(*p==' '||*p=='\t')p++;
  if(!*p||*p=='#'||*p=='[')continue;
  char *eq=strchr(p,'=');if(!eq)continue;*eq++=0;
  p[strcspn(p," \r\n\t")]=0;eq[strcspn(eq," \r\n\t")]=0;
  if(!strcmp(p,"VID")){gotvid=1;badid|=(strtoul(eq,NULL,16)!=vid);continue;}
  if(!strcmp(p,"PID")){gotpid=1;badid|=(strtoul(eq,NULL,16)!=pid);continue;}
  if(!strcmp(p,"MultiIn")){
   int mode=(int)strtoul(eq,NULL,nin?16:0);
   if(mode>=0&&mode<=4)mh_profile.multiin=mode;
   continue;
  }
  if(!strcmp(p,"MultiInValue")){
   int value=(int)strtoul(eq,NULL,nin?16:0);
   if(value>=0&&value<=4)mh_profile.multivalue=value;
   continue;
  }
  if(!strcmp(p,"DPAD")){mh_profile.dpad_mode=atoi(eq);continue;}
  if(!nin){
   /* MAIZO profile syntax: a_offset=5, a_mask=0x40, ... */
   const char *keys[]={"a","b","x_button","y_button","l","r","start","select","z",
                       "up","right","down","left","right_up","down_right","down_left","up_left"};
   if(!strcmp(p,"x_offset")){mh_profile.stickx=atoi(eq);continue;}
   if(!strcmp(p,"y_offset")){mh_profile.sticky=atoi(eq);continue;}
   if(!strcmp(p,"rx_offset")){mh_profile.cstickx=atoi(eq);continue;}
   if(!strcmp(p,"ry_offset")){mh_profile.csticky=atoi(eq);continue;}
   if(!strcmp(p,"hat_offset")){
    int off=atoi(eq);if(off>=0&&off<64){
     for(int z=0;z<8;z++)mh_profile.dirs[z].offset=off;
     const unsigned hat[]={0,2,4,6,1,3,5,7};
     for(int z=0;z<8;z++)mh_profile.dirs[z].mask=hat[z];
     got+=8;
    }
    continue;
   }
   for(int z=0;z<17;z++){
    char key[50];MaizoBit *bit=z<9?&mh_profile.buttons[z]:&mh_profile.dirs[z-9];
    snprintf(key,sizeof(key),"%s_offset",keys[z]);
    if(!strcmp(p,key)){bit->offset=atoi(eq);got++;break;}
    snprintf(key,sizeof(key),"%s_mask",keys[z]);
    if(!strcmp(p,key)){bit->mask=strtoul(eq,NULL,0);got++;break;}
   }
   continue;
  }
  int *axis=NULL;
  if(!strcmp(p,"StickX"))axis=&mh_profile.stickx;
  else if(!strcmp(p,"StickY"))axis=&mh_profile.sticky;
  else if(!strcmp(p,"CStickX"))axis=&mh_profile.cstickx;
  else if(!strcmp(p,"CStickY"))axis=&mh_profile.csticky;
  if(axis){*axis=atoi(eq);continue;}
  MaizoBit *b=NULL;
  for(int i=0;i<9;i++)if(!strcmp(p,button_keys[i])){b=&mh_profile.buttons[i];break;}
  for(int i=0;i<8;i++)if(!strcmp(p,dir_keys[i])){b=&mh_profile.dirs[i];break;}
  if(!b)continue;
  char *comma=strchr(eq,',');if(!comma)continue;*comma++=0;
  int off=atoi(eq);unsigned long mask=strtoul(comma,NULL,16);
  if(off>=0&&off<64&&mask<=255){b->offset=off;b->mask=(unsigned)mask;got++;}
 }
 fclose(f);
 if(nin&&((gotvid&&gotpid)==0||badid))return 0;
 if(got<4)return 0;
 mh_profile.valid=1;return 1;
}
static int mh_byte(int pos){return pos>=0&&pos<(int)mh_len;}
static int mh_active(MaizoBit b,const unsigned char*d){
 return mh_byte(b.offset)&&((d[b.offset]&b.mask)==b.mask)&&b.mask!=0;
}
static s8 mh_axis(const unsigned char*d,int i) {
 if(!mh_byte(i))return 0;
 int v=((int)d[i]-128)*96/127;
 return v<-96?-96:v>96?96:(s8)v;
}
static void mh_decode(const unsigned char*d,int chan){
 if(mh_auto_active){
  MzrState st;
  if(!mzr_decode(&mh_auto,d,mh_len,&st)){
   mh_report_buttons[chan]=0;mh_ax[chan]=mh_ay[chan]=0;
   mh_rx[chan]=mh_ry[chan]=0;mh_read_errors++;return;
  }
  const u16 buttons[]={PAD_BUTTON_A,PAD_BUTTON_B,PAD_BUTTON_X,
   PAD_BUTTON_Y,PAD_TRIGGER_L,PAD_TRIGGER_R,PAD_TRIGGER_Z,
   PAD_TRIGGER_Z,PAD_TRIGGER_Z,PAD_BUTTON_START};
  u32 mask=0;
  for(unsigned i=0;i<10;i++)
   if(st.buttons&(1u<<i))mask|=buttons[i];
  if(st.up||st.y < -60)mask|=PAD_BUTTON_UP;
  if(st.down||st.y > 60)mask|=PAD_BUTTON_DOWN;
  if(st.left||st.x < -60)mask|=PAD_BUTTON_LEFT;
  if(st.right||st.x > 60)mask|=PAD_BUTTON_RIGHT;
  mh_ax[chan]=st.x;mh_ay[chan]=-st.y;
  mh_rx[chan]=st.rx;mh_ry[chan]=-st.ry;
  mh_report_buttons[chan]=mask;
  return;
 }

 const u16 bits[]={PAD_BUTTON_A,PAD_BUTTON_B,PAD_BUTTON_X,PAD_BUTTON_Y,
                   PAD_TRIGGER_L,PAD_TRIGGER_R,PAD_BUTTON_START,PAD_TRIGGER_Z,PAD_TRIGGER_Z};
 u32 mask=0;
 for(int i=0;i<9;i++)if(mh_active(mh_profile.buttons[i],d))mask|=bits[i];
 if(mh_profile.dpad_mode==1){
  /* Nintendont DPAD=1 uses exact hat value for eight directions, 7=neutral
   * on some devices. Require the offset and compare the complete low nibble. */
  for(int i=0;i<8;i++){
   MaizoBit b=mh_profile.dirs[i];
   if(mh_byte(b.offset)&&((d[b.offset]&15)==(b.mask&15))){
    if(i==0||i==4||i==7)mask|=PAD_BUTTON_UP;
    if(i==1||i==4||i==5)mask|=PAD_BUTTON_RIGHT;
    if(i==2||i==5||i==6)mask|=PAD_BUTTON_DOWN;
    if(i==3||i==6||i==7)mask|=PAD_BUTTON_LEFT;
   }
  }
 }else{
  const u16 dir[]={PAD_BUTTON_UP,PAD_BUTTON_RIGHT,PAD_BUTTON_DOWN,PAD_BUTTON_LEFT};
  for(int i=0;i<4;i++)if(mh_active(mh_profile.dirs[i],d))mask|=dir[i];
 }
 mh_ax[chan]=mh_axis(d,mh_profile.stickx);
 mh_ay[chan]=-mh_axis(d,mh_profile.sticky);
 mh_rx[chan]=mh_axis(d,mh_profile.cstickx);
 mh_ry[chan]=-mh_axis(d,mh_profile.csticky);
 if(mh_ax[chan]>60)mask|=PAD_BUTTON_RIGHT;
 if(mh_ax[chan]< -60)mask|=PAD_BUTTON_LEFT;
 if(mh_ay[chan]>60)mask|=PAD_BUTTON_UP;
 if(mh_ay[chan]< -60)mask|=PAD_BUTTON_DOWN;
 mh_report_buttons[chan]=mask;
}
static s32 mh_report_cb(s32 result,void *unused){
 (void)unused;
 if(!mh_running||mh_fd<0)return 0;
 if(result>=0){
  mh_report_count++;
  if(mh_auto_active)mh_last_report_id=mh_auto.has_id?mh_report[0]:0;
  int chan=0,accept=1;
  switch(mh_profile.multiin){
   case 1: /* Single-controller filter on report[0], per Nintendont. */
    accept=(mh_report[0]==(u8)mh_profile.multivalue);
    break;
   case 2: /* Report[0] contains controller number. */
   case 4: /* Independent reports per controller; Nintendont MultiIn=4. */
    if(mh_report[0]<1 || mh_report[0]>4 ||
       (mh_profile.multiin==4 && mh_report[0]>mh_profile.multivalue) ||
       (mh_profile.multiin==2 && mh_report[0]<mh_profile.multivalue))
     accept=0;
    else chan=mh_report[0]-1;
    break;
   case 0:
   case 3:
   default: break;
  }
  if(accept){
   mh_live[chan]=1;
   mh_decode(mh_report,chan);
  }
  if(USB_ReadIntrMsgAsync(mh_fd,mh_ep,mh_len,mh_report,mh_report_cb,NULL)<0){
   mh_read_errors++;mh_dead=1;
  }
 }else{
  for(int i=0;i<4;i++){mh_live[i]=0;mh_report_buttons[i]=0;}
  mh_dead=1;mh_read_errors++;
 }
 return 0;
}
static void mh_attach(void){
 usb_device_entry list[32] ATTRIBUTE_ALIGN(32);u8 count=0;
 if(USB_GetDeviceList(list,32,USB_CLASS_HID,&count)<0)return;
 for(int i=0;i<count;i++){
  mh_seen_vid=list[i].vid;mh_seen_pid=list[i].pid;
  int known=mh_load_profile(list[i].vid,list[i].pid);
  mh_auto_active=0;
  memset(&mh_auto,0,sizeof(mh_auto));
  s32 fd=-1;
  if(USB_OpenDevice(list[i].device_id,list[i].vid,list[i].pid,&fd)<0||fd<0)continue;
  usb_devdesc desc;int ok=0;memset(&desc,0,sizeof(desc));
  if(USB_GetDescriptors(fd,&desc)>=0){
   for(int c=0;c<desc.bNumConfigurations&&!ok;c++){
    usb_configurationdesc*cfg=&desc.configurations[c];
    for(int j=0;j<cfg->bNumInterfaces&&!ok;j++){
     usb_interfacedesc*it=&cfg->interfaces[j];
     if(it->bInterfaceClass!=USB_CLASS_HID)continue;
     /* Do not mistake a keyboard/mouse for a controller. For devices
      * without a controller.ini, require an actual gamepad HID descriptor. */
     if(!known){
      u8 descriptor[512] ATTRIBUTE_ALIGN(32);
      usb_hiddesc hid;
      memset(&hid,0,sizeof(hid));
      if(USB_GetHIDDescriptor(fd,it->bInterfaceNumber,&hid,sizeof(hid))<0)
       continue;
      int size=hid.descr[0].wDescriptorLength;
      if(hid.descr[0].bDescriptorType!=USB_DT_REPORT ||
         size<=0||size>(int)sizeof(descriptor))continue;
      /* Both current libogc and libogc2 expose the generic descriptor
       * accessor; the newer report-specific helpers are not in libogc2. */
      if(USB_GetGenericDescriptor(fd,USB_DT_REPORT,0,it->bInterfaceNumber,
                                  descriptor,(u32)size)<0)continue;
      if(!mzr_parse(descriptor,(size_t)size,&mh_auto))continue;
      mh_auto_active=1;
     }
     for(int k=0;k<it->bNumEndpoints;k++){
      usb_endpointdesc*e=&it->endpoints[k];
      if((e->bEndpointAddress&0x80)&&(e->bmAttributes&3)==3){
       mh_ep=e->bEndpointAddress;mh_len=e->wMaxPacketSize;
       if(mh_len>sizeof(mh_report)||mh_len==0)continue;
       if(known&&mh_len<7)continue;
       ok=1;break;
      }
     }
    }
   }
   USB_FreeDescriptors(&desc);
  }
  /* Nintendont MultiIn=4 needs <=32 byte reports; no guessing on
   * larger packets which would overlap its 32-byte logical pad slots. */
  if(!known && !mh_auto_active)ok=0;
  if(known&&mh_profile.multiin==4 &&
     (mh_len>32 || mh_profile.multivalue<1))ok=0;
  if(!ok){mh_auto_active=0;USB_CloseDevice(&fd);continue;}
  mh_fd=fd;mh_vid=list[i].vid;mh_pid=list[i].pid;mh_dead=0;
  mh_profile_source=known?1:(mh_auto_active?2:0);
  for(int j=0;j<4;j++){mh_live[j]=0;mh_report_buttons[j]=0;
   mh_buttons[j]=0;mh_previous[j]=0;}
  if(USB_ReadIntrMsgAsync(fd,mh_ep,mh_len,mh_report,mh_report_cb,NULL)<0){
   mh_read_errors++;mh_fd=-1;mh_auto_active=0;USB_CloseDevice(&fd);
   continue;
  }
  break;
 }
}
static void maizo_hid_tick(void){
 static int initialized=0;mh_frame++;
 if(!initialized){USB_Initialize();initialized=1;mh_running=1;}
 if(mh_dead&&mh_fd>=0){
  s32 fd=mh_fd;mh_fd=-1;mh_dead=0;
  for(int i=0;i<4;i++){mh_live[i]=0;mh_report_buttons[i]=0;}
  USB_CloseDevice(&fd);
  mh_auto_active=0;
 }
 for(int i=0;i<4;i++){
  mh_previous[i]=mh_buttons[i];
  mh_buttons[i]=mh_live[i]?mh_report_buttons[i]:0;
 }
 if(mh_fd<0&&(mh_frame%120==1))mh_attach();
}
static int maizo_hid_connected(int chan){return chan>=0&&chan<4&&mh_live[chan];}
static u16 maizo_hid_held(int chan){return maizo_hid_connected(chan)?(u16)mh_buttons[chan]:0;}
static u16 maizo_hid_down(int chan){return maizo_hid_connected(chan)?(u16)(mh_buttons[chan]&~mh_previous[chan]):0;}
static u16 maizo_hid_up(int chan){return chan>=0&&chan<4?(u16)(mh_previous[chan]&~mh_buttons[chan]):0;}
static s8 maizo_hid_stickX(int chan,s8 native){return maizo_hid_connected(chan)?mh_ax[chan]:native;}
static s8 maizo_hid_stickY(int chan,s8 native){return maizo_hid_connected(chan)?mh_ay[chan]:native;}
static s8 maizo_hid_substickX(int chan,s8 native){return maizo_hid_connected(chan)?mh_rx[chan]:native;}
static s8 maizo_hid_substickY(int chan,s8 native){return maizo_hid_connected(chan)?mh_ry[chan]:native;}

/* Every DOL owns its own USB stack; avoid leaving async HID reads alive when
 * an application shuts down or replaces its IOS. */
static void maizo_hid_shutdown(void){
 mh_running=0;mh_dead=0;
 s32 fd=mh_fd;mh_fd=-1;
 for(int i=0;i<4;i++){
  mh_live[i]=0;mh_report_buttons[i]=0;
  mh_buttons[i]=mh_previous[i]=0;
 }
 if(fd>=0)USB_CloseDevice(&fd);
 mh_auto_active=0;
 USB_Deinitialize();
}

/* Explicit reinitialization after a deliberate IOS reload by an installer.
 * This is not an IOS-resident HID service; USB is opened afresh under the
 * newly selected IOS, with its own compatibility limitations. */
static void maizo_hid_resume(void){
 USB_Initialize();
 mh_fd=-1;mh_dead=0;mh_auto_active=0;mh_running=1;mh_frame=0;
}
