/* =====================================================================
 *  button.cpp
 * ===================================================================== */
#include <Arduino.h>
#include "app_config.h"
#include "button.h"

#define DEBOUNCE_MS   40
#define LONG_PRESS_MS 2000

static bool     s_level       = HIGH;   /* HIGH = đang nhả (trở kéo lên) */
static uint32_t s_changed_ms  = 0;
static uint32_t s_pressed_ms  = 0;
static bool     s_long_fired  = false;

void button_init(void)
{
    pinMode(PIN_BUTTON, INPUT_PULLUP);
}

button_event_t button_poll(void)
{
    bool level = digitalRead(PIN_BUTTON);
    uint32_t now = millis();

    if (level != s_level && now - s_changed_ms > DEBOUNCE_MS) {
        s_changed_ms = now;
        s_level = level;
        if (level == LOW) {                     /* vừa nhấn */
            s_pressed_ms = now;
            s_long_fired = false;
        } else if (!s_long_fired) {             /* vừa nhả sau nhấn ngắn */
            return BTN_SHORT;
        }
    }
    if (s_level == LOW && !s_long_fired && now - s_pressed_ms >= LONG_PRESS_MS) {
        s_long_fired = true;
        return BTN_LONG;
    }
    return BTN_NONE;
}
