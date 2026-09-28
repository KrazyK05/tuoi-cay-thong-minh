/* =====================================================================
 *  app_state.cpp
 * ===================================================================== */
#include <Arduino.h>
#include "app_config.h"
#include "app_state.h"
#include "storage.h"
#include "irrigation_logic.h"

settings_t    g_settings;
readings_t    g_readings    = { NAN, NAN, 0, 0, false };
pump_state_t  g_pump        = { false, 0, 0, false };
pump_source_t g_manual_source = SRC_MANUAL;
data_queue_t  g_log_queue;
data_queue_t  g_event_queue;

static SemaphoreHandle_t s_mutex;
static log_item_t   s_log_storage[LOG_QUEUE_SIZE];
static event_item_t s_event_storage[EVENT_QUEUE_SIZE];

void state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    dq_init(&g_log_queue,   s_log_storage,   sizeof(log_item_t),   LOG_QUEUE_SIZE);
    dq_init(&g_event_queue, s_event_storage, sizeof(event_item_t), EVENT_QUEUE_SIZE);

    irr_default_settings(&g_settings, DEFAULT_SOIL_LOW, DEFAULT_SOIL_HIGH,
                         DEFAULT_MAX_PUMP_SEC, DEFAULT_COOLDOWN_SEC);
    settings_t saved = g_settings;
    if (storage_load_settings(&saved) &&
        irr_settings_valid(saved.soil_low, saved.soil_high, saved.max_pump_sec, saved.cooldown_sec)) {
        g_settings = saved;
    }
    /* An toàn: sau khi mất điện KHÔNG tự bật lại bơm tay */
    if (!g_settings.auto_mode && g_settings.manual_pump) {
        g_settings.manual_pump = false;
        storage_save_settings(&g_settings);
    }
}

void state_lock(void)   { xSemaphoreTake(s_mutex, portMAX_DELAY); }
void state_unlock(void) { xSemaphoreGive(s_mutex); }
void state_save_locked(void) { storage_save_settings(&g_settings); }

void state_set_readings(const readings_t *r)
{
    state_lock();
    g_readings = *r;
    state_unlock();
}

void state_get_snapshot(app_snapshot_t *out)
{
    uint32_t now = millis();
    state_lock();
    out->settings     = g_settings;
    out->readings     = g_readings;
    out->pump         = g_pump;
    out->pump_run_sec = g_pump.on ? (now - g_pump.started_ms) / 1000UL : 0;
    out->cooldown_left_sec = irr_cooldown_left_sec(&g_settings, g_pump.ever_run, g_pump.on,
                                                   now - g_pump.stopped_ms);
    out->pending_logs = dq_count(&g_log_queue);
    state_unlock();
}

void state_push_log(void)
{
    log_item_t it;
    state_lock();
    it.captured_ms = millis();
    it.t    = g_readings.temperature;
    it.h    = g_readings.humidity;
    it.soil = (int16_t)(g_readings.soil_ok ? g_readings.soil_percent : -1);
    it.pump = g_pump.on;
    dq_push(&g_log_queue, &it);
    state_unlock();
}

void state_local_toggle_pump(void)
{
    state_lock();
    if (g_settings.auto_mode) {
        g_settings.auto_mode = false;
        g_settings.manual_pump = true;
    } else {
        g_settings.manual_pump = !g_settings.manual_pump;
    }
    g_manual_source = SRC_LOCAL;
    state_save_locked();
    state_unlock();
}

void state_local_set_mode(bool auto_mode)
{
    state_lock();
    g_settings.auto_mode = auto_mode;
    g_settings.manual_pump = false;      /* đổi chế độ luôn bắt đầu với bơm tay TẮT */
    g_manual_source = SRC_LOCAL;
    state_save_locked();
    state_unlock();
}

void state_local_set_pump(bool on)
{
    state_lock();
    g_settings.auto_mode = false;
    g_settings.manual_pump = on;
    g_manual_source = SRC_LOCAL;
    state_save_locked();
    state_unlock();
}

bool state_local_set_thresholds(long soil_low, long soil_high, long max_pump_sec, long cooldown_sec)
{
    if (!irr_settings_valid(soil_low, soil_high, max_pump_sec, cooldown_sec)) return false;
    state_lock();
    g_settings.soil_low     = (uint8_t)soil_low;
    g_settings.soil_high    = (uint8_t)soil_high;
    g_settings.max_pump_sec = (uint16_t)max_pump_sec;
    g_settings.cooldown_sec = (uint32_t)cooldown_sec;
    state_save_locked();
    state_unlock();
    return true;
}

bool state_apply_cloud_settings(const settings_t *incoming)
{
    bool changed;
    state_lock();
    changed = incoming->ver != g_settings.ver;
    if (changed) {
        g_settings = *incoming;
        g_manual_source = SRC_MANUAL;
        state_save_locked();
    }
    state_unlock();
    return changed;
}
