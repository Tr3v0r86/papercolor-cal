#include "app_calendar.h"
#include <cstdio>
#include <ctime>
#include "esp_log.h"
#include "hal/hal.h"
#include "hal/wifi/hal_wifi.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef GCAL_URL
#define GCAL_URL ""
#endif

using namespace hal_wifi;

static const char* TAG = "Calendar";

// TODO(cal): main/cal/ lands the fetch (GET GCAL_URL, store the window in NVS) and the model the
// face reads. Wi-Fi join and the SNTP-to-RTC sync stay here; the fetch only does HTTP.
static bool cal_fetch_hook()
{
    ESP_LOGI(TAG, "fetch hook stub, url %s", GCAL_URL[0] ? "set" : "unset");
    return false;
}

void app_calendar_render(int day_offset)
{
    // ponytail: placeholder face. The real one follows boards/papercolor/calendar/face-spec.md.
    m5::rtc_datetime_t now;
    struct tm day = {};
    if (M5.Rtc.getDateTime(&now)) day = now.get_tm();
    day.tm_mday += day_offset;
    day.tm_isdst = 0;
    mktime(&day);

    M5Canvas* c = hal.Canvas;
    const int cx = c->width() / 2;
    char buf[48];

    c->fillScreen(TFT_WHITE);
    c->setTextColor(TFT_BLACK, TFT_WHITE);
    c->setTextDatum(middle_center);

    c->setFont(&fonts::Font7);
    c->setTextSize(4);
    snprintf(buf, sizeof(buf), "%02d", day.tm_mday);
    c->drawString(buf, cx, c->height() * 2 / 5);

    c->setFont(&fonts::FreeSansBold12pt7b);
    c->setTextSize(1);
    snprintf(buf, sizeof(buf), "CALENDAR STUB offset %d", day_offset);
    c->drawString(buf, cx, c->height() * 3 / 4);

    // RTC readout so the glass shows whether SNTP has set the clock (first boot reads 2026-01-01).
    snprintf(buf, sizeof(buf), "RTC %04d-%02d-%02d %02d:%02d", now.date.year, now.date.month, now.date.date,
             now.time.hours, now.time.minutes);
    c->drawString(buf, cx, c->height() * 3 / 4 + 40);

    hal.statusEventSend(OPERATION_EVENT_REFRESH_START);
    c->pushSprite(0, 0);
    hal.statusEventSend(OPERATION_EVENT_REFRESH_COMPLETE);
}

void app_calendar_daily_cycle(void)
{
    bool online = hal.settings.wifi_ssid[0] &&
                  (WiFi.isConnected() ||
                   WiFi.connect(hal.settings.wifi_ssid, hal.settings.wifi_password, 15000) == ESP_OK);
    if (online) {
        hal.syncRtcFromSntp(10000);
        cal_fetch_hook();
    } else {
        ESP_LOGW(TAG, "offline, rendering from what is stored");
    }
    app_calendar_render(0);
}
