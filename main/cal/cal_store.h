// cal_store.h - the fetched window in NVS: namespace "cal", one blob per day ("dYYYYMMDD")
// plus "meta". The fetch writes it; the render reads one day at a time.
//
// Component REQUIRES: nvs_flash. nvs_flash_init() is the app's job (the vendor HAL does it).
#pragma once
#include "esp_err.h"
#include "cal_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char    generated[26];
    char    tz[24];
    uint8_t n_days;
    char    first[11];   // window bounds, "YYYY-MM-DD"
    char    last[11];
} cal_meta_t;

// Writes every day of *w and the meta, erasing day keys that are not in the new window.
// ESP_ERR_INVALID_ARG for an empty window, so a bad fetch can never wipe a good store.
esp_err_t cal_store_save(const cal_window_t *w);

// ESP_ERR_NVS_NOT_FOUND if the day (or the namespace) is absent; *out is zeroed on any error.
esp_err_t cal_store_load_day(const char *date, cal_day_t *out);
esp_err_t cal_store_load_meta(cal_meta_t *out);

#ifdef __cplusplus
}
#endif
