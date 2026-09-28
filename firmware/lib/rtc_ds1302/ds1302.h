/* =====================================================================
 *  ds1302 — DRIVER CHIP ĐỒNG HỒ DS1302 (viết bằng C)
 *
 *  Giao tiếp 3 dây: CLK, DAT (I/O), RST (còn gọi CE).
 *  Đồng hồ trong DS1302 được lưu theo giờ UTC (quy ước của project này);
 *  phần hiển thị tự cộng +7 để ra giờ Việt Nam.
 * ===================================================================== */
#ifndef DS1302_H
#define DS1302_H

#include <stdint.h>
#include <stdbool.h>
#include "time_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

void ds1302_init(uint8_t pin_clk, uint8_t pin_dat, uint8_t pin_rst);

/* Đọc ngày giờ. Trả false nếu đồng hồ đang dừng (chip mới / hết pin)
 * hoặc dữ liệu không hợp lệ (chưa nối dây, nối sai chân). */
bool ds1302_read(datetime_t *out);

/* Ghi ngày giờ và cho đồng hồ chạy (xoá cờ dừng, bỏ chống ghi). */
void ds1302_write(const datetime_t *dt);

#ifdef __cplusplus
}
#endif

#endif /* DS1302_H */
