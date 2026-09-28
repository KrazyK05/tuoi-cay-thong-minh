/* =====================================================================
 *  cloud.cpp
 * ===================================================================== */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "app_config.h"
#include "cloud.h"
#include "app_state.h"
#include "net.h"
#include "timekeeper.h"
#include "irrigation_logic.h"

#define MAX_ITEMS_PER_SYNC 30

static volatile bool s_ok = false;

bool cloud_is_ok(void) { return s_ok; }

/* Đổi thời điểm ghi nhận (millis) sang giờ UNIX (giờ DS1302); 0 nếu chưa có giờ (server sẽ dùng giờ nhận) */
static uint32_t to_epoch(uint32_t captured_ms)
{
    uint32_t now = net_epoch();
    if (now == 0) return 0;
    return now - (millis() - captured_ms) / 1000UL;
}

static void json_float_or_skip(JsonObject o, const char *key, float v)
{
    if (!isnan(v)) o[key] = roundf(v * 10.0f) / 10.0f;
}

/* Tạo nội dung gửi lên. Gọi khi đang giữ khoá. */
static void build_payload_locked(JsonDocument &doc, uint16_t *n_logs, uint16_t *n_events)
{
    JsonObject root = doc.to<JsonObject>();
    root["fw"]   = FIRMWARE_VERSION;
    root["ip"]   = WiFi.localIP().toString();
    root["rssi"] = WiFi.RSSI();
    json_float_or_skip(root, "t", g_readings.temperature);
    json_float_or_skip(root, "h", g_readings.humidity);
    if (g_readings.soil_ok) root["soil"] = g_readings.soil_percent;
    root["soil_raw"] = g_readings.soil_raw;
    root["pump"]     = g_pump.on;

    JsonObject c = root["cfg"].to<JsonObject>();
    c["ver"]          = g_settings.ver;
    c["mode"]         = g_settings.auto_mode ? "auto" : "manual";
    c["manual_pump"]  = g_settings.manual_pump;
    c["soil_low"]     = g_settings.soil_low;
    c["soil_high"]    = g_settings.soil_high;
    c["max_pump_sec"] = g_settings.max_pump_sec;
    c["cooldown_sec"] = g_settings.cooldown_sec;

    *n_logs = dq_count(&g_log_queue);
    if (*n_logs > MAX_ITEMS_PER_SYNC) *n_logs = MAX_ITEMS_PER_SYNC;
    JsonArray logs = root["logs"].to<JsonArray>();
    for (uint16_t i = 0; i < *n_logs; i++) {
        const log_item_t *it = (const log_item_t *)dq_peek(&g_log_queue, i);
        JsonObject o = logs.add<JsonObject>();
        o["ts"] = to_epoch(it->captured_ms);
        json_float_or_skip(o, "t", it->t);
        json_float_or_skip(o, "h", it->h);
        if (it->soil >= 0) o["soil"] = it->soil;
        o["pump"] = it->pump;
    }

    *n_events = dq_count(&g_event_queue);
    if (*n_events > MAX_ITEMS_PER_SYNC) *n_events = MAX_ITEMS_PER_SYNC;
    JsonArray evs = root["events"].to<JsonArray>();
    for (uint16_t i = 0; i < *n_events; i++) {
        const event_item_t *e = (const event_item_t *)dq_peek(&g_event_queue, i);
        JsonObject o = evs.add<JsonObject>();
        o["ts"]  = to_epoch(e->captured_ms);
        o["on"]  = e->on;
        o["src"] = irr_source_name((pump_source_t)e->src);
    }
}

