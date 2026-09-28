/* =====================================================================
 *  pump.cpp
 * ===================================================================== */
#include <Arduino.h>
#include "app_config.h"
#include "pump.h"
#include "app_state.h"
#include "irrigation_logic.h"

static void relay_write(bool on)
{
    bool level = RELAY_ACTIVE_LOW ? !on : on;
    digitalWrite(PIN_RELAY, level ? HIGH : LOW);
}

void pump_init(void)
{
    relay_write(false);          /* đặt mức TẮT trước khi bật OUTPUT → bơm không giật */
    pinMode(PIN_RELAY, OUTPUT);
    relay_write(false);
}

/* Bật/tắt thật + ghi sự kiện. Gọi khi đang giữ khoá. */
static void pump_set_locked(bool on, pump_source_t src)
{
    event_item_t ev;
    uint32_t now = millis();
    if (on == g_pump.on) return;

    relay_write(on);
    g_pump.on = on;
    if (on) { g_pump.started_ms = now; g_pump.ever_run = true; }
    else    { g_pump.stopped_ms = now; }

    ev.captured_ms = now;
    ev.on  = on;
    ev.src = (uint8_t)src;
    dq_push(&g_event_queue, &ev);
    Serial.printf("[PUMP] %s (%s)\n", on ? "BAT" : "TAT", irr_source_name(src));
}

void pump_control(void)
{
    irr_input_t in;
    irr_decision_t d;
    uint32_t now = millis();

    state_lock();
    in.settings      = &g_settings;
    in.readings      = &g_readings;
    in.pump_on       = g_pump.on;
    in.run_ms        = g_pump.on ? now - g_pump.started_ms : 0;
    in.ever_run      = g_pump.ever_run;
    in.since_stop_ms = now - g_pump.stopped_ms;
    in.manual_source = g_manual_source;

    d = irr_decide(&in);
    if (d.action == IRR_TURN_ON)  pump_set_locked(true,  d.source);
    if (d.action == IRR_TURN_OFF) pump_set_locked(false, d.source);
    if (d.clear_manual) {
        g_settings.manual_pump = false;   /* thay đổi tại chỗ → tự báo lên cloud */
        state_save_locked();
    }
    state_unlock();
}
