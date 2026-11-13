#ifndef MAIZONET_SMB1_H
#define MAIZONET_SMB1_H
/* Portable, read-only SMB1/NT1 transport for MaizoNet.
 * Wire protocol is LITTLE endian on both Wii PPC and Nintendont ARM.
 * Guest-only: no password authentication, signing or Internet exposure.
 * All IO has to be bounded by the transport's timeouts.
 */
#include <stddef.h>
#include <stdint.h>
/* PS2 OPL uses 8-KiB SMB1 receive limits to fit constrained I/O
 * processor TCP windows. Bound Wii remote disc SMB packets similarly. */
#define MAIZONET_SMB1_MAX_IO 8192u
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
 /* Last handshake stage: 1=negotiate, 2=session, 3=tree. */
 uint8_t last_phase,last_reason,last_command;
 /* Keep raw response identity for kernel telemetry on header mismatches. */
 uint8_t last_received_command;
 uint16_t expected_mid,received_mid;
 uint32_t last_received_magic;
} MaizoNetSmb1;
int maizonet_smb1_connect_guest(MaizoNetSmb1*,MaizoNetTransport,
                                const char *ip,const char *share);
int maizonet_smb1_open(MaizoNetSmb1*,const char *path);
int maizonet_smb1_read_at(MaizoNetSmb1*,uint64_t offset,void *buf,unsigned length);
void maizonet_smb1_close_file(MaizoNetSmb1*);
void maizonet_smb1_close(MaizoNetSmb1*);
const char *maizonet_smb1_error(uint32_t ntstatus);
typedef struct {
 char name[128];
 unsigned char is_dir;
} MaizoNetDirEntry;
/* Returns bounded entry count or -1 for unsupported/malformed SMB response. */
int maizonet_smb1_list_dir(MaizoNetSmb1*,const char*relative_dir,
                            MaizoNetDirEntry*,unsigned capacity);
#endif