/* Đọc cấu hình server trả về. Trả false nếu thiếu / sai. */
static bool parse_cloud_settings(JsonObject c, settings_t *out)
{
    long lo = c["soil_low"] | -1L, hi = c["soil_high"] | -1L;
    long mx = c["max_pump_sec"] | -1L, cd = c["cooldown_sec"] | -1L;
    if (!c["ver"].is<int>() || !irr_settings_valid(lo, hi, mx, cd)) return false;
    out->ver          = c["ver"].as<int32_t>();
    out->auto_mode    = strcmp(c["mode"] | "auto", "auto") == 0;
    out->manual_pump  = c["manual_pump"] | false;
    out->soil_low     = (uint8_t)lo;
    out->soil_high    = (uint8_t)hi;
    out->max_pump_sec = (uint16_t)mx;
    out->cooldown_sec = (uint32_t)cd;
    return true;
}

static bool sync_once(WiFiClientSecure &client, HTTPClient &http)
{
    JsonDocument doc;
    uint16_t n_logs, n_events;
    uint32_t log_evicted, ev_evicted;
    String body;

    /* 1) Chụp nhanh dữ liệu trong khoá — không gọi mạng khi đang giữ khoá */
    state_lock();
    build_payload_locked(doc, &n_logs, &n_events);
    log_evicted = g_log_queue.evicted;
    ev_evicted  = g_event_queue.evicted;
    state_unlock();
    serializeJson(doc, body);

    /* 2) Gửi */
    http.begin(client, String(SERVER_URL) + "/api/device/sync");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Key", DEVICE_KEY);
    uint32_t t0 = millis();
    int code = http.POST(body);
    String resp = code > 0 ? http.getString() : String();
    uint32_t rtt_ms = millis() - t0;
    http.end();

    if (code != 200) {
        Serial.printf("[CLOUD] Loi HTTP %d %s\n", code,
                      code < 0 ? HTTPClient::errorToString(code).c_str() : resp.c_str());
        return false;
    }

    JsonDocument r;
    if (deserializeJson(r, resp)) return false;

    /* Giờ server chỉ để timekeeper so sánh / chỉnh DS1302 khi lệch nhiều.
     * Bỏ qua nếu phản hồi quá chậm (giờ nhận được không còn chính xác). */
    uint32_t server_time = r["server_time"] | 0UL;
    if (server_time > 0 && rtt_ms < 3000) {
        timekeeper_submit_reference(server_time + rtt_ms / 2000UL);
    }

    /* 3) Xoá dữ liệu đã gửi thành công */
    state_lock();
    dq_drop_sent(&g_log_queue, n_logs, log_evicted);
    dq_drop_sent(&g_event_queue, n_events, ev_evicted);
    state_unlock();

    /* 4) Áp dụng cấu hình mới từ web (nếu phiên bản khác) */
    settings_t incoming;
    if (parse_cloud_settings(r["cfg"], &incoming) && state_apply_cloud_settings(&incoming)) {
        Serial.printf("[CLOUD] Nhan cau hinh v%ld: %s, bom tay=%d, nguong %u-%u%%\n",
                      (long)incoming.ver, incoming.auto_mode ? "AUTO" : "MANUAL",
                      incoming.manual_pump, incoming.soil_low, incoming.soil_high);
    }
    return true;
}

static void cloud_task(void *arg)
{
    (void)arg;
    WiFiClientSecure client;
    /* Bỏ kiểm tra chứng chỉ cho đơn giản (dữ liệu VẪN được mã hoá TLS).
     * Muốn chặt chẽ hơn: client.setCACert(<chứng chỉ gốc của Render>). */
    client.setInsecure();
    HTTPClient http;
    http.setReuse(true);          /* giữ kết nối → không bắt tay TLS lại mỗi 5 giây */
    http.setTimeout(10000);
    http.setConnectTimeout(8000);

    for (;;) {
        s_ok = net_wifi_ok() ? sync_once(client, http) : false;
        vTaskDelay(pdMS_TO_TICKS(SYNC_INTERVAL_MS));
    }
}

void cloud_start(void)
{
    /* Stack 12 KB đủ cho TLS + JSON; ưu tiên thấp; ghim vào Core 0 */
    xTaskCreatePinnedToCore(cloud_task, "cloud", 12288, NULL, 1, NULL, 0);
}
