/* =====================================================================
 *  irrigation_logic.c — THUẬT TOÁN TƯỚI (C thuần)
 * ===================================================================== */
#include "irrigation_logic.h"

irr_decision_t irr_decide(const irr_input_t *in)
{
    irr_decision_t d = { IRR_KEEP, SRC_AUTO, false };
    const settings_t *s = in->settings;
    const readings_t *r = in->readings;

    /* 1) NGẮT AN TOÀN — áp dụng cho CẢ 2 chế độ.
     *    Bơm chạy quá max_pump_sec thì tắt, dù đất chưa đủ ẩm
     *    (chống ngập, chống cháy bơm khi hết nước / cảm biến rơi khỏi chậu). */
    if (in->pump_on && in->run_ms >= (uint32_t)s->max_pump_sec * 1000UL) {
        d.action = IRR_TURN_OFF;
        d.source = SRC_SAFETY;
        d.clear_manual = (!s->auto_mode && s->manual_pump);
        return d;
    }

    if (s->auto_mode) {
        /* 2) AUTO có trễ (hysteresis): bật khi < soil_low, tắt khi >= soil_high.
         *    Hai ngưỡng tách nhau để bơm không bật tắt liên tục quanh 1 mốc. */
        bool rested = !in->ever_run ||
                      in->since_stop_ms >= s->cooldown_sec * 1000UL;

        if (!in->pump_on) {
            if (r->soil_ok && r->soil_percent < s->soil_low && rested) {
                d.action = IRR_TURN_ON;
                d.source = SRC_AUTO;
            }
        } else if (!r->soil_ok || r->soil_percent >= s->soil_high) {
            /* đủ ẩm, hoặc cảm biến lỗi → tắt cho an toàn */
            d.action = IRR_TURN_OFF;
            d.source = SRC_AUTO;
        }
    } else {
        /* 3) MANUAL: bơm theo lệnh manual_pump */
        if (in->pump_on != s->manual_pump) {
            d.action = s->manual_pump ? IRR_TURN_ON : IRR_TURN_OFF;
            d.source = in->manual_source;
        }
    }
    return d;
}

uint32_t irr_cooldown_left_sec(const settings_t *s, bool ever_run, bool pump_on, uint32_t since_stop_ms)
{
    uint32_t need_ms = s->cooldown_sec * 1000UL;
    if (!s->auto_mode || pump_on || !ever_run || since_stop_ms >= need_ms) return 0;
    return (need_ms - since_stop_ms + 999UL) / 1000UL;
}

int irr_soil_percent(int raw, int raw_dry, int raw_wet)
{
    long pct;
    if (raw_dry == raw_wet) return 0;
    pct = (long)(raw - raw_dry) * 100L / (long)(raw_wet - raw_dry);
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    return (int)pct;
}

bool irr_soil_raw_valid(int raw)
{
    return raw > 50 && raw < 4050;
}

bool irr_settings_valid(long soil_low, long soil_high, long max_pump_sec, long cooldown_sec)
{
    return soil_low >= 0 && soil_high <= 100 && soil_low < soil_high &&
           max_pump_sec >= 5 && max_pump_sec <= 3600 &&
           cooldown_sec >= 0 && cooldown_sec <= 86400;
}

void irr_default_settings(settings_t *s, int soil_low, int soil_high, int max_pump_sec, long cooldown_sec)
{
    s->auto_mode    = true;
    s->manual_pump  = false;
    s->soil_low     = (uint8_t)soil_low;
    s->soil_high    = (uint8_t)soil_high;
    s->max_pump_sec = (uint16_t)max_pump_sec;
    s->cooldown_sec = (uint32_t)cooldown_sec;
    s->ver          = 0;
}

const char *irr_source_name(pump_source_t src)
{
    switch (src) {
        case SRC_AUTO:   return "auto";
        case SRC_MANUAL: return "manual";
        case SRC_LOCAL:  return "local";
        case SRC_SAFETY: return "safety";
        default:         return "auto";
    }
}
