/* =====================================================================
 *  pump — ĐIỀU KHIỂN RELAY / MÁY BƠM
 *  Gọi thuật toán trong irrigation_logic rồi bật/tắt relay theo kết quả.
 * ===================================================================== */
#ifndef PUMP_H
#define PUMP_H

#include "app_types.h"

/* Đặt relay về TẮT ngay khi khởi động (gọi SỚM NHẤT trong setup) */
void pump_init(void);

/* Chạy 1 vòng điều khiển — gọi liên tục trong loop() */
void pump_control(void);

#endif /* PUMP_H */
