#ifndef MAIZONET_SMB1_H
#define MAIZONET_SMB1_H
/* Portable, read-only SMB1/NT1 transport for MaizoNet.
 * Wire protocol is LITTLE endian on both Wii PPC and Nintendont ARM.
 * Guest-only: no password authentication, signing or Internet exposure.
 * All IO has to be bounded by the transport's timeouts.
 */
#include <stddef.h>
#include <stdint.h>
#define MAIZONET_SMB1_MAX_IO 32768u
#define MAIZONET_SMB1_RX_CAP 36864u
#define MAIZONET_SMB1_TX_CAP 512u
typedef int (*MaizoNetRead)(void *,void *,unsigned);
typedef int (*MaizoNetWrite)(void *,const void *,unsigned);
typedef struct {
 MaizoNetRead receive;
 MaizoNetWrite transmit;
 void *user;
} MaizoNetTransport;
typedef struct {
 MaizoNetTransport io;
 uint8_t rx[MAIZONET_SMB1_RX_CAP+4];
 uint8_t tx[MAIZONET_SMB1_TX_CAP+4];
 unsigned rxlen;
 uint16_t uid,tid,fid,mid;
 uint16_t max_chunk;
 int session,tree,opened;
 uint32_t last_status;
} MaizoNetSmb1;
int maizonet_smb1_connect_guest(MaizoNetSmb1*,MaizoNetTransport,
                                const char *ip,const char *share);
int maizonet_smb1_open(MaizoNetSmb1*,const char *path);
int maizonet_smb1_read_at(MaizoNetSmb1*,uint64_t offset,void *buf,unsigned length);
void maizonet_smb1_close(MaizoNetSmb1*);
const char *maizonet_smb1_error(uint32_t ntstatus);
#endif
