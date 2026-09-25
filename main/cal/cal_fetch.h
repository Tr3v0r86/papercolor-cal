// cal_fetch.h - one HTTPS GET of the Apps Script window into a caller-owned buffer.
//
// Component REQUIRES: esp_http_client mbedtls (esp_crt_bundle). Needs
// CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y (IDF default) and Wi-Fi already up.
#pragma once
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// GET url + "?back=3&days=14" (or "&..." if url already has a query), following the 302 to
// script.googleusercontent.com. The body lands in buf (32 KB is the planned size, allocated
// by the caller in PSRAM with heap_caps_malloc(..., MALLOC_CAP_SPIRAM)) and is NUL-terminated;
// *out_len excludes the NUL. ESP_ERR_INVALID_SIZE if the body does not fit in cap - 1,
// ESP_FAIL on a final status other than 200, else the esp_http_client error.
esp_err_t cal_fetch(const char *url, char *buf, size_t cap, size_t *out_len);

#ifdef __cplusplus
}
#endif
