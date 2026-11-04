/*
 * MAIZO Wii USB HID splitter / MultiIn packet routing.
 * Portable reference-compatible packet layout with Nintendont's
 * MultiIn=0/1/2/3/4 conventions (FIX94/Nintendont kernel/HID.c and
 * PADReadGC.c). GPL-2.0-or-later.
 *
 * This is report routing, NOT identification of proprietary HID formats.
 * Every returned slice is bounds-checked against the ACTUAL interrupt
 * transfer length, not an advertised endpoint packet size.
 */
#ifndef MAIZO_MULTI_H
#define MAIZO_MULTI_H
#include <stddef.h>
#include <stdint.h>
#define MAIZO_MULTI_PORTS 4
typedef struct {
 uint8_t port;
 uint16_t offset;
 uint16_t length;
} MaizoReportSlice;
typedef struct {
 MaizoReportSlice slice[MAIZO_MULTI_PORTS];
 unsigned count;
} MaizoReportRouting;
/* Returns 1 iff routing mode / packet is admissible.
 * mode 1: only process reports matching first byte value
 * mode 2: first byte selects logical port; value is first valid index
 * mode 3: contiguous multi-pad message, value = bytes per pad
 * mode 4: one packet per pad, first byte 1..value, packet size <=32
 */
static int maizo_route_reports(unsigned mode,unsigned value,
                               const uint8_t *packet,size_t length,
                               MaizoReportRouting*out) {
 if(!packet||!out||!length||length>128)return 0;
 MaizoReportRouting r={0};
 unsigned port=0;
 switch(mode){
  case 0:
   break;
  case 1:
   if(value>255 || packet[0]!=value)return 0;
   break;
  case 2:
   if(value>4||packet[0]<1||packet[0]<value||packet[0]>4)return 0;
   port=(unsigned)packet[0]-1;
   break;
  case 3: {
   /* Nintendont MultiInValue is STRIDE in BYTES, NOT pad count. */
   if(value==0||value>length)return 0;
   unsigned count=(unsigned)(length/value);
   if(!count)return 0;
   if(count>MAIZO_MULTI_PORTS)count=MAIZO_MULTI_PORTS;
   /* Ignore incomplete trailing bytes and any pad beyond port 4. */
   for(unsigned i=0;i<count;i++){
    r.slice[i].port=(uint8_t)i;
    r.slice[i].offset=(uint16_t)(i*value);
    r.slice[i].length=(uint16_t)value;
   }
   r.count=count;*out=r;return 1;
  }
  case 4:
   if(value<1||value>4||length>32 ||
      packet[0]<1||packet[0]>value)return 0;
   port=(unsigned)packet[0]-1;
   break;
  default:return 0;
 }
 r.slice[0].port=(uint8_t)port;
 r.slice[0].offset=0;
 r.slice[0].length=(uint16_t)length;
 r.count=1;
 *out=r;
 return 1;
}
#endif
