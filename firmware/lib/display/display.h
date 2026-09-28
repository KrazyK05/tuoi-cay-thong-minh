/* =====================================================================
 *  display — MÀN HÌNH TFT ILI9341 320x240 (thư viện TFT_eSPI)
 *
 *  Bố cục (nằm ngang):
 *   ┌───────────────────────────────────────────────┐
 *   │ TUOI CAY THONG MINH     ● WiFi ● Cloud  23:45 │  thanh tiêu đề
 *   ├───────────────┬───────────────┬───────────────┤
 *   │ NHIET DO      │ DO AM KK      │ DO AM DAT     │  3 ô số liệu
 *   │ 29.4 °C       │ 69 %          │ 47 %  [====|] │
 *   ├───────────────┴───────────────┴───────────────┤
 *   │ DANG BOM                             [ AUTO ] │  khung máy bơm
 *   │ Bat < 35%   Tat >= 60%                        │
 *   │ Da chay 12s / toi da 60s                      │
 *   ├───────────────────────────────────────────────┤
 *   │ IP 192.168.1.50                   Cloud: OK   │  chân trang
 *   └───────────────────────────────────────────────┘
 *  Chỉ vẽ lại phần thay đổi → không nhấp nháy.
 *  Font có sẵn của TFT_eSPI không có dấu tiếng Việt nên chữ viết không dấu.
 * ===================================================================== */
#ifndef DISPLAY_H
#define DISPLAY_H

#include "app_types.h"

void display_init(void);

/* Màn hình chờ lúc khởi động */
void display_boot(const char *line1, const char *line2);

/* Vẽ toàn bộ giao diện chính lần đầu (gọi 1 lần sau khi khởi động xong) */
void display_draw_layout(void);

/* Cập nhật số liệu — gọi định kỳ (mỗi DISPLAY_INTERVAL_MS) */
void display_update(const app_snapshot_t *s, const net_status_t *n);

#endif /* DISPLAY_H */
