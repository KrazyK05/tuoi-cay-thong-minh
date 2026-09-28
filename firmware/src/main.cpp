/* =====================================================================
 *  HỆ THỐNG TƯỚI CÂY THÔNG MINH IoT — CHƯƠNG TRÌNH CHÍNH
 *
 *  File này chỉ "nối dây" các thư viện trong lib/ với nhau:
 *
 *   lib/irrigation_logic  (C thuần)  thuật toán AUTO / MANUAL / ngắt an toàn
 *   lib/data_queue        (C thuần)  hàng đợi dữ liệu khi mất mạng
 *   lib/app_state                    trạng thái dùng chung + khoá giữa 2 nhân
 *   lib/storage                      lưu cài đặt vào NVS (mất điện không mất)
 *   lib/sensors                      DHT11 + cảm biến độ ẩm đất
 *   lib/pump                         relay / máy bơm
 *   lib/button                       nút nhấn tại chỗ
 *   lib/display                      màn hình TFT ILI9341 320x240
 *   lib/net                          WiFi, mDNS
 *   lib/time_utils        (C thuần)  đổi ngày giờ <-> epoch, mã BCD
 *   lib/rtc_ds1302        (C)        driver đồng hồ DS1302
 *   lib/timekeeper        (C)        giờ hệ thống luôn lấy từ DS1302 (có/không WiFi)
 *   lib/cloud                        đồng bộ server Render (Core 0)
 *   lib/local_web                    web nội bộ http://iotfarm.local
 *
 *  Core 1 (loop): cảm biến → điều khiển bơm → nút → web nội bộ → màn hình
 *                 luôn chạy thời gian thực, KHÔNG phụ thuộc Internet.
 *  Core 0 (cloud task): gửi/nhận dữ liệu với server mỗi 5 giây.
 * ===================================================================== */
#include <Arduino.h>
#include "app_config.h"
#include "app_state.h"
#include "sensors.h"
#include "pump.h"
#include "button.h"
#include "display.h"
#include "net.h"
#include "timekeeper.h"
#include "time_utils.h"
#include "cloud.h"
#include "local_web.h"

static uint32_t s_last_sensor_ms  = 0;
static uint32_t s_last_display_ms = 0;
static uint32_t s_last_log_ms     = 0;
static uint32_t s_last_print_ms   = 0;

static void read_sensors(void)
{
    readings_t r;
    sensors_read(&r);
    state_set_readings(&r);
}

static void handle_button(void)
{
    switch (button_poll()) {
        case BTN_SHORT:                         /* nhấn ngắn: bật/tắt bơm (tự chuyển MANUAL) */
            state_local_toggle_pump();
            Serial.println("[BTN] Nhan ngan: dao trang thai bom");
            break;
        case BTN_LONG:                          /* giữ 2 giây: về AUTO */
            state_local_set_mode(true);
            Serial.println("[BTN] Giu 2s: chuyen ve AUTO");
            break;
        default:
            break;
    }
}

/* Lệnh qua Serial Monitor (chọn "Newline" hoặc "Both NL & CR"):
 *   SETTIME 2026-09-29 01:45:00   → đặt giờ DS1302 (giờ Việt Nam)
 *   TIME                          → xem giờ hiện tại */
static void handle_serial_command(void)
{
    static char line[48];
    static uint8_t len = 0;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c != '\n' && c != '\r') {
            if (len < sizeof(line) - 1) line[len++] = c;
            continue;
        }
        if (len == 0) continue;
        line[len] = '\0';
        len = 0;

        datetime_t dt;
        if (strncmp(line, "SETTIME ", 8) == 0) {
            if (tu_parse_datetime(line + 8, &dt)) {
                uint32_t local = tu_datetime_to_epoch(&dt);
                timekeeper_set_epoch(local - TIMEZONE_OFFSET_SEC, "Serial");
            } else {
                Serial.println("[CMD] Sai dinh dang. VD: SETTIME 2026-09-29 01:45:00");
            }
        } else if (strcmp(line, "TIME") == 0) {
            time_t now = time(NULL);
            tu_epoch_to_datetime((uint32_t)now + TIMEZONE_OFFSET_SEC, &dt);
            Serial.printf("[CMD] %04u-%02u-%02u %02u:%02u:%02u (gio VN) | %s\n", dt.year, dt.month, dt.day,
                          dt.hour, dt.minute, dt.second, timekeeper_status_name());
        } else {
            Serial.println("[CMD] Lenh: SETTIME YYYY-MM-DD HH:MM:SS | TIME");
        }
    }
}

static void refresh_display(void)
{
    app_snapshot_t s;
    net_status_t n;
    state_get_snapshot(&s);
    net_get_status(&n);
    n.cloud_ok = cloud_is_ok();
    n.rtc_ok   = timekeeper_rtc_ok();
    display_update(&s, &n);
}

static void print_status(void)
{
    app_snapshot_t s;
    state_get_snapshot(&s);
    Serial.printf("[DATA] T=%.1fC H=%.0f%% Dat=%d%% (ADC %d%s) Bom=%s %s | cloud=%s | gio=%s | cho gui %u\n",
                  s.readings.temperature, s.readings.humidity, s.readings.soil_percent,
                  s.readings.soil_raw, s.readings.soil_ok ? "" : " LOI",
                  s.pump.on ? "BAT" : "TAT", s.settings.auto_mode ? "AUTO" : "MANUAL",
                  cloud_is_ok() ? "OK" : "--", timekeeper_status_name(), s.pending_logs);
}

void setup(void)
{
    pump_init();                 /* relay TẮT ngay lập tức, trước mọi thứ khác */
    Serial.begin(115200);
    delay(100);
    Serial.println("\n=== IoTFarm - HE THONG GIAM SAT CAY TRONG v" FIRMWARE_VERSION " ===");

    state_init();                /* đọc cài đặt đã lưu trong NVS */
    timekeeper_init();           /* giờ hệ thống lấy từ DS1302 */
    display_init();
    display_boot("Dang khoi dong...", WIFI_SSID);

    sensors_init();
    button_init();
    net_init();
    local_web_init();

    /* Chờ WiFi tối đa 8 giây để màn hình có IP; quá thời gian vẫn chạy tiếp (tưới offline) */
    for (int i = 0; i < 80 && !net_wifi_ok(); i++) delay(100);
    display_boot(net_wifi_ok() ? "Da ket noi WiFi" : "Chua co WiFi - van tuoi offline", "");
    delay(600);

    read_sensors();
    cloud_start();               /* Core 0 */
    display_draw_layout();
    refresh_display();
}

void loop(void)
{
    uint32_t now = millis();

    if (now - s_last_sensor_ms >= SENSOR_INTERVAL_MS) {
        s_last_sensor_ms = now;
        read_sensors();
    }

    pump_control();
    handle_button();
    local_web_loop();
    net_loop();
    timekeeper_loop();           /* đọc lại DS1302, chỉnh giờ nếu lệch nhiều so với server */
    handle_serial_command();

    if (now - s_last_display_ms >= DISPLAY_INTERVAL_MS) {
        s_last_display_ms = now;
        refresh_display();
    }
    if (s_last_log_ms == 0 || now - s_last_log_ms >= LOG_INTERVAL_MS) {
        s_last_log_ms = now;
        state_push_log();
    }
    if (now - s_last_print_ms >= 10000) {
        s_last_print_ms = now;
        print_status();
    }
    delay(2);
}
