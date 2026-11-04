/*
 * MaizoWii: portable USB HID report-descriptor decoder, GPL-2.0-or-later.
 * No Wii-specific headers; production and host tests compile exactly this code.
 * Handles standard Game Pad / Joystick HID INPUT variable items (buttons,
 * X/Y/Rx/Ry, Hat Switch, D-pad) with report IDs and bit-packed values.
 * Other HID usages, proprietary protocols and HID arrays are NOT guessed.
 */
#ifndef MAIZO_REPORT_H
#define MAIZO_REPORT_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define MZR_FIELDS 64
#define MZR_REPORTS 16
typedef struct {
 uint16_t bit;
 int32_t minimum, maximum;
 uint16_t page, usage;
 uint8_t size, id;
} MzrField;
typedef struct {
 MzrField fields[MZR_FIELDS];
 uint16_t bits[MZR_REPORTS];
 uint8_t count, gamepad, has_id;
} MzrMap;
typedef struct {
 uint16_t buttons;
 int8_t x,y,rx,ry;
 uint8_t up,down,left,right;
} MzrState;
typedef struct {
 uint32_t page;
 int32_t min,max;
 uint16_t size,count;
 uint8_t id;
} MzrGlobal;
static int32_t mzr_signed(uint32_t v,unsigned width) {
 if(width==0)return 0;
 if(width>=32)return (int32_t)v;
 if(v & (1u<<(width-1)))return (int32_t)(v | ~( (1u<<width)-1u ));
 return (int32_t)v;
}
static unsigned mzr_shortval(const uint8_t *p,unsigned n) {
 uint32_t v=0;
 for(unsigned i=0;i<n;i++)v|=(uint32_t)p[i]<<(8*i);
 return v;
}
static int mzr_add(MzrMap*m,uint16_t bit,uint8_t size,uint8_t id,
                   uint16_t page,uint16_t usage,int32_t min,int32_t max){
 if(m->count>=MZR_FIELDS||size==0||size>16||id>=MZR_REPORTS)return 0;
 MzrField*f=&m->fields[m->count++];
 f->bit=bit;f->size=size;f->id=id;f->page=page;
 f->usage=usage;f->minimum=min;f->maximum=max;
 return 1;
}
static int mzr_parse(const uint8_t*descriptor,size_t length,MzrMap*out){
 if(!descriptor||!out||length==0||length>4096)return 0;
 MzrMap map;memset(&map,0,sizeof(map));
 MzrGlobal g={0,0,0,0,0,0},saved[4];
 int sp=0,depth=0,joydepth=0,top_gamepad=0;
 uint32_t usages[32];int ucount=0;
 uint32_t umin=0,umax=0;int have_min=0,have_max=0;
 size_t pos=0;
 while(pos<length){
  uint8_t prefix=descriptor[pos++];
  if(prefix==0xfe){
   if(pos+2>length)return 0;
   unsigned count=descriptor[pos];pos+=2;
   if(count>length-pos)return 0;
   pos+=count;continue;
  }
  unsigned n=prefix&3u;n=n==3?4:n;
  unsigned type=(prefix>>2)&3u,tag=(prefix>>4)&15u;
  if(n>length-pos)return 0;
  uint32_t val=mzr_shortval(descriptor+pos,n);pos+=n;
  if(type==1){
   switch(tag){
    case 0:g.page=val;break;
    case 1:g.min=mzr_signed(val,n*8);break;
    case 2:g.max=g.min<0?mzr_signed(val,n*8):(int32_t)val;break;
    case 7:g.size=(uint16_t)val;break;
    case 8:if(!val||val>=MZR_REPORTS)return 0;g.id=(uint8_t)val;map.has_id=1;break;
    case 9:g.count=(uint16_t)val;break;
    case 10:if(sp>=4)return 0;saved[sp++]=g;break;
    case 11:if(!sp)return 0;g=saved[--sp];break;
    default:break;
   }
  }else if(type==2){
   if(tag==0){if(ucount<32)usages[ucount++]=val;}
   if(tag==1){umin=val;have_min=1;}
   if(tag==2){umax=val;have_max=1;}
  }else if(type==0){
   if(tag==10){
    depth++;
    uint32_t first=ucount?usages[0]:have_min?umin:0;
    unsigned page=(first>>16)?(first>>16):g.page;
    unsigned usage=first&0xffffu;
    if(!joydepth && page==1 && (usage==4||usage==5||usage==8)
       && (val&0xffu)==1){joydepth=depth;top_gamepad=1;}
   }else if(tag==12){
    if(depth==joydepth)joydepth=0;
    if(depth>0)depth--;
   }else if(tag==8){
    if(!g.size||!g.count||g.count>128 || g.size>32 || g.count>512/g.size ||
       g.id>=MZR_REPORTS)return 0;
    uint32_t start=map.bits[g.id];
    uint32_t total=(uint32_t)g.size*g.count;
    if(total>512u-start)return 0;
    if(joydepth && (val&0x01u)==0 && (val&0x02u)!=0) {
     for(unsigned i=0;i<g.count;i++){
      uint32_t raw=(i<(unsigned)ucount)?usages[i]:
                   have_min?umin+i:0;
      if(have_max&&have_min&&raw>umax)raw=0;
      unsigned page=(raw>>16)?raw>>16:g.page;
      unsigned usage=raw&0xffffu;
      if((page==9&&usage>=1&&usage<=16)||
         (page==1&&(usage==0x30||usage==0x31||usage==0x33||
                    usage==0x34||usage==0x39|| (usage>=0x90&&usage<=0x93)))){
       if(!mzr_add(&map,(uint16_t)(start+i*g.size),(uint8_t)g.size,g.id,
                   (uint16_t)page,(uint16_t)usage,g.min,g.max))return 0;
      }
     }
    }
    map.bits[g.id]=(uint16_t)(start+total);
   }
   ucount=0;have_min=0;have_max=0;
  }
 }
 if(depth!=0 || !top_gamepad)return 0;
 int buttons=0,axis=0;
 for(unsigned i=0;i<map.count;i++){
  const MzrField*f=&map.fields[i];
  if(f->page==9)buttons++;
  if(f->page==1&&(f->usage==0x30||f->usage==0x31||
                  f->usage==0x39))axis++;
 }
 if(buttons<2||axis<1)return 0;
 map.gamepad=1;
 *out=map;return 1;
}
static int mzr_bits(const uint8_t*buf,size_t len,unsigned bit,
                    unsigned width,uint32_t*out){
 if(width==0||width>16||bit>len*8||width>len*8-bit)return 0;
 uint32_t v=0;
 for(unsigned i=0;i<width;i++)
  if(buf[(bit+i)/8]&(1u<<((bit+i)%8)))v|=1u<<i;
 *out=v;return 1;
}
static int8_t mzr_scale(int32_t value,int32_t lo,int32_t hi){
 if(hi<=lo)return 0;
 if(value<lo)value=lo;
 if(value>hi)value=hi;
 int64_t s=((int64_t)(value-lo)*192)/(hi-lo)-96;
 return s<-96?-96:s>96?96:(int8_t)s;
}
static int mzr_decode(const MzrMap*m,const uint8_t*data,size_t len,
                      MzrState*out){
 if(!m||!data||!out||!m->gamepad||len==0)return 0;
 MzrState s;memset(&s,0,sizeof(s));
 uint8_t id=m->has_id?data[0]:0;
 unsigned base=m->has_id?8:0;int matched=0;
 for(unsigned i=0;i<m->count;i++){
  const MzrField*f=&m->fields[i];
  if(f->id!=id)continue;
  uint32_t raw;
  if(!mzr_bits(data,len,base+f->bit,f->size,&raw))return 0;
  int32_t value=f->minimum<0?mzr_signed(raw,f->size):(int32_t)raw;
  matched++;
  if(f->page==9){if(value)s.buttons|=(uint16_t)(1u<<(f->usage-1));continue;}
  if(f->page!=1)continue;
  switch(f->usage){
   case 0x30:s.x=mzr_scale(value,f->minimum,f->maximum);break;
   case 0x31:s.y=mzr_scale(value,f->minimum,f->maximum);break;
   case 0x33:s.rx=mzr_scale(value,f->minimum,f->maximum);break;
   case 0x34:s.ry=mzr_scale(value,f->minimum,f->maximum);break;
   case 0x90:s.up=!!value;break;
   case 0x91:s.down=!!value;break;
   case 0x92:s.right=!!value;break;
   case 0x93:s.left=!!value;break;
   case 0x39: {
     int v=value-f->minimum;
     if(v>=0&&v<=7){
      if(v==0||v==1||v==7)s.up=1;
      if(v>=1&&v<=3)s.right=1;
      if(v>=3&&v<=5)s.down=1;
      if(v>=5&&v<=7)s.left=1;
     }
     break;
   }
  }
 }
 if(!matched)return 0;
 *out=s;return 1;
}
#endif
