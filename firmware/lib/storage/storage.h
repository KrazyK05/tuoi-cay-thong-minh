/* =====================================================================
 *  storage — LƯU CÀI ĐẶT VÀO BỘ NHỚ NVS (flash) của ESP32
 *  Mất điện vẫn giữ nguyên chế độ, ngưỡng, phiên bản cấu hình.
 * ===================================================================== */
#ifndef STORAGE_H
#define STORAGE_H

#include "app_types.h"

/* Đọc cài đặt đã lưu. Trả false nếu chưa từng lưu (giữ nguyên giá trị trong *out). */
bool storage_load_settings(settings_t *out);

/* Ghi cài đặt xuống flash */
void storage_save_settings(const settings_t *s);

/* Xoá toàn bộ cài đặt (về mặc định ở lần khởi động sau) */
void storage_clear(void);

#endif /* STORAGE_H */
