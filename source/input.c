#include "app.h"
#include <wiiuse/wpad.h>
#include <ogc/usb.h>
#include <string.h>
#include <malloc.h>
/* Scope: known 0079:0006 report layout. Other HID layouts require descriptors/profiles.
 * Poll asynchronously: never issue blocking interrupt reads on the GX/UI thread. */
enum { B_UP=1,B_DOWN=2,B_LEFT=4,B_RIGHT=8,B_A=16,B_B=32,B_X=64,B_Y=128,B_L=256,B_R=512,B_START=1024,B_BACK=2048 };
static s32 hid_fd=-1;static u8 hid_ep=0x81;static u16 hid_len=8;
static unsigned char hid_data[64] ATTRIBUTE_ALIGN(32);
static volatile unsigned hid_mask=0;static volatile int running=0,connected=0;
static u16 active_vid=0,active_pid=0;static unsigned tick=0;
static unsigned decode(const unsigned char*b,int n){
 if(n<7)return 0;
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
 if(result>=0){hid_mask=decode(hid_data,hid_len);connected=1;
  if(USB_ReadIntrMsgAsync(hid_fd,hid_ep,hid_len,hid_data,on_report,NULL)<0)connected=0;
 }else{connected=0;hid_mask=0;}
 return 0;
}
static void search_hid(void){
 if(hid_fd>=0)return;
 usb_device_entry found[32] ATTRIBUTE_ALIGN(32);u8 count=0;
 if(USB_GetDeviceList(found,32,USB_CLASS_HID,&count)<0)return;
 for(int i=0;i<count;i++){
  if(!((found[i].vid==0x0079&&found[i].pid==0x0006)||
       (found[i].vid==0x0810&&found[i].pid==0x0001)))continue;
  s32 fd=-1;if(USB_OpenDevice(found[i].device_id,found[i].vid,found[i].pid,&fd)<0||fd<0)continue;
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
  if(!good){USB_CloseDevice(&fd);continue;}
  hid_fd=fd;active_vid=found[i].vid;active_pid=found[i].pid;
  if(USB_ReadIntrMsgAsync(fd,hid_ep,hid_len,hid_data,on_report,NULL)<0){connected=0;hid_fd=-1;USB_CloseDevice(&fd);}
  return;
 }
}
void input_init(void){PAD_Init();WPAD_Init();WPAD_SetDataFormat(WPAD_CHAN_0,WPAD_FMT_BTNS_ACC_IR);USB_Initialize();running=1;}
void input_shutdown(void){running=0;hid_mask=0;connected=0;if(hid_fd>=0){USB_CloseDevice(&hid_fd);hid_fd=-1;}USB_Deinitialize();WPAD_Shutdown();}
void input_poll(Input*out){
 memset(out,0,sizeof(*out));PAD_ScanPads();WPAD_ScanPads();
 u32 gc=PAD_ButtonsHeld(0),wi=WPAD_ButtonsHeld(0);
 unsigned m=hid_mask;
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
