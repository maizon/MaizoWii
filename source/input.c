/*
 * MaizoWii frontend input: same source HID driver as every adapted DOL.
 * USB is owned by this process only; IOS reload/DOL switch starts a new one.
 */
#include "app.h"
#include <wiiuse/wpad.h>
#include <ogc/usb.h>
#include <string.h>
#include "../integrations/maizo_hid.inc"

enum {
 B_UP=1,B_DOWN=2,B_LEFT=4,B_RIGHT=8,B_A=16,B_B=32,
 B_X=64,B_Y=128,B_L=256,B_R=512,B_START=1024,B_BACK=2048
};

void input_hid_diagnostics(HidDiagnostics*out){
 if(!out)return;
 out->vid=mh_fd>=0?mh_vid:mh_seen_vid;
 out->pid=mh_fd>=0?mh_pid:mh_seen_pid;
 out->reports=mh_report_count;
 out->read_errors=mh_read_errors;
 out->connected=mh_live[0]||mh_live[1]||mh_live[2]||mh_live[3];
 out->profile_loaded=mh_profile_source; /* 1=controller.ini; 2=HID descriptor */
 out->last_report_id=mh_last_report_id;
 out->endpoint=mh_ep;out->packet_size=mh_len;
 out->multiin=mh_profile.multiin;
 out->connected_ports=0;
 for(int port=0;port<4;port++){
  out->held[port]=(unsigned)maizo_hid_held(port);
  out->stick_x[port]=mh_ax[port];out->stick_y[port]=mh_ay[port];
  if(mh_live[port])out->connected_ports++;
 }
}
void input_init(void){
 PAD_Init();WPAD_Init();
 for(int i=0;i<4;i++)WPAD_SetDataFormat(i,WPAD_FMT_BTNS_ACC_IR);
 /* Driver initializes USB when maizo_hid_tick is first invoked. */
}
void input_shutdown(void){
 maizo_hid_shutdown();WPAD_Shutdown();
}
static unsigned from_gc(u32 c){
 unsigned m=0;
 if(c&PAD_BUTTON_UP)m|=B_UP;
 if(c&PAD_BUTTON_DOWN)m|=B_DOWN;
 if(c&PAD_BUTTON_LEFT)m|=B_LEFT;
 if(c&PAD_BUTTON_RIGHT)m|=B_RIGHT;
 if(c&PAD_BUTTON_A)m|=B_A;
 if(c&PAD_BUTTON_B)m|=B_B;
 if(c&PAD_BUTTON_X)m|=B_X;
 if(c&PAD_BUTTON_Y)m|=B_Y;
 if(c&PAD_TRIGGER_L)m|=B_L;
 if(c&PAD_TRIGGER_R)m|=B_R;
 if(c&PAD_BUTTON_START)m|=B_START;
 if(c&PAD_TRIGGER_Z)m|=B_BACK;
 return m;
}
static unsigned from_wii(u32 w){
 unsigned m=0;
 if(w&WPAD_BUTTON_UP)m|=B_UP;
 if(w&WPAD_BUTTON_DOWN)m|=B_DOWN;
 if(w&WPAD_BUTTON_LEFT)m|=B_LEFT;
 if(w&WPAD_BUTTON_RIGHT)m|=B_RIGHT;
 if(w&WPAD_BUTTON_A)m|=B_A;
 if(w&WPAD_BUTTON_B)m|=B_B;
 if(w&WPAD_BUTTON_1)m|=B_X;
 if(w&WPAD_BUTTON_2)m|=B_Y;
 if(w&WPAD_BUTTON_MINUS)m|=B_L;
 if(w&WPAD_BUTTON_PLUS)m|=B_R;
 if(w&WPAD_BUTTON_HOME)m|=B_START;
 return m;
}
void input_poll(Input*out){
 if(!out)return;
 memset(out,0,sizeof(*out));
 PAD_ScanPads();WPAD_ScanPads();
 maizo_hid_tick();
 unsigned m=0;
 for(int port=0;port<4;port++){
  m|=from_gc(PAD_ButtonsHeld(port));
  m|=from_wii(WPAD_ButtonsHeld(port));
  m|=from_gc(maizo_hid_held(port));
  if(PAD_StickX(port)<-55)m|=B_LEFT;
  if(PAD_StickX(port)>55)m|=B_RIGHT;
  if(PAD_StickY(port)<-55)m|=B_DOWN;
  if(PAD_StickY(port)>55)m|=B_UP;
 }
 static unsigned previous=0,repeat=0;
 unsigned pressed=m&~previous;
 const unsigned dirs=B_UP|B_DOWN|B_LEFT|B_RIGHT;
 if((m&dirs) && (m&dirs)==(previous&dirs)){
  repeat++;
  if(repeat>=22&&repeat%6==0)pressed|=m&dirs;
 }else repeat=0;
 previous=m;
 out->up=!!(pressed&B_UP);out->down=!!(pressed&B_DOWN);
 out->left=!!(pressed&B_LEFT);out->right=!!(pressed&B_RIGHT);
 out->accept=!!(pressed&B_A);out->back=!!(pressed&B_B);
 out->favorite=!!(pressed&B_Y);out->search=!!(pressed&B_X);
 out->settings=!!(pressed&B_START);out->refresh=!!(pressed&B_BACK);
 out->tab_prev=!!(pressed&B_L);out->tab_next=!!(pressed&B_R);
 out->hid_present=mh_live[0]||mh_live[1]||mh_live[2]||mh_live[3];
}
