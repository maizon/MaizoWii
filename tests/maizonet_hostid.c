#include "maizonet_net_ipv4.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 assert(maizonet_ipv4_hostid_ok(0xC0A80008u)); /* Wii 192.168.0.8 */
 assert(maizonet_ipv4_hostid_ok(0x0A000002u)); /* 10.0.0.2 */
 assert(maizonet_ipv4_hostid_ok(0xAC100201u)); /* 172.16.2.1 */
 assert(!maizonet_ipv4_hostid_ok(0));
 assert(!maizonet_ipv4_hostid_ok(0xffffffffu));
 assert(!maizonet_ipv4_hostid_ok(0xffffffe3u)); /* IOS -29 */
 assert(!maizonet_ipv4_hostid_ok(0xfffffffdu)); /* IOS -3 */
 puts("PASS IOS hostid sign/regression");
 return 0;
}
