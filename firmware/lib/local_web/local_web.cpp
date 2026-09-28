/* =====================================================================
 *  local_web.cpp
 * ===================================================================== */
#include <Arduino.h>
#include <time.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "app_config.h"
#include "local_web.h"
#include "web_page.h"
#include "app_state.h"
#include "cloud.h"
#include "net.h"
#include "timekeeper.h"
#include "time_utils.h"

static WebServer s_server(80);

static bool check_auth(void)
{
    if (strlen(LOCAL_WEB_USER) == 0) return true;
    if (s_server.authenticate(LOCAL_WEB_USER, LOCAL_WEB_PASS)) return true;
    s_server.requestAuthentication();
    return false;
}

static void send_json(int code, JsonDocument &doc)
{
    String s;
    serializeJson(doc, s);
    s_server.send(code, "application/json", s);
}

static void send_error(int code, const char *msg)
{
    JsonDocument d;
    d["error"] = msg;
    send_json(code, d);
}

static void handle_root(void)
{
    if (!check_auth()) return;
    s_server.send_P(200, "text/html; charset=utf-8", LOCAL_PAGE);
}

static void handle_status(void)
{
    if (!check_auth()) return;
    app_snapshot_t s;
    net_status_t n;
    state_get_snapshot(&s);
    net_get_status(&n);

    JsonDocument d;
    if (!isnan(s.readings.temperature)) d["t"] = s.readings.temperature; else d["t"] = nullptr;
    if (!isnan(s.readings.humidity))    d["h"] = s.readings.humidity;    else d["h"] = nullptr;
    d["soil"]         = s.readings.soil_percent;
    d["soil_raw"]     = s.readings.soil_raw;
    d["soil_ok"]      = s.readings.soil_ok;
    d["pump"]         = s.pump.on;
    d["mode"]         = s.settings.auto_mode ? "auto" : "manual";
    d["manual_pump"]  = s.settings.manual_pump;
    d["soil_low"]     = s.settings.soil_low;
    d["soil_high"]    = s.settings.soil_high;
    d["max_pump_sec"] = s.settings.max_pump_sec;
    d["cooldown_sec"] = s.settings.cooldown_sec;
    d["cloud"]        = cloud_is_ok();
    d["ip"]           = n.ip;
    d["rssi"]         = n.rssi;
    d["fw"]           = FIRMWARE_VERSION;
    if (n.time_ok) {
        datetime_t dt;
        char t[32];
        tu_epoch_to_datetime((uint32_t)time(NULL) + TIMEZONE_OFFSET_SEC, &dt);
        snprintf(t, sizeof(t), "%04u-%02u-%02u %02u:%02u:%02u", dt.year, dt.month, dt.day,
                 dt.hour, dt.minute, dt.second);
        d["time"] = t;
    }
    d["time_status"]  = timekeeper_status_name();
    send_json(200, d);
}

/* Body: {"mode":"auto"|"manual"}  hoặc  {"pump":true|false} */
static void handle_control(void)
{
    if (!check_auth()) return;
    JsonDocument b;
    if (deserializeJson(b, s_server.arg("plain"))) return send_error(400, "JSON khong hop le");

    if (b["mode"].is<const char *>()) {
        const char *m = b["mode"];
        if (strcmp(m, "auto") != 0 && strcmp(m, "manual") != 0) return send_error(400, "Che do khong hop le");
        state_local_set_mode(strcmp(m, "auto") == 0);
    }
    if (b["pump"].is<bool>()) state_local_set_pump(b["pump"].as<bool>());
    s_server.send(200, "application/json", "{\"ok\":true}");
}

/* Body: {"soil_low":35,"soil_high":60,"max_pump_sec":60,"cooldown_sec":300} */
static void handle_settings(void)
{
    if (!check_auth()) return;
    JsonDocument b;
    if (deserializeJson(b, s_server.arg("plain"))) return send_error(400, "JSON khong hop le");
    if (!state_local_set_thresholds(b["soil_low"] | -1L, b["soil_high"] | -1L,
                                    b["max_pump_sec"] | -1L, b["cooldown_sec"] | -1L)) {
        return send_error(400, "Gia tri khong hop le (nguong bat phai nho hon nguong tat)");
    }
    s_server.send(200, "application/json", "{\"ok\":true}");
}

/* Body: {"epoch": <giờ UNIX UTC>} — trang web gửi giờ của điện thoại / máy tính */
static void handle_time(void)
{
    if (!check_auth()) return;
    JsonDocument b;
    if (deserializeJson(b, s_server.arg("plain"))) return send_error(400, "JSON khong hop le");
    uint32_t epoch = b["epoch"] | 0UL;
    if (!timekeeper_set_epoch(epoch, "web noi bo")) {
        return send_error(500, "Khong ghi duoc DS1302 (kiem tra day noi / gio khong hop le)");
    }
    s_server.send(200, "application/json", "{\"ok\":true}");
}

void local_web_init(void)
{
    s_server.on("/", HTTP_GET, handle_root);
    s_server.on("/api/status", HTTP_GET, handle_status);
    s_server.on("/api/control", HTTP_POST, handle_control);
    s_server.on("/api/settings", HTTP_POST, handle_settings);
    s_server.on("/api/time", HTTP_POST, handle_time);
    s_server.onNotFound([]() { s_server.send(404, "text/plain", "Not found"); });
    s_server.begin();
}

void local_web_loop(void)
{
    s_server.handleClient();
}
