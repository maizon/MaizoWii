#ifndef MAIZONET_NET_IPV4_H
#define MAIZONET_NET_IPV4_H
/* IOS SOGetHostID returns an IPv4 address as a raw 32-bit value, not a
 * positive signed integer. In particular 192.168.x.x sets bit 31.
 * Reject zero, multicast/reserved addresses, and negative IOS errno values
 * (typically 0xffffffxx), without interpreting valid IPv4 as an error. */
static inline int maizonet_ipv4_hostid_ok(unsigned int ip){
 return ip != 0u && ip < 0xe0000000u;
}
#endif
