#include "cal_store.h"

#include <cstddef>
#include <cstring>

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "cal_store";
static const char *NS = "cal";

// "2026-09-25" -> "d20260925"
static void day_key(const char *date, char key[10])
{
    key[0] = 'd';
    memcpy(key + 1, date, 4);
    memcpy(key + 5, date + 5, 2);
    memcpy(key + 7, date + 8, 2);
    key[9] = '\0';
}

// Only the used events go to flash: a 3-event day is ~300 bytes, not 2292.
static size_t day_len(uint8_t n) { return offsetof(cal_day_t, ev) + n * sizeof(cal_event_t); }

static bool in_window(const cal_window_t *w, const char *key)
{
    char k[10];
    for (int i = 0; i < w->n_days; i++) {
        day_key(w->day[i].date, k);
        if (strcmp(k, key) == 0) return true;
    }
    return false;
}

esp_err_t cal_store_save(const cal_window_t *w)
{
    if (!w || w->n_days == 0) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    // Stale keys first, so the partition never holds the old and new windows at once.
    // ponytail: 32 is plenty, a previous save left at most CAL_MAX_DAYS day keys behind.
    char stale[32][NVS_KEY_NAME_MAX_SIZE];
    int n_stale = 0;
    nvs_iterator_t it = NULL;
    esp_err_t res = nvs_entry_find_in_handle(h, NVS_TYPE_BLOB, &it);
    while (res == ESP_OK && n_stale < 32) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        if (info.key[0] == 'd' && !in_window(w, info.key)) strcpy(stale[n_stale++], info.key);
        res = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
    for (int i = 0; i < n_stale && err == ESP_OK; i++) err = nvs_erase_key(h, stale[i]);

    char key[10];
    for (int i = 0; i < w->n_days && err == ESP_OK; i++) {
        day_key(w->day[i].date, key);
        err = nvs_set_blob(h, key, &w->day[i], day_len(w->day[i].n));
    }

    cal_meta_t m = {};
    memcpy(m.generated, w->generated, sizeof m.generated);
    memcpy(m.tz, w->tz, sizeof m.tz);
    m.n_days = w->n_days;
    memcpy(m.first, w->day[0].date, sizeof m.first);
    memcpy(m.last, w->day[w->n_days - 1].date, sizeof m.last);
    if (err == ESP_OK) err = nvs_set_blob(h, "meta", &m, sizeof m);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) ESP_LOGE(TAG, "save: %s", esp_err_to_name(err));
    else ESP_LOGI(TAG, "saved %u days %s..%s, erased %d", (unsigned)m.n_days, m.first, m.last, n_stale);
    return err;
}

// Reads a blob and rejects any length that does not match the current layout, so a firmware
// update that changes the structs reads as "no data" rather than as garbage.
static esp_err_t load_blob(const char *key, void *out, size_t cap, size_t *len)
{
    memset(out, 0, cap);
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    *len = cap;
    err = nvs_get_blob(h, key, out, len);
    nvs_close(h);
    return err;
}

esp_err_t cal_store_load_day(const char *date, cal_day_t *out)
{
    if (!date || strlen(date) != 10 || !out) return ESP_ERR_INVALID_ARG;
    char key[10];
    day_key(date, key);
    size_t len;
    esp_err_t err = load_blob(key, out, sizeof *out, &len);
    if (err == ESP_OK && (out->n > CAL_MAX_EVENTS || len != day_len(out->n))) {
        memset(out, 0, sizeof *out);
        err = ESP_ERR_INVALID_SIZE;
    }
    return err;
}

esp_err_t cal_store_load_meta(cal_meta_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    size_t len;
    esp_err_t err = load_blob("meta", out, sizeof *out, &len);
    if (err == ESP_OK && len != sizeof *out) {
        memset(out, 0, sizeof *out);
        err = ESP_ERR_INVALID_SIZE;
    }
    return err;
}
