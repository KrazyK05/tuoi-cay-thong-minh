/* =====================================================================
 *  display.cpp — giao diện ILI9341 320x240
 *  Muốn đổi màu / vị trí: sửa các hằng số ở đầu file.
 * ===================================================================== */
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "app_config.h"
#include "display.h"

static TFT_eSPI tft;

/* ---------- Màu (RGB565) ---------- */
#define RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define C_BG      RGB(10, 16, 13)
#define C_HEADER  RGB(22, 90, 58)
#define C_CARD    RGB(24, 34, 29)
#define C_BORDER  RGB(48, 62, 54)
#define C_TEXT    RGB(232, 238, 234)
#define C_MUTED   RGB(140, 156, 147)
#define C_GREEN   RGB(63, 181, 122)
#define C_BLUE    RGB(70, 160, 235)
#define C_ORANGE  RGB(235, 150, 60)
#define C_RED     RGB(235, 95, 85)
#define C_TRACK   RGB(44, 56, 50)

/* ---------- Font TFT_eSPI ---------- */
#define F_SMALL   1     /* 8 px  */
#define F_TEXT    2     /* 16 px */
#define F_BIG     4     /* 26 px */

/* ---------- Bố cục ---------- */
#define SCREEN_W  320
#define SCREEN_H  240
#define HEADER_H  26
#define CARD_Y    32
#define CARD_H    86
#define CARD_W    100
#define CARD_GAP  4
#define MARGIN    6
#define PANEL_Y   (CARD_Y + CARD_H + 6)   /* 124 */
#define PANEL_H   78
#define FOOTER_Y  (PANEL_Y + PANEL_H + 6) /* 208 */

static int card_x(int i) { return MARGIN + i * (CARD_W + CARD_GAP); }

/* ---------- Bộ nhớ đệm: chỉ vẽ lại khi nội dung đổi ---------- */
typedef struct {
    char     text[48];
    uint16_t color;
    bool     valid;
} cache_t;

enum {
    K_TIME, K_WIFI, K_CLOUD,
    K_TEMP, K_HUMI, K_SOIL, K_SOILBAR, K_SOILRAW,
    K_PUMP_STATE, K_PUMP_LINE2, K_PUMP_LINE3,
    K_FOOT_L, K_FOOT_R,
    K_COUNT
};
static cache_t s_cache[K_COUNT];

static bool changed(int key, const char *text, uint16_t color)
{
    cache_t *c = &s_cache[key];
    if (c->valid && c->color == color && strcmp(c->text, text) == 0) return false;
    snprintf(c->text, sizeof(c->text), "%s", text);
    c->color = color;
    c->valid = true;
    return true;
}

static void invalidate_all(void)
{
    for (int i = 0; i < K_COUNT; i++) s_cache[i].valid = false;
}

/* Vẽ chữ, xoá trước vùng rộng `pad` px bằng màu nền `bg` */
static void text_at(const char *s, int x, int y, uint8_t font, uint16_t fg, uint16_t bg,
                    uint8_t datum, int pad)
{
    tft.setTextDatum(datum);
    tft.setTextColor(fg, bg);
    tft.setTextPadding(pad);
    tft.drawString(s, x, y, font);
    tft.setTextPadding(0);
}

/* ===================================================================== */
void display_init(void)
{
    tft.init();
    tft.setRotation(DISPLAY_ROTATION);
    tft.fillScreen(C_BG);
    invalidate_all();
}

void display_boot(const char *line1, const char *line2)
{
    tft.fillScreen(C_BG);
    /* Giọt nước */
    tft.fillCircle(SCREEN_W / 2, 92, 18, C_GREEN);
    tft.fillTriangle(SCREEN_W / 2 - 16, 84, SCREEN_W / 2 + 16, 84, SCREEN_W / 2, 52, C_GREEN);
    text_at("TUOI CAY THONG MINH", SCREEN_W / 2, 132, F_BIG, C_TEXT, C_BG, TC_DATUM, 0);
    text_at(line1 ? line1 : "", SCREEN_W / 2, 170, F_TEXT, C_MUTED, C_BG, TC_DATUM, SCREEN_W);
    text_at(line2 ? line2 : "", SCREEN_W / 2, 192, F_TEXT, C_MUTED, C_BG, TC_DATUM, SCREEN_W);
    text_at("v" FIRMWARE_VERSION, SCREEN_W / 2, SCREEN_H - 14, F_SMALL, C_BORDER, C_BG, TC_DATUM, 0);
}

