/* =====================================================================
 *  irrigation_logic — THUẬT TOÁN TƯỚI (C thuần, không phụ thuộc Arduino)
 *
 *  Toàn bộ "bộ não" quyết định bật/tắt bơm nằm ở đây, tách khỏi phần cứng:
 *   - Dễ đọc, dễ sửa thuật toán mà không đụng tới relay / WiFi.
 *   - Test được trên máy tính:  pio test -e native
 * ===================================================================== */
#ifndef IRRIGATION_LOGIC_H
#define IRRIGATION_LOGIC_H

#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Đầu vào cho 1 lần ra quyết định */
typedef struct {
    const settings_t *settings;
    const readings_t *readings;
    bool     pump_on;
    uint32_t run_ms;          /* bơm đã chạy bao lâu (nếu đang bật)          */
    bool     ever_run;        /* đã từng bơm từ lúc khởi động chưa           */
    uint32_t since_stop_ms;   /* đã tắt được bao lâu (dùng cho thời gian nghỉ) */
    pump_source_t manual_source; /* nguồn của lệnh MANUAL hiện tại        */
} irr_input_t;

typedef enum {
    IRR_KEEP = 0,             /* giữ nguyên */
    IRR_TURN_ON,
    IRR_TURN_OFF
} irr_action_t;

typedef struct {
    irr_action_t  action;
    pump_source_t source;     /* lý do, ghi vào nhật ký bơm                  */
    bool          clear_manual; /* true = phải đặt manual_pump = false (ngắt an toàn ở MANUAL) */
} irr_decision_t;

/* Quyết định bật / tắt / giữ nguyên bơm. Hàm thuần: không đọc/ghi gì bên ngoài. */
irr_decision_t irr_decide(const irr_input_t *in);

/* Còn phải nghỉ bao nhiêu giây trước khi AUTO được tưới lại (0 = sẵn sàng) */
uint32_t irr_cooldown_left_sec(const settings_t *s, bool ever_run, bool pump_on, uint32_t since_stop_ms);

/* Đổi giá trị ADC thô sang % độ ẩm (raw_dry = 0 %, raw_wet = 100 %) */
int  irr_soil_percent(int raw, int raw_dry, int raw_wet);

/* ADC sát 0 hoặc sát 4095 thường là dây hở / chập → coi là lỗi */
bool irr_soil_raw_valid(int raw);

/* Kiểm tra bộ cài đặt hợp lệ (ngưỡng bật < ngưỡng tắt, thời gian trong giới hạn) */
bool irr_settings_valid(long soil_low, long soil_high, long max_pump_sec, long cooldown_sec);

/* Cài đặt mặc định */
void irr_default_settings(settings_t *s, int soil_low, int soil_high, int max_pump_sec, long cooldown_sec);

/* Tên nguồn gửi lên server: "auto" | "manual" | "local" | "safety" */
const char *irr_source_name(pump_source_t src);

#ifdef __cplusplus
}
#endif

#endif /* IRRIGATION_LOGIC_H */
