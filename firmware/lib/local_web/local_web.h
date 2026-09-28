/* =====================================================================
 *  local_web — WEB SERVER NỘI BỘ TRÊN ESP32 (cổng 80)
 *  http://iotfarm.local hoặc http://<IP>: xem số liệu, đổi chế độ,
 *  bật tắt bơm, sửa ngưỡng — dùng được cả khi mất Internet.
 * ===================================================================== */
#ifndef LOCAL_WEB_H
#define LOCAL_WEB_H

void local_web_init(void);
void local_web_loop(void);   /* gọi trong loop() */

#endif /* LOCAL_WEB_H */
