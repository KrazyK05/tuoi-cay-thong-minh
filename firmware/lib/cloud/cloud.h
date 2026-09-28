/* =====================================================================
 *  cloud — ĐỒNG BỘ VỚI SERVER RENDER (chạy riêng trên Core 0)
 *
 *  Mỗi SYNC_INTERVAL_MS gửi POST /api/device/sync gồm:
 *   - trạng thái hiện tại + cấu hình (kèm phiên bản ver)
 *   - các bản ghi lịch sử và sự kiện bơm đang chờ
 *  và nhận lại cấu hình mới nhất từ web.
 *  Server chậm / mất mạng KHÔNG ảnh hưởng việc tưới trên Core 1.
 * ===================================================================== */
#ifndef CLOUD_H
#define CLOUD_H

#include <stdbool.h>

void cloud_start(void);     /* tạo task trên Core 0 */
bool cloud_is_ok(void);     /* lần đồng bộ gần nhất thành công? */

#endif /* CLOUD_H */
