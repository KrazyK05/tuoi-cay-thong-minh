/* =====================================================================
 *  time_utils.c — C thuần
 * ===================================================================== */
#include "time_utils.h"

uint8_t tu_bcd_to_dec(uint8_t bcd)
{
    return (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0F));
}

uint8_t tu_dec_to_bcd(uint8_t dec)
{
    return (uint8_t)(((dec / 10) << 4) | (dec % 10));
}

bool tu_is_leap_year(uint16_t year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

uint8_t tu_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 1 || month > 12) return 0;
    if (month == 2 && tu_is_leap_year(year)) return 29;
    return days[month - 1];
}

bool tu_datetime_valid(const datetime_t *dt)
{
    return dt->year >= 2000 && dt->year <= 2099 &&
           dt->month >= 1 && dt->month <= 12 &&
           dt->day >= 1 && dt->day <= tu_days_in_month(dt->year, dt->month) &&
           dt->hour <= 23 && dt->minute <= 59 && dt->second <= 59;
}

/* Số ngày từ 1/1/1970 đến ngày y-m-d (thuật toán "days from civil" của H. Hinnant) */
static int32_t days_from_civil(int32_t y, uint32_t m, uint32_t d)
{
    int32_t  era;
    uint32_t yoe, doy, doe;
    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (uint32_t)(y - era * 400);
    doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int32_t)doe - 719468;
}

uint32_t tu_datetime_to_epoch(const datetime_t *dt)
{
    int32_t days = days_from_civil(dt->year, dt->month, dt->day);
    return (uint32_t)days * 86400UL + dt->hour * 3600UL + dt->minute * 60UL + dt->second;
}

void tu_epoch_to_datetime(uint32_t epoch, datetime_t *out)
{
    uint32_t days = epoch / 86400UL, rem = epoch % 86400UL;
    /* "civil from days" (H. Hinnant) */
    int32_t  z   = (int32_t)days + 719468;
    int32_t  era = z / 146097;
    uint32_t doe = (uint32_t)(z - era * 146097);
    uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int32_t  y   = (int32_t)yoe + era * 400;
    uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    uint32_t mp  = (5 * doy + 2) / 153;
    uint32_t d   = doy - (153 * mp + 2) / 5 + 1;
    uint32_t m   = mp < 10 ? mp + 3 : mp - 9;

    out->year    = (uint16_t)(y + (m <= 2));
    out->month   = (uint8_t)m;
    out->day     = (uint8_t)d;
    out->hour    = (uint8_t)(rem / 3600);
    out->minute  = (uint8_t)((rem % 3600) / 60);
    out->second  = (uint8_t)(rem % 60);
    out->weekday = (uint8_t)((days + 3) % 7 + 1);   /* 1/1/1970 là Thứ Năm (4) */
}

/* Đọc đúng n chữ số tại *p, tiến con trỏ. Trả -1 nếu không đủ chữ số. */
static int read_digits(const char **p, int n)
{
    int v = 0;
    for (int i = 0; i < n; i++) {
        char c = (*p)[i];
        if (c < '0' || c > '9') return -1;
        v = v * 10 + (c - '0');
    }
    *p += n;
    return v;
}

static bool expect_char(const char **p, char c1, char c2)
{
    if (**p != c1 && **p != c2) return false;
    (*p)++;
    return true;
}

bool tu_parse_datetime(const char *text, datetime_t *out)
{
    const char *p = text;
    int y, mo, d, h, mi, se;
    datetime_t dt;
    while (*p == ' ') p++;
    if ((y  = read_digits(&p, 4)) < 0 || !expect_char(&p, '-', '/')) return false;
    if ((mo = read_digits(&p, 2)) < 0 || !expect_char(&p, '-', '/')) return false;
    if ((d  = read_digits(&p, 2)) < 0 || !expect_char(&p, ' ', 'T')) return false;
    if ((h  = read_digits(&p, 2)) < 0 || !expect_char(&p, ':', ':')) return false;
    if ((mi = read_digits(&p, 2)) < 0 || !expect_char(&p, ':', ':')) return false;
    if ((se = read_digits(&p, 2)) < 0) return false;
    while (*p == ' ' || *p == '\r' || *p == '\n') p++;
    if (*p != '\0') return false;

    dt.year = (uint16_t)y; dt.month = (uint8_t)mo; dt.day = (uint8_t)d;
    dt.hour = (uint8_t)h;  dt.minute = (uint8_t)mi; dt.second = (uint8_t)se;
    dt.weekday = 0;
    if (!tu_datetime_valid(&dt)) return false;
    tu_epoch_to_datetime(tu_datetime_to_epoch(&dt), out);   /* điền luôn thứ trong tuần */
    return true;
}

uint32_t tu_abs_diff(uint32_t a, uint32_t b)
{
    return a > b ? a - b : b - a;
}