static void draw_card_frame(int i, const char *label)
{
    int x = card_x(i);
    tft.fillRoundRect(x, CARD_Y, CARD_W, CARD_H, 8, C_CARD);
    tft.drawRoundRect(x, CARD_Y, CARD_W, CARD_H, 8, C_BORDER);
    text_at(label, x + 9, CARD_Y + 8, F_TEXT, C_MUTED, C_CARD, TL_DATUM, 0);
}

void display_draw_layout(void)
{
    tft.fillScreen(C_BG);
    invalidate_all();

    /* Thanh tiêu đề */
    tft.fillRect(0, 0, SCREEN_W, HEADER_H, C_HEADER);
    text_at("TUOI CAY THONG MINH", 8, HEADER_H / 2, F_TEXT, C_TEXT, C_HEADER, ML_DATUM, 0);

    /* 3 ô số liệu */
    draw_card_frame(0, "NHIET DO");
    draw_card_frame(1, "DO AM KK");
    draw_card_frame(2, "DO AM DAT");
    text_at("DHT11", card_x(0) + 9, CARD_Y + CARD_H - 13, F_SMALL, C_MUTED, C_CARD, TL_DATUM, 0);
    text_at("DHT11", card_x(1) + 9, CARD_Y + CARD_H - 13, F_SMALL, C_MUTED, C_CARD, TL_DATUM, 0);

    /* Chân trang */
    tft.drawFastHLine(MARGIN, FOOTER_Y - 1, SCREEN_W - 2 * MARGIN, C_BORDER);
}

/* ---------- Thanh tiêu đề: WiFi, Cloud, giờ ---------- */
static void draw_status_dot(int key, int x, const char *label, bool ok)
{
    uint16_t col = ok ? C_GREEN : C_RED;
    if (!changed(key, label, col)) return;
    tft.fillCircle(x, HEADER_H / 2, 4, col);
    text_at(label, x + 8, HEADER_H / 2, F_SMALL, C_TEXT, C_HEADER, ML_DATUM, 0);
}

static void update_header(const net_status_t *n)
{
    char buf[24];
    draw_status_dot(K_WIFI, 178, "WiFi", n->wifi_ok);
    draw_status_dot(K_CLOUD, 222, "Cloud", n->cloud_ok);
    /* Giờ lấy từ DS1302. Cam = DS1302 đang lỗi (tạm dùng đồng hồ ESP32); "--:--" = chưa đặt giờ */
    uint16_t tc = n->rtc_ok ? C_TEXT : C_ORANGE;
    if (n->hour >= 0) snprintf(buf, sizeof(buf), "%02d:%02d", n->hour, n->minute);
    else              snprintf(buf, sizeof(buf), "--:--");
    if (changed(K_TIME, buf, tc)) {
        text_at(buf, SCREEN_W - 8, HEADER_H / 2, F_TEXT, tc, C_HEADER, MR_DATUM, 44);
    }
}

/* ---------- Ô số liệu: giá trị + đơn vị ---------- */
static void draw_value(int key, int card, const char *value, const char *unit, bool degree, uint16_t color)
{
    char k[24];
    snprintf(k, sizeof(k), "%s%s", value, unit);
    if (!changed(key, k, color)) return;

    int x = card_x(card) + 9, y = CARD_Y + 30;
    tft.fillRect(x, y, CARD_W - 14, 28, C_CARD);            /* xoá vùng giá trị cũ */
    text_at(value, x, y, F_BIG, color, C_CARD, TL_DATUM, 0);
    int ux = x + tft.textWidth(value, F_BIG) + 3;
    if (degree) {
        tft.drawCircle(ux + 2, y + 5, 2, C_MUTED);
        ux += 7;
    }
    text_at(unit, ux, y + 4, F_TEXT, C_MUTED, C_CARD, TL_DATUM, 0);
}

