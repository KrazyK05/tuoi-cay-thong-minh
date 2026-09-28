/* =====================================================================
 *  storage.cpp — NVS qua thư viện Preferences
 * ===================================================================== */
#include <Preferences.h>
#include "storage.h"

static const char *NVS_NAMESPACE = "irrig";
static const char *NVS_KEY       = "cfg";

bool storage_load_settings(settings_t *out)
{
    Preferences prefs;
    bool ok = false;
    prefs.begin(NVS_NAMESPACE, true);
    /* Chỉ đọc khi kích thước khớp → đổi cấu trúc settings_t thì tự về mặc định */
    if (prefs.getBytesLength(NVS_KEY) == sizeof(settings_t)) {
        ok = prefs.getBytes(NVS_KEY, out, sizeof(settings_t)) == sizeof(settings_t);
    }
    prefs.end();
    return ok;
}

void storage_save_settings(const settings_t *s)
{
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putBytes(NVS_KEY, s, sizeof(settings_t));
    prefs.end();
}

void storage_clear(void)
{
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
}
