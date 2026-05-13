// =====================================================================
//  web_portal.cpp — HTTP + WebSocket server for the phone dashboard.
//
//  Backed by esp_http_server (comes with ESP-IDF / arduino-esp32 v3).
//  Lives entirely on the NCM netif's IP; the regular Wi-Fi station is
//  not used (and is in fact in scan mode).
//
//  The WebSocket frame format is intentionally simple JSON so the page
//  can stay tiny:
//    {"seq":1234,"mode":"wardrive","events":[
//       {"type":1,"ts":...,"mac":"..","name":"..","rssi":-62,"channel":6,"auth":3,"flags":0},
//       ...
//    ]}
// =====================================================================

#include "web_portal.h"
#include "web_dashboard.h"
#include "scan_event_bus.h"
#include "serial_cmd.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"

#include <Arduino.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const char* TAG = "huginn_web";
static httpd_handle_t s_server = NULL;

// WebSocket client tracker — esp_http_server gives us socket fds we can
// reuse for broadcast. Capped because the phone use-case is single-client.
#define WS_MAX_CLIENTS 4
static int s_ws_fds[WS_MAX_CLIENTS] = {-1,-1,-1,-1};
static SemaphoreHandle_t s_ws_mux = NULL;

static void ws_add(int fd) {
    if (!s_ws_mux || xSemaphoreTake(s_ws_mux, pdMS_TO_TICKS(10)) != pdTRUE) return;
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (s_ws_fds[i] == fd) { xSemaphoreGive(s_ws_mux); return; }
    }
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (s_ws_fds[i] < 0) { s_ws_fds[i] = fd; break; }
    }
    xSemaphoreGive(s_ws_mux);
}
static void ws_remove(int fd) {
    if (!s_ws_mux || xSemaphoreTake(s_ws_mux, pdMS_TO_TICKS(10)) != pdTRUE) return;
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (s_ws_fds[i] == fd) s_ws_fds[i] = -1;
    }
    xSemaphoreGive(s_ws_mux);
}

void web_portal_ws_broadcast(const char* payload, size_t len) {
    if (!s_server || !s_ws_mux) return;
    if (xSemaphoreTake(s_ws_mux, pdMS_TO_TICKS(20)) != pdTRUE) return;
    int fds[WS_MAX_CLIENTS];
    memcpy(fds, s_ws_fds, sizeof(fds));
    xSemaphoreGive(s_ws_mux);

    httpd_ws_frame_t frame = {};
    frame.type    = HTTPD_WS_TYPE_TEXT;
    frame.payload = (uint8_t*)payload;
    frame.len     = len;
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (fds[i] < 0) continue;
        esp_err_t err = httpd_ws_send_frame_async(s_server, fds[i], &frame);
        if (err != ESP_OK) {
            ws_remove(fds[i]);
        }
    }
}

// ---------- Handlers ----------

static esp_err_t handle_root(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, HUGINN_DASHBOARD_HTML, HTTPD_RESP_USE_STRLEN);
}

static const char* authStr(uint8_t a) {
    switch (a) {
        case 0: return "Open";
        case 1: return "WEP";
        case 2: return "WPA";
        case 3: return "WPA2";
        case 4: return "WPA/WPA2";
        case 5: return "WPA2-Ent";
        case 6: return "WPA3";
        default: return "?";
    }
}

static void escape_json(char* dst, size_t cap, const char* src) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 2 < cap; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '"' || c == '\\') {
            if (j + 3 >= cap) break;
            dst[j++] = '\\'; dst[j++] = c;
        } else if (c < 0x20) {
            if (j + 7 >= cap) break;
            j += snprintf(dst + j, cap - j, "\\u%04x", c);
        } else {
            dst[j++] = c;
        }
    }
    dst[j] = 0;
}

static size_t events_to_json(char* out, size_t cap, size_t limit) {
    // Snapshot newest first.
    ScanEvent evs[128];
    size_t want = (limit && limit < 128) ? limit : 128;
    size_t n = scan_event_bus_snapshot(evs, want);

    char esc_mac[40], esc_name[80];
    size_t w = 0;
    int first = 1;
    w += snprintf(out + w, cap - w, "\"events\":[");
    for (size_t i = 0; i < n && w < cap - 200; i++) {
        const ScanEvent& e = evs[i];
        escape_json(esc_mac,  sizeof(esc_mac),  e.mac);
        escape_json(esc_name, sizeof(esc_name), e.ssid_or_name);
        w += snprintf(out + w, cap - w,
                      "%s{\"type\":%u,\"ts\":%u,\"mac\":\"%s\",\"name\":\"%s\","
                      "\"rssi\":%d,\"channel\":%u,\"auth\":%u,\"flags\":%u",
                      first ? "" : ",",
                      e.type, e.ts_ms, esc_mac, esc_name,
                      (int)e.rssi, e.channel, e.auth, e.flags);
        if (e.gps_fix) {
            w += snprintf(out + w, cap - w,
                          ",\"lat\":%.7f,\"lon\":%.7f,\"alt\":%d",
                          e.lat, e.lon, (int)e.alt_m);
        }
        w += snprintf(out + w, cap - w, "}");
        first = 0;
    }
    w += snprintf(out + w, cap - w, "]");
    return w;
}

