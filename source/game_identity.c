/* Non-destructive header identification; no ROM scans, hashes,
 * network, or engine changes. Disc IDs are validated against the file,
 * not inferred solely from a renamed USB/SD folder.
 */
#include "game_identity.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <strings.h>
static int read_at(FILE*f,long off,void*dest,size_t size){
 return fseek(f,off,SEEK_SET)==0&&fread(dest,1,size,f)==size;
}
static int id6(const unsigned char*s){
 for(int i=0;i<6;i++)if(!(s[i]>='A'&&s[i]<='Z')&&
                          !(s[i]>='0'&&s[i]<='9'))return 0;
 return 1;
}
static int text(const unsigned char*s,size_t count,char*dst,size_t cap){
 if(!dst||cap<2)return 0;
 size_t n=0;
 for(size_t i=0;i<count&&n+1<cap;i++){
  unsigned char c=s[i];
  if(c==0)break;
  if(c<32||c>126)return 0;
  dst[n++]=(char)c;
 }
 while(n&&dst[n-1]==' ')n--;
 dst[n]=0;return n>1;
}
int maizo_game_probe_identity(const char *path,int system,
                             char *product,size_t product_cap,
                             char *internal,size_t internal_cap){
 if(product&&product_cap)product[0]=0;
 if(internal&&internal_cap)internal[0]=0;
 if(!path||!product||product_cap<7||!internal||internal_cap<25)return 0;
 FILE*f=fopen(path,"rb");if(!f)return 0;
 int valid=0;
 if(system==0||system==1){
  /* Wii WBFS uses embedded disc offset 0x200; ISO/GCM at offset 0.
   * CISO has a different container header and is not blindly parsed. */
  const char*ext=strrchr(path,'.');
  long ofs=0;
  if(system==1&&ext&&!strcasecmp(ext,".ciso")){
   /* Nintendont CISO uses a 0x8000-byte header/map. The first
    * physical data block starts at 0x8000 only when map[0] is present.
    * Never interpret the ASCII "CISO" header as a GameCube disc ID. */
   unsigned char ciso[9]={0};
   if(!read_at(f,0,ciso,sizeof(ciso))||
      memcmp(ciso,"CISO",4)||ciso[8]!=1)goto end;
   unsigned block=(unsigned)ciso[4]|((unsigned)ciso[5]<<8)|
                  ((unsigned)ciso[6]<<16)|((unsigned)ciso[7]<<24);
   if(block<0x8000u||block>0x800000u)goto end;
   ofs=0x8000L;
  }
  if(system==0&&ext&&!strcasecmp(ext,".wbfs")){
   char magic[4];if(!read_at(f,0,magic,4)||memcmp(magic,"WBFS",4))goto end;
   ofs=0x200L;
  }
  unsigned char header[0x80]={0};
  if(!read_at(f,ofs,header,sizeof(header)))goto end;
  if(!id6(header))goto end;
  /* Wii/GC native disc headers carry a manufacturer/game-region code.
   * Trust 6 bytes only if the platform magic at offset 0x18 is present. */
  unsigned magic_offset=system==1?0x1c:0x18;
  uint32_t magic=((uint32_t)header[magic_offset]<<24)|
                 ((uint32_t)header[magic_offset+1]<<16)|
                 ((uint32_t)header[magic_offset+2]<<8)|
                 header[magic_offset+3];
  if(system==0&&magic!=0x5D1C9EA3u)goto end;
  if(system==1&&magic!=0xC2339F3Du)goto end;
  if(product_cap<7)goto end;
  memcpy(product,header,6);product[6]=0;
  (void)text(header+0x20,0x60,internal,internal_cap);
  valid=1;
 }else if(system==5){
  unsigned char h[0xc0];
  if(!read_at(f,0,h,sizeof(h))||h[0xb2]!=0x96)goto end;
  for(int i=0;i<4;i++)
   if(!((h[0xac+i]>='A'&&h[0xac+i]<='Z')||
        (h[0xac+i]>='0'&&h[0xac+i]<='9')))goto end;
  memcpy(product,h+0xac,4);product[4]=0;
  (void)text(h+0xa0,12,internal,internal_cap);
  valid=1;
 }else if(system==6){
  unsigned char h[0x150];
  if(!read_at(f,0,h,sizeof(h)) || h[0x104]!=0xCE ||
     h[0x105]!=0xED||h[0x106]!=0x66||h[0x107]!=0x66)goto end;
  (void)text(h+0x134,(h[0x143]&0x80)?11:16,internal,internal_cap);
  valid=1;
 }else if(system==3){
  /* SNES LoROM/HiROM headers (optional 512-byte copier prefix).
   * No globally unique SNES ID exists: internal title is advisory only. */
  long size=0;
  if(fseek(f,0,SEEK_END)!=0||(size=ftell(f))<0x8000L)goto end;
  long prefix=(size%0x8000L==512)?512:0;
  const long regions[]={0x7fc0L,0xffc0L};
  for(unsigned i=0;i<2;i++){
   unsigned char h[32]={0};
   if(size<regions[i]+prefix+32 ||
      !read_at(f,regions[i]+prefix,h,32))continue;
   unsigned complement=(unsigned)h[28]|((unsigned)h[29]<<8);
   unsigned checksum=(unsigned)h[30]|((unsigned)h[31]<<8);
   if((complement^checksum)!=0xffffu)continue;
   if(text(h,21,internal,internal_cap)){valid=1;break;}
  }
 }
end:
 fclose(f);
 return valid;
}
