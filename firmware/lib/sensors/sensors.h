/* =====================================================================
 *  sensors — ĐỌC DHT11 (nhiệt độ, độ ẩm không khí) + CẢM BIẾN ĐỘ ẨM ĐẤT
 * ===================================================================== */
#ifndef SENSORS_H
#define SENSORS_H

#include "app_types.h"

void sensors_init(void);

/* Đọc tất cả cảm biến vào *out.
 * DHT11 thỉnh thoảng đọc lỗi → giữ giá trị hợp lệ gần nhất thay vì trả NAN. */
void sensors_read(readings_t *out);

#endif /* SENSORS_H */