static esp_err_t handle_scans(httpd_req_t* req) {
    char qbuf[32];
    size_t limit = 128;
    if (httpd_req_get_url_query_str(req, qbuf, sizeof(qbuf)) == ESP_OK) {
        char v[16];
        if (httpd_query_key_value(qbuf, "limit", v, sizeof(v)) == ESP_OK) {
            int n = atoi(v);
            if (n > 0 && n <= 300) limit = (size_t)n;
        }
    }
    static char buf[16 * 1024];
    size_t w = 0;
    w += snprintf(buf + w, sizeof(buf) - w, "{\"seq\":%u,", scan_event_bus_seq());
    w += events_to_json(buf + w, sizeof(buf) - w, limit);
    w += snprintf(buf + w, sizeof(buf) - w, "}");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, buf, w);
}

static esp_err_t handle_status(httpd_req_t* req) {
    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "{\"seq\":%u,\"mode\":\"%s\",\"wifi\":%d,\"ble\":%d,"
        "\"flipper\":%d,\"airtag\":%d,\"skimmer\":%d}",
        scan_event_bus_seq(),
        scanModeName(g_currentMode),
        wifi_scanner_count(),
        ble_scanner_count(),
        ble_scanner_flipper_count(),
        ble_scanner_airtag_count(),
        ble_scanner_skimmer_count());
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, buf, n);
}

// Reuses the existing serial command parser so the dashboard's command
// buttons (Wardrive / Stop) speak the same vocabulary as Ragnar.
extern "C" void huginn_dispatch_cmd_line(const char* line);

static esp_err_t handle_cmd(httpd_req_t* req) {
    char body[64];
    int total = req->content_len < (int)sizeof(body) - 1 ? req->content_len : (int)sizeof(body) - 1;
    int got = 0;
    while (got < total) {
        int r = httpd_req_recv(req, body + got, total - got);
        if (r <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv failed");
            return ESP_FAIL;
        }
        got += r;
    }
    body[got] = 0;
    // Trim trailing CR/LF
    while (got > 0 && (body[got - 1] == '\r' || body[got - 1] == '\n' || body[got - 1] == ' ')) {
        body[--got] = 0;
    }
    huginn_dispatch_cmd_line(body);
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, "ok");
}

static esp_err_t handle_ws(httpd_req_t* req) {
    if (req->method == HTTP_GET) {
        // Upgrade handshake — esp_http_server has already validated it.
        int fd = httpd_req_to_sockfd(req);
        ws_add(fd);
        ESP_LOGI(TAG, "WS client connected: fd=%d", fd);
        return ESP_OK;
    }

    // Incoming frame from client — we mostly ignore but consume to keep
    // the socket healthy. Could be used for future bidirectional control.
    httpd_ws_frame_t frame = {};
    uint8_t buf[128];
    frame.payload = buf;
    esp_err_t err = httpd_ws_recv_frame(req, &frame, sizeof(buf) - 1);
    if (err != ESP_OK) {
        ws_remove(httpd_req_to_sockfd(req));
        return err;
    }
    // Echo PINGs implicitly handled by stack; nothing else to do.
    return ESP_OK;
}

// ---------- Lifecycle ----------

bool web_portal_start() {
    if (s_server) return true;
    if (!s_ws_mux) s_ws_mux = xSemaphoreCreateMutex();

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port      = 80;
    cfg.ctrl_port        = 32768;
    cfg.max_open_sockets = 7;
    cfg.lru_purge_enable = true;
    cfg.stack_size       = 8192;

    if (httpd_start(&s_server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        s_server = NULL;
        return false;
    }

    const httpd_uri_t routes[] = {
        { .uri = "/",            .method = HTTP_GET,  .handler = handle_root,   .user_ctx = NULL },
        { .uri = "/api/status",  .method = HTTP_GET,  .handler = handle_status, .user_ctx = NULL },
        { .uri = "/api/scans",   .method = HTTP_GET,  .handler = handle_scans,  .user_ctx = NULL },
        { .uri = "/api/cmd",     .method = HTTP_POST, .handler = handle_cmd,    .user_ctx = NULL },
        { .uri = "/ws",          .method = HTTP_GET,  .handler = handle_ws,     .user_ctx = NULL,
          .is_websocket = true, .handle_ws_control_frames = false, .supported_subprotocol = NULL },
    };
    for (auto& r : routes) {
        if (httpd_register_uri_handler(s_server, &r) != ESP_OK) {
            ESP_LOGW(TAG, "register %s failed", r.uri);
        }
    }
    ESP_LOGI(TAG, "web portal up on :80");
    return true;
}

void web_portal_stop() {
    if (!s_server) return;
    httpd_stop(s_server);
    s_server = NULL;
    if (s_ws_mux && xSemaphoreTake(s_ws_mux, pdMS_TO_TICKS(50)) == pdTRUE) {
        for (int i = 0; i < WS_MAX_CLIENTS; i++) s_ws_fds[i] = -1;
        xSemaphoreGive(s_ws_mux);
    }
}

bool web_portal_is_running() {
    return s_server != NULL;
}
