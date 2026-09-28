/* =====================================================================
 *  CẤU HÌNH FIRMWARE — sửa file này trước khi nạp
 *  (Chân màn hình TFT nằm trong platformio.ini, mục build_flags)
 * ===================================================================== */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ---------- WiFi ---------- */
#define WIFI_SSID        "Ten_WiFi_Nha_Ban"
#define WIFI_PASSWORD    "Mat_Khau_WiFi"

/* ---------- Cloud server (Render) ---------- */
/* Địa chỉ web service trên Render, KHÔNG có dấu "/" ở cuối */
#define SERVER_URL       "https://ten-app-cua-ban.onrender.com"
/* Khoá thiết bị: lấy trên web dashboard → "Thêm thiết bị" (chỉ hiện 1 lần) */
#define DEVICE_KEY       "dev_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

/* ---------- Web nội bộ trên ESP32 (dùng trong LAN, kể cả khi mất Internet) ---------- */
#define MDNS_NAME        "tuoicay"      /* truy cập http://tuoicay.local */
#define LOCAL_WEB_USER   "admin"        /* để "" nếu không cần mật khẩu */
#define LOCAL_WEB_PASS   "12345678"

/* ---------- Chân cảm biến / chấp hành ----------
 * Màn hình TFT đang dùng: MOSI 23, SCLK 18, MISO 19, CS 15, DC 2, RST 4
 * → các chân dưới đây KHÔNG được trùng các chân đó. */
#define PIN_DHT          16     /* DATA của DHT11 */
#define PIN_SOIL         34     /* AO cảm biến đất — BẮT BUỘC chân ADC1 (32..39) */
#define PIN_RELAY        26     /* IN của module relay */
#define PIN_BUTTON       27     /* nút nhấn nối xuống GND (không bắt buộc) */

/* ---------- Đồng hồ thời gian thực DS1302 ----------
 * DS1302 là NGUỒN GIỜ DUY NHẤT của hệ thống, dù có WiFi hay không:
 * màn hình, lịch sử đo, nhật ký bơm đều lấy giờ từ DS1302 (giữ bằng pin CR2032).
 * Đặt giờ lần đầu: nút "Đặt giờ" trên web nội bộ, hoặc gõ qua Serial Monitor:
 *     SETTIME 2026-09-29 01:45:00      (giờ Việt Nam) */
#define PIN_RTC_CLK      25     /* CLK  */
#define PIN_RTC_DAT      33     /* DAT (I/O) */
#define PIN_RTC_RST      32     /* RST (CE)  */

/* Tự chỉnh DS1302 theo giờ của server khi có mạng:
 *   1 = chỉ ghi lại DS1302 khi lệch quá TIME_CORRECT_THRESHOLD_SEC giây
 *       (hoặc khi DS1302 chưa có giờ). Giờ vẫn luôn được ĐỌC từ DS1302.
 *   0 = không bao giờ tự chỉnh, chỉ đặt giờ bằng tay. */
#define TIME_AUTO_CORRECT          1
#define TIME_CORRECT_THRESHOLD_SEC 5
#define RTC_RESYNC_MS              10000   /* đọc lại DS1302 mỗi 10 giây */
#define TIMEZONE_OFFSET_SEC        (7 * 3600)   /* giờ Việt Nam = UTC+7 */

/* Module relay 5V phổ biến kích mức THẤP → 1. Nếu bơm chạy ngược thì đổi thành 0 */
#define RELAY_ACTIVE_LOW 1

/* ---------- Hiệu chuẩn cảm biến độ ẩm đất ----------
 * Xem "ADC" trên màn hình TFT: để cảm biến ngoài không khí → SOIL_RAW_DRY,
 * nhúng vào cốc nước → SOIL_RAW_WET. */
#define SOIL_RAW_DRY     3200   /* khô = 0%   */
#define SOIL_RAW_WET     1300   /* ướt = 100% */

/* ---------- Cài đặt mặc định (lần nạp đầu; sau đó chỉnh trên web) ---------- */
#define DEFAULT_SOIL_LOW      35    /* %  : đất dưới ngưỡng → BẬT bơm       */
#define DEFAULT_SOIL_HIGH     60    /* %  : đất đạt ngưỡng → TẮT bơm         */
#define DEFAULT_MAX_PUMP_SEC  60    /* s  : bơm tối đa mỗi lần (ngắt an toàn) */
#define DEFAULT_COOLDOWN_SEC  300   /* s  : nghỉ giữa 2 lần tưới tự động      */

/* ---------- Chu kỳ ---------- */
#define SENSOR_INTERVAL_MS    2000    /* đọc cảm biến (DHT11 cần >= 1 s)      */
#define DISPLAY_INTERVAL_MS   500     /* vẽ lại màn hình                      */
#define SYNC_INTERVAL_MS      5000    /* đồng bộ cloud → nhận lệnh <= 5 s     */
#define LOG_INTERVAL_MS       60000   /* 1 bản ghi lịch sử mỗi phút           */
#define LOG_QUEUE_SIZE        240     /* giữ tối đa 4 giờ dữ liệu khi mất mạng */
#define EVENT_QUEUE_SIZE      40

/* ---------- Màn hình ---------- */
#define DISPLAY_ROTATION      1       /* 1 hoặc 3 = nằm ngang 320x240 */

#define FIRMWARE_VERSION      "1.2.0"

#endif /* APP_CONFIG_H */
