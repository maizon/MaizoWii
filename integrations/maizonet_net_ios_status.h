#ifndef MAIZONET_NET_IOS_STATUS_H
#define MAIZONET_NET_IOS_STATUS_H
/* Raw IOS socket error numbers from libogc/ogc_sockets/soc_common.c.
 * IOS_Ioctl on Nintendont's ARM kernel returns raw values; newlib
 * POSIX errno and libogc's PPC _net_convert_error() are NOT applicable.
 */
#define MZ_IOS_EAGAIN       (-6)
#define MZ_IOS_EALREADY     (-7)
#define MZ_IOS_ECONNREFUSED (-14)
#define MZ_IOS_EINPROGRESS  (-26)
#define MZ_IOS_EINTR        (-27)
#define MZ_IOS_EISCONN      (-30)
#define MZ_IOS_ENOTCONN     (-56)
static inline int mz_ios_connect_done(int code){
 return code==0 || code==MZ_IOS_EISCONN;
}
static inline int mz_ios_connect_pending(int code){
 return code==MZ_IOS_EINPROGRESS || code==MZ_IOS_EALREADY ||
        code==MZ_IOS_EAGAIN || code==MZ_IOS_EINTR;
}
static inline int mz_ios_io_retryable(int code){
 return mz_ios_connect_pending(code);
}
#endif
