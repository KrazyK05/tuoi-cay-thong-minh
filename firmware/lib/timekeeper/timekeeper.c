/* =====================================================================
 *  timekeeper.c — DS1302 là nguồn giờ duy nhất (C)
 * ===================================================================== */
#include <Arduino.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include "app_config.h"
#include "timekeeper.h"
#include "time_utils.h"
#include "ds1302.h"

#define FAILS_BEFORE_ERROR 3          /* lỗi liên tiếp 3 lần mới báo "RTC LOI" */
#define MIN_VALID_EPOCH    1704067200UL   /* 01/01/2024 — giờ cũ hơn coi như chưa đặt */

static bool     s_valid   = false;    /* đã có giờ hợp lệ */
static bool     s_rtc_ok  = false;    /* lần đọc DS1302 gần nhất thành công */
static uint8_t  s_fails   = 0;
static uint32_t s_last_sync_ms = 0;

/* Giờ tham chiếu từ server (Core 0 ghi, Core 1 đọc) */
static portMUX_TYPE      s_ref_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool     s_ref_pending = false;
static uint32_t          s_ref_epoch = 0;
static uint32_t          s_ref_ms = 0;

static void print_local(const char *prefix, uint32_t epoch_utc)
{
    datetime_t dt;
    tu_epoch_to_datetime(epoch_utc + TIMEZONE_OFFSET_SEC, &dt);
    printf("%s %04u-%02u-%02u %02u:%02u:%02u (gio VN)\n", prefix,
           dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
}

static void set_system_time(uint32_t epoch_utc)
{
    struct timeval tv;
    tv.tv_sec  = (time_t)epoch_utc;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
}

/* Đọc DS1302 2 lần liên tiếp, chỉ chấp nhận khi 2 kết quả khớp nhau (±1 s)
 * → loại bỏ lần đọc sai do nhiễu hoặc dây lỏng. */
static bool read_rtc_checked(uint32_t *epoch_out)
{
    datetime_t a, b;
    uint32_t ea, eb;
    if (!ds1302_read(&a) || !ds1302_read(&b)) return false;
    ea = tu_datetime_to_epoch(&a);
    eb = tu_datetime_to_epoch(&b);
    if (tu_abs_diff(ea, eb) > 1 || eb < MIN_VALID_EPOCH) return false;
    *epoch_out = eb;
    return true;
}

/* Đọc DS1302 → giờ hệ thống. Đọc lỗi thì GIỮ giờ đang chạy, không làm mất giờ. */
static void sync_from_rtc(void)
{
    uint32_t e;
    if (read_rtc_checked(&e)) {
        set_system_time(e);
        if (!s_rtc_ok && s_valid) printf("[RTC] DS1302 hoat dong lai\n");
        s_valid  = true;
        s_rtc_ok = true;
        s_fails  = 0;
    } else {
        if (s_fails < 255) s_fails++;
        if (s_fails >= FAILS_BEFORE_ERROR && s_rtc_ok) {
            s_rtc_ok = false;
            printf("[RTC] LOI: khong doc duoc DS1302 (kiem tra day CLK/DAT/RST, pin CR2032)\n");
        }
    }
}

void timekeeper_init(void)
{
    char tz[16];
    int hours = TIMEZONE_OFFSET_SEC / 3600;
    uint32_t e;

    /* Múi giờ cho localtime(): POSIX ghi ngược dấu, VD UTC+7 → "<+07>-7" */
    snprintf(tz, sizeof(tz), "<%+03d>%d", hours, -hours);
    setenv("TZ", tz, 1);
    tzset();

    ds1302_init(PIN_RTC_CLK, PIN_RTC_DAT, PIN_RTC_RST);
    if (read_rtc_checked(&e)) {
        set_system_time(e);
        s_valid = s_rtc_ok = true;
        print_local("[RTC] Gio tu DS1302:", e);
    } else {
        s_fails = FAILS_BEFORE_ERROR;
        printf("[RTC] DS1302 chua co gio hoac khong phan hoi.\n"
               "      Dat gio: nut 'Dat gio' tren web noi bo, hoac go: SETTIME YYYY-MM-DD HH:MM:SS\n");
    }
    s_last_sync_ms = millis();
}

bool timekeeper_set_epoch(uint32_t epoch_utc, const char *who)
{
    datetime_t dt;
    uint32_t check;
    tu_epoch_to_datetime(epoch_utc, &dt);
    if (epoch_utc < MIN_VALID_EPOCH || !tu_datetime_valid(&dt)) return false;

    ds1302_write(&dt);
    set_system_time(epoch_utc);
    s_valid = true;
    s_rtc_ok = read_rtc_checked(&check) && tu_abs_diff(check, epoch_utc) <= 2;
    s_fails = s_rtc_ok ? 0 : FAILS_BEFORE_ERROR;

    print_local(s_rtc_ok ? "[RTC] Da dat gio DS1302:" : "[RTC] LOI ghi DS1302, tam dung gio ESP32:", epoch_utc);
    printf("      (nguon: %s)\n", who ? who : "?");
    return s_rtc_ok;
}

void timekeeper_submit_reference(uint32_t epoch_utc)
{
    portENTER_CRITICAL(&s_ref_mux);
    s_ref_epoch   = epoch_utc;
    s_ref_ms      = millis();
    s_ref_pending = true;
    portEXIT_CRITICAL(&s_ref_mux);
}

void timekeeper_loop(void)
{
    uint32_t now_ms = millis();

    if (now_ms - s_last_sync_ms >= RTC_RESYNC_MS) {
        s_last_sync_ms = now_ms;
        sync_from_rtc();
    }

    if (s_ref_pending) {
        uint32_t ref_epoch, ref_ms;
        portENTER_CRITICAL(&s_ref_mux);
        ref_epoch = s_ref_epoch;
        ref_ms    = s_ref_ms;
        s_ref_pending = false;
        portEXIT_CRITICAL(&s_ref_mux);

#if TIME_AUTO_CORRECT
        {
            uint32_t ref_now = ref_epoch + (millis() - ref_ms) / 1000UL;
            uint32_t cur = (uint32_t)time(NULL);
            if (!s_valid || tu_abs_diff(cur, ref_now) > TIME_CORRECT_THRESHOLD_SEC) {
                if (s_valid) printf("[RTC] DS1302 lech %lu giay so voi server -> chinh lai\n",
                                    (unsigned long)tu_abs_diff(cur, ref_now));
                timekeeper_set_epoch(ref_now, "server");
            }
        }
#else
        (void)ref_epoch;
        (void)ref_ms;
#endif
    }
}

bool timekeeper_time_valid(void) { return s_valid; }
bool timekeeper_rtc_ok(void)     { return s_rtc_ok; }

const char *timekeeper_status_name(void)
{
    if (!s_valid)  return "CHUA CO GIO";
    if (!s_rtc_ok) return "RTC LOI";
    return "RTC";
}
