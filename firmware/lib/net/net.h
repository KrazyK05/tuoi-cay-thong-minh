/* =====================================================================
 *  net — WiFi (tự kết nối lại), mDNS (tuoicay.local)
 * ===================================================================== */
#ifndef NET_H
#define NET_H

#include "app_types.h"

void     net_init(void);
void     net_loop(void);               /* gọi trong loop(): tự kết nối lại khi rớt WiFi */
bool     net_wifi_ok(void);
uint32_t net_epoch(void);              /* giờ UNIX (UTC) của hệ thống (= giờ DS1302); 0 nếu chưa có giờ */
void     net_get_status(net_status_t *out);   /* điền wifi_ok, time_ok, ip, rssi, giờ:phút (cloud_ok để false) */

#endif /* NET_H */
