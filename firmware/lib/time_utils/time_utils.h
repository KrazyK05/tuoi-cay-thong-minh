/* =====================================================================
 *  time_utils — TIỆN ÍCH NGÀY GIỜ (C thuần, không phụ thuộc Arduino)
 *  Đổi qua lại giữa giờ UNIX (epoch, giây từ 1/1/1970 UTC) và ngày/giờ,
 *  và mã BCD mà chip DS1302 dùng để lưu số. Test được trên máy tính.
 * ===================================================================== */
#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t year;      /* 2000..2099 */
    uint8_t  month;     /* 1..12 */
    uint8_t  day;       /* 1..31 */
    uint8_t  hour;      /* 0..23 */
    uint8_t  minute;    /* 0..59 */
    uint8_t  second;    /* 0..59 */
    uint8_t  weekday;   /* 1 = Thứ Hai … 7 = Chủ Nhật (ISO) */
} datetime_t;

uint8_t  tu_bcd_to_dec(uint8_t bcd);
uint8_t  tu_dec_to_bcd(uint8_t dec);

bool     tu_is_leap_year(uint16_t year);
uint8_t  tu_days_in_month(uint16_t year, uint8_t month);

/* Kiểm tra ngày giờ hợp lệ trong khoảng 2000..2099 (khoảng DS1302 lưu được) */
bool     tu_datetime_valid(const datetime_t *dt);

/* Ngày giờ (UTC) → epoch. dt phải hợp lệ. */
uint32_t tu_datetime_to_epoch(const datetime_t *dt);

/* Epoch → ngày giờ (UTC), có tính thứ trong tuần */
void     tu_epoch_to_datetime(uint32_t epoch, datetime_t *out);

/* Đọc chuỗi "YYYY-MM-DD HH:MM:SS" (hoặc có chữ T ở giữa). Trả false nếu sai định dạng / ngày không tồn tại. */
bool     tu_parse_datetime(const char *text, datetime_t *out);

/* |a - b| cho số không dấu */
uint32_t tu_abs_diff(uint32_t a, uint32_t b);

#ifdef __cplusplus
}
#endif

#endif /* TIME_UTILS_H */
