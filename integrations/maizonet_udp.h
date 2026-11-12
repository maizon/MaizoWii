#ifndef MAIZONET_UDP_H
#define MAIZONET_UDP_H
/* MaizoNet UDP virtual-disc wire format, protocol v1.
 * Fixed MTU-safe frames, retry/CRC, READ ONLY, no complete ISO download.
 * The portable client is shared between native tests and Nintendont ARM.
 * This is a LAN-only protocol, NOT authenticated or suitable for the WAN.
 */
#include <stdint.h>
#define MZUDP_PAYLOAD 1024u
#define MZUDP_BLOCK 8192u
#define MZUDP_MAX_FRAG 8u
#define MZUDP_REQ_H 26u
#define MZUDP_RES_H 44u
typedef struct {
 int (*transmit)(void*,const void*,unsigned);
 int (*receive)(void*,void*,unsigned,unsigned timeout_ms);
 void *user;
} MaizoUdpTransport;
typedef struct {
 MaizoUdpTransport io;
 uint8_t tx[MZUDP_REQ_H+256];
 uint8_t rx[MZUDP_RES_H+MZUDP_PAYLOAD];
 uint32_t token,request;
 uint64_t size;
 int opened,error;
} MaizoUdp;
int maizonet_udp_open(MaizoUdp*,MaizoUdpTransport,const char *relative_path);
int maizonet_udp_read(MaizoUdp*,uint64_t offset,void *buffer,unsigned size);
void maizonet_udp_close(MaizoUdp*);
#endif
