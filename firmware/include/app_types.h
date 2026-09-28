/* =====================================================================
 *  KIỂU DỮ LIỆU DÙNG CHUNG — viết bằng C thuần, dùng được cả trong .c và .cpp
 * ===================================================================== */
#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Cấu hình điều khiển — lưu trong NVS, đồng bộ 2 chiều với cloud */
typedef struct {
    bool     auto_mode;      /* true = AUTO, false = MANUAL              */
    bool     manual_pump;    /* bơm mong muốn khi ở MANUAL                */
    uint8_t  soil_low;       /* % : dưới ngưỡng này thì bật bơm (AUTO)    */
    uint8_t  soil_high;      /* % : đạt ngưỡng này thì tắt bơm (AUTO)     */
    uint16_t max_pump_sec;   /* s : bơm tối đa mỗi lần                    */
    uint32_t cooldown_sec;   /* s : nghỉ tối thiểu giữa 2 lần tưới AUTO   */
    int32_t  ver;            /* phiên bản cấu hình (so với cloud)         */
} settings_t;

/* Kết quả đọc cảm biến */
typedef struct {
    float temperature;       /* °C, NAN nếu chưa đọc được */
    float humidity;          /* %,  NAN nếu chưa đọc được */
    int   soil_percent;      /* 0..100 */
    int   soil_raw;          /* 0..4095 (ADC 12 bit) */
    bool  soil_ok;           /* false = cảm biến hở / chập */
} readings_t;

/* Ai / cái gì đã bật tắt bơm — gửi lên cloud để ghi nhật ký */
typedef enum {
    SRC_AUTO = 0,            /* thuật toán tự động            */
    SRC_MANUAL,              /* lệnh thủ công từ web cloud    */
    SRC_LOCAL,               /* nút nhấn / web nội bộ ESP32   */
    SRC_SAFETY               /* ngắt an toàn do chạy quá lâu  */
} pump_source_t;

/* Trạng thái máy bơm */
typedef struct {
    bool     on;
    uint32_t started_ms;     /* millis() lúc bật     */
    uint32_t stopped_ms;     /* millis() lúc tắt gần nhất */
    bool     ever_run;       /* đã từng chạy từ lúc khởi động chưa */
} pump_state_t;

/* 1 bản ghi lịch sử (gửi lên cloud) */
typedef struct {
    uint32_t captured_ms;
    float    t;
    float    h;
    int16_t  soil;           /* -1 nếu cảm biến lỗi */
    bool     pump;
} log_item_t;

/* 1 sự kiện bật/tắt bơm */
typedef struct {
    uint32_t captured_ms;
    bool     on;
    uint8_t  src;            /* pump_source_t */
} event_item_t;

/* Ảnh chụp trạng thái để hiển thị (màn hình, web nội bộ) */
typedef struct {
    settings_t   settings;
    readings_t   readings;
    pump_state_t pump;
    uint32_t     pump_run_sec;       /* đã chạy bao lâu (nếu đang bật) */
    uint32_t     cooldown_left_sec;  /* còn phải nghỉ bao lâu (AUTO)   */
    uint16_t     pending_logs;       /* bản ghi chờ gửi lên cloud      */
} app_snapshot_t;

/* Trạng thái mạng */
typedef struct {
    bool wifi_ok;
    bool cloud_ok;
    bool time_ok;
    char ip[16];
    int  rssi;
    bool rtc_ok;             /* DS1302 đọc được */
    int  hour;               /* giờ Việt Nam, -1 nếu chưa có giờ */
    int  minute;
} net_status_t;

#ifdef __cplusplus
}
#endif

#endif /* APP_TYPES_H */