static void update_cards(const app_snapshot_t *s)
{
    char v[16];
    const readings_t *r = &s->readings;

    /* Nhiệt độ: cam khi nóng > 35°C */
    if (isnan(r->temperature)) snprintf(v, sizeof(v), "--");
    else                       snprintf(v, sizeof(v), "%.1f", r->temperature);
    draw_value(K_TEMP, 0, v, "C", true,
               (!isnan(r->temperature) && r->temperature > 35.0f) ? C_ORANGE : C_TEXT);

    /* Độ ẩm không khí */
    if (isnan(r->humidity)) snprintf(v, sizeof(v), "--");
    else                    snprintf(v, sizeof(v), "%.0f", r->humidity);
    draw_value(K_HUMI, 1, v, "%", false, C_TEXT);

    /* Độ ẩm đất: đỏ khi lỗi, cam khi dưới ngưỡng bật, xanh khi đủ ẩm */
    uint16_t sc = !r->soil_ok ? C_RED
                : r->soil_percent < s->settings.soil_low ? C_ORANGE
                : r->soil_percent >= s->settings.soil_high ? C_BLUE : C_TEXT;
    if (r->soil_ok) snprintf(v, sizeof(v), "%d", r->soil_percent);
    else            snprintf(v, sizeof(v), "LOI");
    draw_value(K_SOIL, 2, v, r->soil_ok ? "%" : "", false, sc);

    /* Thanh độ ẩm đất + 2 vạch ngưỡng */
    char key[24];
    snprintf(key, sizeof(key), "%d/%d/%d/%d", r->soil_ok ? r->soil_percent : -1,
             s->settings.soil_low, s->settings.soil_high, r->soil_ok);
    if (changed(K_SOILBAR, key, sc)) {
        int bx = card_x(2) + 9, by = CARD_Y + 61, bw = CARD_W - 18, bh = 6;
        tft.fillRect(bx, by - 3, bw + 1, bh + 6, C_CARD);
        tft.fillRoundRect(bx, by, bw, bh, 3, C_TRACK);
        if (r->soil_ok && r->soil_percent > 0) {
            int w = bw * r->soil_percent / 100;
            if (w < 6) w = 6;
            tft.fillRoundRect(bx, by, w, bh, 3, C_BLUE);
        }
        tft.drawFastVLine(bx + bw * s->settings.soil_low / 100, by - 3, bh + 6, C_TEXT);
        tft.drawFastVLine(bx + bw * s->settings.soil_high / 100, by - 3, bh + 6, C_TEXT);
    }

    snprintf(v, sizeof(v), "ADC %d", r->soil_raw);
    if (changed(K_SOILRAW, v, C_MUTED)) {
        text_at(v, card_x(2) + 9, CARD_Y + CARD_H - 13, F_SMALL, C_MUTED, C_CARD, TL_DATUM, CARD_W - 18);
    }
}

