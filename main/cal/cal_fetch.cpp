#include "cal_fetch.h"

#include <cstdio>
#include <cstring>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "cal_fetch";

namespace {
struct sink_t {
    char  *buf;
    size_t cap, len;
    bool   overflow;
};
}

// perform() also reads the 302's HTML body and dispatches it as ON_DATA, so only the final
// 200 response is kept.
static esp_err_t on_event(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
    if (esp_http_client_get_status_code(evt->client) != 200) return ESP_OK;
    sink_t *s = static_cast<sink_t *>(evt->user_data);
    size_t n = (size_t)evt->data_len;
    if (s->overflow || s->len + n >= s->cap) { s->overflow = true; return ESP_OK; }
    memcpy(s->buf + s->len, evt->data, n);
    s->len += n;
    return ESP_OK;
}

esp_err_t cal_fetch(const char *url, char *buf, size_t cap, size_t *out_len)
{
    if (!url || !buf || cap < 2 || !out_len) return ESP_ERR_INVALID_ARG;
    *out_len = 0;
    char full[512];
    int n = snprintf(full, sizeof full, "%s%sback=3&days=14", url, strchr(url, '?') ? "&" : "?");
    if (n < 0 || (size_t)n >= sizeof full) return ESP_ERR_INVALID_ARG;

    sink_t sink = { buf, cap, 0, false };
    esp_http_client_config_t cfg = {};
    cfg.url = full;
    cfg.event_handler = on_event;
    cfg.user_data = &sink;
    cfg.timeout_ms = 45000;                    // per network operation; a cold Apps Script took 16.5 s live (2026-09-25)
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.disable_auto_redirect = false;
    cfg.max_redirection_count = 5;
    cfg.buffer_size = 2048;
    cfg.buffer_size_tx = 2048;                 // the redirect target URL is ~500+ chars; 512 fails "Out of buffer"

    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return ESP_ERR_NO_MEM;
    esp_err_t err = esp_http_client_perform(c);
    int status = esp_http_client_get_status_code(c);
    esp_http_client_cleanup(c);

    // Never log the URL: it is the secret.
    if (err != ESP_OK) { ESP_LOGW(TAG, "perform: %s", esp_err_to_name(err)); return err; }
    if (status != 200) { ESP_LOGW(TAG, "HTTP %d", status); return ESP_FAIL; }
    if (sink.overflow) { ESP_LOGW(TAG, "body over %u bytes", (unsigned)(cap - 1)); return ESP_ERR_INVALID_SIZE; }
    buf[sink.len] = '\0';
    *out_len = sink.len;
    ESP_LOGI(TAG, "%u bytes", (unsigned)sink.len);
    return ESP_OK;
}
