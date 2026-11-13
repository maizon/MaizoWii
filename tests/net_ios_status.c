#include <assert.h>
#include "maizonet_net_ios_status.h"
int main(void){
 assert(mz_ios_connect_done(0));
 assert(mz_ios_connect_done(-30));
 assert(!mz_ios_connect_done(-26));
 assert(!mz_ios_connect_done(-14));
 assert(mz_ios_connect_pending(-26));
 assert(mz_ios_connect_pending(-7));
 assert(mz_ios_connect_pending(-6));
 assert(mz_ios_connect_pending(-27));
 assert(!mz_ios_connect_pending(-14));
 assert(!mz_ios_connect_pending(-56));
 assert(mz_ios_io_retryable(-6));
 assert(!mz_ios_io_retryable(-56));
 assert(!mz_ios_io_retryable(0));
 return 0;
}
