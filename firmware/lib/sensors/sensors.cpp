/* =====================================================================
 *  sensors.cpp
 * ===================================================================== */
#include <Arduino.h>
#include <DHT.h>
#include "app_config.h"
#include "sensors.h"
#include "irrigation_logic.h"

static DHT   s_dht(PIN_DHT, DHT11);
static float s_last_t = NAN;
static float s_last_h = NAN;

void sensors_init(void)
{
    analogReadResolution(12);
    s_dht.begin();
}

/* Lấy trung bình 16 lần đọc ADC để giảm nhiễu */
static int read_soil_raw(void)
{
    long sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += analogRead(PIN_SOIL);
        delayMicroseconds(200);
    }
    return (int)(sum / 16);
}

void sensors_read(readings_t *out)
{
    float t = s_dht.readTemperature();
    float h = s_dht.readHumidity();
    if (!isnan(t)) s_last_t = t;
    if (!isnan(h)) s_last_h = h;

    int raw = read_soil_raw();
    out->temperature  = s_last_t;
    out->humidity     = s_last_h;
    out->soil_raw     = raw;
    out->soil_ok      = irr_soil_raw_valid(raw);
    out->soil_percent = irr_soil_percent(raw, SOIL_RAW_DRY, SOIL_RAW_WET);
}
