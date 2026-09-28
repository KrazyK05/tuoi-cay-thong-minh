/* =====================================================================
 *  timekeeper — GIỜ HỆ THỐNG LẤY TỪ DS1302 (C)
 *
 *  DS1302 là nguồn giờ DUY NHẤT, dù có WiFi hay không:
 *   - Khởi động và cứ mỗi RTC_RESYNC_MS: đọc DS1302 (đọc 2 lần đối chiếu
 *     để loại lần đọc lỗi do nhiễu / lỏng dây) → đặt làm giờ hệ thống.
 *     Nhờ vậy time(), localtime() ở mọi module đều chính là giờ DS1302.
 *   - Không dùng NTP. Khi có mạng, giờ server chỉ dùng để CHỈNH LẠI
 *     DS1302 nếu lệch quá ngưỡng (tắt được bằng TIME_AUTO_CORRECT = 0).
 *   - Đặt giờ bằng tay: web nội bộ hoặc lệnh SETTIME qua Serial.
 *
 *  Chỉ Core 1 (loop) được giao tiếp với chip DS1302. Core 0 (cloud) chỉ
 *  gửi giờ tham chiếu qua timekeeper_submit_reference(), loop sẽ xử lý.
 * ===================================================================== */
#ifndef TIMEKEEPER_H
#define TIMEKEEPER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void timekeeper_init(void);          /* gọi sớm trong setup() */
void timekeeper_loop(void);          /* gọi trong loop() (Core 1) */

/* Đặt giờ bằng tay (giờ UNIX, UTC). Ghi DS1302 + giờ hệ thống. Chỉ gọi từ Core 1. */
bool timekeeper_set_epoch(uint32_t epoch_utc, const char *who);

/* Giờ tham chiếu từ server (gọi được từ Core 0). Loop sẽ quyết định có chỉnh DS1302 không. */
void timekeeper_submit_reference(uint32_t epoch_utc);

bool timekeeper_time_valid(void);    /* đã có giờ hợp lệ (từ DS1302 hoặc vừa đặt) */
bool timekeeper_rtc_ok(void);        /* lần đọc DS1302 gần nhất thành công */
const char *timekeeper_status_name(void);   /* "RTC" | "RTC LOI" | "CHUA CO GIO" */

#ifdef __cplusplus
}
#endif

#endif /* TIMEKEEPER_H */
