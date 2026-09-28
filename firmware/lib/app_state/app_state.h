/* =====================================================================
 *  app_state — TRẠNG THÁI DÙNG CHUNG GIỮA 2 NHÂN
 *
 *  Core 1 (loop) và Core 0 (cloud) cùng đọc/ghi các biến dưới đây, nên
 *  MỌI truy cập phải nằm giữa state_lock() ... state_unlock().
 *  Giữ khoá càng ngắn càng tốt, KHÔNG gọi mạng / vẽ màn hình khi đang giữ khoá.
 * ===================================================================== */
#ifndef APP_STATE_H
#define APP_STATE_H

#include "app_types.h"
#include "data_queue.h"

/* ---- Biến toàn cục (chỉ truy cập khi đã khoá) ---- */
extern settings_t    g_settings;
extern readings_t    g_readings;
extern pump_state_t  g_pump;
extern pump_source_t g_manual_source;   /* ai ra lệnh MANUAL gần nhất */
extern data_queue_t  g_log_queue;       /* log_item_t   */
extern data_queue_t  g_event_queue;     /* event_item_t */

/* Khởi tạo mutex, hàng đợi, đọc cài đặt từ NVS. Gọi 1 lần đầu setup(). */
void state_init(void);

void state_lock(void);
void state_unlock(void);

/* ---- Các hàm tiện ích: TỰ khoá bên trong, gọi thoải mái ---- */
void state_set_readings(const readings_t *r);
void state_get_snapshot(app_snapshot_t *out);
void state_push_log(void);                       /* chụp 1 bản ghi lịch sử từ số đo hiện tại */

/* Thay đổi tại chỗ (nút nhấn, web nội bộ) — lưu NVS, KHÔNG đổi ver
 * → lần đồng bộ sau cloud sẽ tự nhận cấu hình mới của thiết bị. */
void state_local_toggle_pump(void);              /* AUTO → MANUAL + bật; MANUAL → đảo bơm */
void state_local_set_mode(bool auto_mode);
void state_local_set_pump(bool on);              /* tự chuyển MANUAL */
bool state_local_set_thresholds(long soil_low, long soil_high, long max_pump_sec, long cooldown_sec);

/* Áp dụng cấu hình từ cloud (đã kiểm tra hợp lệ). Trả true nếu có thay đổi. */
bool state_apply_cloud_settings(const settings_t *incoming);

/* Lưu cài đặt hiện tại xuống NVS — gọi khi ĐANG giữ khoá */
void state_save_locked(void);

#endif /* APP_STATE_H */