/* ---------- Khung máy bơm ---------- */
static void update_pump_panel(const app_snapshot_t *s)
{
    const settings_t *cfg = &s->settings;
    const int x = MARGIN, y = PANEL_Y, w = SCREEN_W - 2 * MARGIN, h = PANEL_H;
    char l1[32], l2[48], l3[48];
    uint16_t c1, c3;

    snprintf(l1, sizeof(l1), "%s|%s", s->pump.on ? "DANG BOM" : "BOM TAT", cfg->auto_mode ? "AUTO" : "MANUAL");
    c1 = s->pump.on ? C_BLUE : C_MUTED;

    /* Dòng 1 + nhãn chế độ: đổi thì vẽ lại cả khung (viền đổi màu theo trạng thái bơm) */
    if (changed(K_PUMP_STATE, l1, c1)) {
        tft.fillRoundRect(x, y, w, h, 8, C_CARD);
        tft.drawRoundRect(x, y, w, h, 8, s->pump.on ? C_BLUE : C_BORDER);
        if (s->pump.on) tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 7, C_BLUE);
        text_at(s->pump.on ? "DANG BOM" : "BOM TAT", x + 12, y + 8, F_BIG, c1, C_CARD, TL_DATUM, 0);

        uint16_t pill = cfg->auto_mode ? C_GREEN : C_ORANGE;
        tft.fillRoundRect(x + w - 92, y + 10, 80, 24, 12, pill);
        text_at(cfg->auto_mode ? "AUTO" : "MANUAL", x + w - 52, y + 22, F_TEXT, C_BG, pill, MC_DATUM, 0);
        s_cache[K_PUMP_LINE2].valid = false;
        s_cache[K_PUMP_LINE3].valid = false;
    }

    /* Dòng 2: cách hoạt động */
    if (cfg->auto_mode) snprintf(l2, sizeof(l2), "Bat < %u%%    Tat >= %u%%", cfg->soil_low, cfg->soil_high);
    else                snprintf(l2, sizeof(l2), "Dieu khien tay (web / nut nhan)");
    if (changed(K_PUMP_LINE2, l2, C_TEXT)) {
        text_at(l2, x + 12, y + 40, F_TEXT, C_TEXT, C_CARD, TL_DATUM, w - 24);
    }

    /* Dòng 3: thời gian chạy / nghỉ / lỗi */
    if (!s->readings.soil_ok && cfg->auto_mode) {
        snprintf(l3, sizeof(l3), "Loi cam bien dat - khong tu tuoi");
        c3 = C_RED;
    } else if (s->pump.on) {
        snprintf(l3, sizeof(l3), "Da chay %lus / toi da %us",
                 (unsigned long)s->pump_run_sec, cfg->max_pump_sec);
        c3 = C_BLUE;
    } else if (s->cooldown_left_sec > 0) {
        snprintf(l3, sizeof(l3), "Nghi them %lus cho nuoc tham", (unsigned long)s->cooldown_left_sec);
        c3 = C_MUTED;
    } else {
        snprintf(l3, sizeof(l3), "%s", cfg->auto_mode ? "San sang tuoi tu dong" : "Cho lenh bat bom");
        c3 = C_MUTED;
    }
    if (changed(K_PUMP_LINE3, l3, c3)) {
        text_at(l3, x + 12, y + 58, F_TEXT, c3, C_CARD, TL_DATUM, w - 24);
    }
}

/* ---------- Chân trang ---------- */
static void update_footer(const app_snapshot_t *s, const net_status_t *n)
{
    char l[40], r[40];
    uint16_t rc;
    const int y = FOOTER_Y + (SCREEN_H - FOOTER_Y) / 2;

    if (n->wifi_ok) snprintf(l, sizeof(l), "IP %s  %ddBm", n->ip, n->rssi);
    else            snprintf(l, sizeof(l), "Dang ket noi WiFi...");
    if (changed(K_FOOT_L, l, C_MUTED)) {
        text_at(l, MARGIN + 2, y, F_TEXT, C_MUTED, C_BG, ML_DATUM, 190);
    }

    if (n->cloud_ok && s->pending_logs <= 1) { snprintf(r, sizeof(r), "Cloud: OK"); rc = C_GREEN; }
    else if (s->pending_logs > 0) { snprintf(r, sizeof(r), "Cho gui: %u", s->pending_logs); rc = C_ORANGE; }
    else { snprintf(r, sizeof(r), "Cloud: --"); rc = C_MUTED; }
    if (changed(K_FOOT_R, r, rc)) {
        text_at(r, SCREEN_W - MARGIN - 2, y, F_TEXT, rc, C_BG, MR_DATUM, 110);
    }
}

void display_update(const app_snapshot_t *s, const net_status_t *n)
{
    update_header(n);
    update_cards(s);
    update_pump_panel(s);
    update_footer(s, n);
}
