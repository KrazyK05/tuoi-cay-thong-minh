/* =====================================================================
 *  ds1302.c — driver DS1302 bằng C (dùng các hàm GPIO C của core ESP32)
 *
 *  Giao thức (datasheet DS1302):
 *   - Kéo RST lên cao để bắt đầu, xuống thấp để kết thúc.
 *   - Mỗi byte truyền bit THẤP trước (LSB first).
 *   - Ghi: đặt bit lên DAT rồi tạo sườn LÊN của CLK (chip đọc bit lúc này).
 *   - Đọc: chip đặt bit lên DAT sau sườn XUỐNG của CLK.
 *   - Lệnh 0xBE / 0xBF = ghi / đọc liên tiếp 8 thanh ghi đồng hồ (burst),
 *     tránh việc giây nhảy sang phút giữa 2 lần đọc riêng lẻ.
 * ===================================================================== */
#include <Arduino.h>
#include "ds1302.h"

#define CMD_CLOCK_BURST_WRITE 0xBE
#define CMD_CLOCK_BURST_READ  0xBF
#define CMD_WRITE_PROTECT     0x8E
#define READ_FLAG             0x01

#define SEC_CLOCK_HALT        0x80   /* bit 7 thanh ghi giây: 1 = đồng hồ dừng */
#define HOUR_12H_MODE         0x80   /* bit 7 thanh ghi giờ: 1 = chế độ 12 giờ */

static uint8_t s_clk, s_dat, s_rst;

static void begin_transfer(void)
{
    digitalWrite(s_rst, LOW);
    digitalWrite(s_clk, LOW);
    pinMode(s_dat, OUTPUT);
    digitalWrite(s_rst, HIGH);
    delayMicroseconds(4);                 /* tCC >= 4 us */
}

static void end_transfer(void)
{
    digitalWrite(s_rst, LOW);
    delayMicroseconds(4);                 /* tCWH >= 4 us */
}

/* Gửi 1 byte. Nếu là lệnh đọc thì nhả chân DAT trước sườn xuống cuối cùng,
 * vì ngay sau đó chip sẽ bắt đầu xuất dữ liệu ra DAT. */
static void write_byte(uint8_t value, bool is_read_command)
{
    for (uint8_t bit = 0; bit < 8; bit++) {
        digitalWrite(s_dat, value & 0x01);
        delayMicroseconds(1);
        digitalWrite(s_clk, HIGH);
        delayMicroseconds(1);
        if (bit == 7 && is_read_command) pinMode(s_dat, INPUT);
        digitalWrite(s_clk, LOW);
        delayMicroseconds(1);
        value >>= 1;
    }
}

static uint8_t read_byte(void)
{
    uint8_t value = 0;
    for (uint8_t bit = 0; bit < 8; bit++) {
        value |= (uint8_t)(digitalRead(s_dat) << bit);
        digitalWrite(s_clk, HIGH);
        delayMicroseconds(1);
        digitalWrite(s_clk, LOW);
        delayMicroseconds(1);
    }
    return value;
}

static void write_register(uint8_t cmd, uint8_t value)
{
    begin_transfer();
    write_byte(cmd, false);
    write_byte(value, false);
    end_transfer();
}

void ds1302_init(uint8_t pin_clk, uint8_t pin_dat, uint8_t pin_rst)
{
    s_clk = pin_clk;
    s_dat = pin_dat;
    s_rst = pin_rst;
    digitalWrite(s_rst, LOW);
    pinMode(s_rst, OUTPUT);
    digitalWrite(s_clk, LOW);
    pinMode(s_clk, OUTPUT);
    pinMode(s_dat, INPUT);
}

bool ds1302_read(datetime_t *out)
{
    uint8_t r[8];
    begin_transfer();
    write_byte(CMD_CLOCK_BURST_READ, true);
    for (int i = 0; i < 8; i++) r[i] = read_byte();
    end_transfer();

    if (r[0] & SEC_CLOCK_HALT) return false;          /* đồng hồ đang dừng */
    if (r[2] & HOUR_12H_MODE) return false;           /* project luôn ghi ở chế độ 24 giờ */

    out->second  = tu_bcd_to_dec(r[0] & 0x7F);
    out->minute  = tu_bcd_to_dec(r[1] & 0x7F);
    out->hour    = tu_bcd_to_dec(r[2] & 0x3F);
    out->day     = tu_bcd_to_dec(r[3] & 0x3F);
    out->month   = tu_bcd_to_dec(r[4] & 0x1F);
    out->weekday = r[5] & 0x07;
    out->year    = (uint16_t)(2000 + tu_bcd_to_dec(r[6]));
    /* Chưa nối dây thường đọc ra toàn 0x00 hoặc 0xFF → sẽ không hợp lệ */
    return tu_datetime_valid(out);
}

void ds1302_write(const datetime_t *dt)
{
    write_register(CMD_WRITE_PROTECT, 0x00);          /* tắt chống ghi */

    begin_transfer();
    write_byte(CMD_CLOCK_BURST_WRITE, false);
    write_byte(tu_dec_to_bcd(dt->second) & 0x7F, false);   /* bit 7 = 0 → đồng hồ chạy */
    write_byte(tu_dec_to_bcd(dt->minute), false);
    write_byte(tu_dec_to_bcd(dt->hour), false);             /* bit 7 = 0 → 24 giờ */
    write_byte(tu_dec_to_bcd(dt->day), false);
    write_byte(tu_dec_to_bcd(dt->month), false);
    write_byte(dt->weekday >= 1 && dt->weekday <= 7 ? dt->weekday : 1, false);
    write_byte(tu_dec_to_bcd((uint8_t)(dt->year - 2000)), false);
    write_byte(0x00, false);                                 /* thanh ghi WP: vẫn cho ghi */
    end_transfer();
}
