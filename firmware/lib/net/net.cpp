/* =====================================================================
 *  net.cpp
 * ===================================================================== */
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <time.h>
#include "app_config.h"
#include "net.h"

#define RECONNECT_EVERY_MS 20000

static bool     s_was_connected = false;
static uint32_t s_last_try_ms   = 0;

void net_init(void)
{
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    /* Không dùng NTP: giờ hệ thống do lib/timekeeper lấy từ DS1302 */
    s_last_try_ms = millis();
}

void net_loop(void)
{
    bool connected = WiFi.status() == WL_CONNECTED;

    if (connected && !s_was_connected) {
        Serial.printf("[WIFI] Da ket noi, IP: %s\n", WiFi.localIP().toString().c_str());
        MDNS.end();
        if (MDNS.begin(MDNS_NAME)) MDNS.addService("http", "tcp", 80);
    }
    if (!connected && millis() - s_last_try_ms > RECONNECT_EVERY_MS) {
        s_last_try_ms = millis();
        Serial.println("[WIFI] Dang ket noi lai...");
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    s_was_connected = connected;
}

bool net_wifi_ok(void)
{
    return WiFi.status() == WL_CONNECTED;
}

uint32_t net_epoch(void)
{
    time_t now = time(NULL);
    return now > 1700000000 ? (uint32_t)now : 0;
}

void net_get_status(net_status_t *out)
{
    time_t now = time(NULL);
    out->wifi_ok  = net_wifi_ok();
    out->cloud_ok = false;
    out->time_ok  = now > 1700000000;
    out->rssi     = out->wifi_ok ? WiFi.RSSI() : 0;
    if (out->wifi_ok) {
        snprintf(out->ip, sizeof(out->ip), "%s", WiFi.localIP().toString().c_str());
    } else {
        snprintf(out->ip, sizeof(out->ip), "--");
    }
    if (out->time_ok) {
        struct tm t;
        localtime_r(&now, &t);
        out->hour = t.tm_hour;
        out->minute = t.tm_min;
    } else {
        out->hour = out->minute = -1;
    }
}
