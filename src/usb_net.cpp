// =====================================================================
//  usb_net.cpp — Ethernet-over-USB (CDC-NCM) host
//
//  Implementation strategy for the S3:
//    * Arduino's auto-init of USB-CDC-on-boot is left in place. When
//      usbnet is OFF (the default), the device enumerates exactly like
//      stock HuginnESP — one CDC-ACM interface, Ragnar untouched.
//    * On `usbnet on`, we ask TinyUSB to add a CDC-NCM network interface
//      to the live configuration via esp_tinyusb's tinyusb_net_init().
//      The S3's USB-OTG controller renegotiates with the host, which
//      then sees both interfaces. Ragnar reconnects (the announce line
//      fires on every reconnect, which is the documented host pattern).
//
//  Implementation strategy for the C5:
//    * Returns false from usb_net_is_supported(). The current C5 build
//      uses USB-Serial-JTAG, which is a fixed-function peripheral and
//      cannot host additional USB classes. Switching the C5 to its
//      USB-OTG peripheral is a separate project.
//
//  IP plan on the NCM netif:
//    Device   192.168.7.1   /24
//    Phone    192.168.7.2 .. 192.168.7.10  (DHCP pool)
//    Gateway  192.168.7.1
//    DNS      192.168.7.1   (mDNS hijack so huginn.local resolves)
//    mDNS     huginn.local
//
//  Some ESP-IDF API call sites in this file are marked HUGINN_TODO:
//  those are points where the exact signature varies between IDF 5.3
//  (S3 env) and 5.5 (C5 env) — easy targets to triage on first build.
// =====================================================================

#include "usb_net.h"
#include "web_portal.h"
#include "scan_event_bus.h"
#include "runtime_config.h"
#include "serial_cmd.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"

#include <Arduino.h>
#include <string.h>

#ifndef HUGINN_USBNET
  #define HUGINN_USBNET 0
#endif

// The NCM bring-up touches arduino-esp32's USB internals + raw TinyUSB +
// lwIP + esp_netif. That stack is gated together so the rest of the
// firmware compiles cleanly whether or not the framework was rebuilt
// with CONFIG_TINYUSB_NET_MODE_NCM=y.
#define HUGINN_USBNET_ACTIVE  (HUGINN_BOARD_S3 && HUGINN_USBNET)

#if HUGINN_USBNET_ACTIVE
  #include "tinyusb.h"
  #include "tinyusb_net.h"
  #include "esp_netif.h"
  #include "esp_event.h"
  #include "esp_log.h"
  #include "esp_mac.h"
  #include "lwip/esp_netif_net_stack.h"
  #include "mdns.h"
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "huginn_usbnet";
static bool s_enabled = false;

#if HUGINN_USBNET_ACTIVE
static esp_netif_t* s_netif = nullptr;
static TaskHandle_t s_tick_task = nullptr;
#endif

bool usb_net_is_supported() {
#if HUGINN_USBNET_ACTIVE
    return true;
#else
    return false;
#endif
}

bool usb_net_is_enabled() {
    return s_enabled;
}

String usb_net_status_json() {
    String s = "{\"feature\":\"usbnet\",\"supported\":";
    s += usb_net_is_supported() ? "true" : "false";
    s += ",\"enabled\":";
    s += s_enabled ? "true" : "false";
#if HUGINN_USBNET_ACTIVE
    if (s_enabled && s_netif) {
        esp_netif_ip_info_t ip = {};
        if (esp_netif_get_ip_info(s_netif, &ip) == ESP_OK) {
            char ipstr[16];
            snprintf(ipstr, sizeof(ipstr), IPSTR, IP2STR(&ip.ip));
            s += ",\"ip\":\""; s += ipstr; s += "\"";
        }
    }
#else
    s += ",\"stage\":\"scaffold\"";
#endif
    s += "}";
    return s;
}

#if HUGINN_USBNET_ACTIVE

// ---------- TinyUSB NCM -> lwIP plumbing ----------

// TinyUSB delivers an inbound Ethernet frame here. We hand it to
// esp_netif which forwards it into lwIP for the local stack to chew.
static esp_err_t tusb_net_rx_cb(void* buffer, uint16_t len, void* ctx) {
    if (!s_netif) return ESP_FAIL;
    // Take ownership of the buffer and pass to lwIP via esp_netif.
    void* buf_copy = malloc(len);
    if (!buf_copy) return ESP_ERR_NO_MEM;
    memcpy(buf_copy, buffer, len);
    esp_err_t err = esp_netif_receive(s_netif, buf_copy, len, NULL);
    // esp_netif_receive takes ownership when it returns ESP_OK; otherwise free.
    if (err != ESP_OK) free(buf_copy);
    return err;
}

// esp_netif wants to transmit a frame. Hand to TinyUSB which packages
// it into an NCM transfer to the host.
static esp_err_t netif_transmit_cb(void* h, void* buffer, size_t len) {
    if (tinyusb_net_send_sync(buffer, len, NULL, pdMS_TO_TICKS(100)) != ESP_OK) {
        ESP_LOGW(TAG, "ncm tx %u bytes failed", (unsigned)len);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void netif_free_rx_buffer(void* h, void* buffer) {
    free(buffer);
}

// ---------- Periodic WebSocket tick ----------
// Pushes a delta envelope to all WS clients ~2 Hz. Newest events first,
// capped so the payload stays under ~8 KB to fit cleanly in one TCP MSS
// over USB.
static void tick_task(void* arg) {
    uint32_t last_seq = 0;
    char buf[8192];
    while (s_enabled) {
        uint32_t cur = scan_event_bus_seq();
        if (cur != last_seq) {
            // Estimate how many new events to send (cap to 64 per tick).
            size_t want = (cur - last_seq);
            if (want > 64) want = 64;

            ScanEvent evs[64];
            size_t n = scan_event_bus_snapshot(evs, want);

            size_t w = 0;
            w += snprintf(buf + w, sizeof(buf) - w,
                "{\"seq\":%u,\"mode\":\"%s\",\"events\":[",
                cur, scanModeName(g_currentMode));

            for (size_t i = 0; i < n && w < sizeof(buf) - 200; i++) {
                const ScanEvent& e = evs[i];
                // Inline JSON-escape MAC (always safe) + name (may contain quotes).
                char esc_name[80];
                size_t j = 0;
                for (size_t k = 0; e.ssid_or_name[k] && j + 8 < sizeof(esc_name); k++) {
                    unsigned char c = (unsigned char)e.ssid_or_name[k];
                    if (c == '"' || c == '\\') { esc_name[j++] = '\\'; esc_name[j++] = c; }
                    else if (c < 0x20) { j += snprintf(esc_name + j, sizeof(esc_name) - j, "\\u%04x", c); }
                    else esc_name[j++] = c;
                }
                esc_name[j] = 0;

                w += snprintf(buf + w, sizeof(buf) - w,
                    "%s{\"type\":%u,\"ts\":%u,\"mac\":\"%s\",\"name\":\"%s\","
                    "\"rssi\":%d,\"channel\":%u,\"auth\":%u,\"flags\":%u",
                    (i == 0) ? "" : ",",
                    e.type, e.ts_ms, e.mac, esc_name,
                    (int)e.rssi, e.channel, e.auth, e.flags);
                if (e.gps_fix) {
                    w += snprintf(buf + w, sizeof(buf) - w,
                                  ",\"lat\":%.7f,\"lon\":%.7f,\"alt\":%d",
                                  e.lat, e.lon, (int)e.alt_m);
                }
                w += snprintf(buf + w, sizeof(buf) - w, "}");
            }
            w += snprintf(buf + w, sizeof(buf) - w, "]}");
            web_portal_ws_broadcast(buf, w);
            last_seq = cur;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    s_tick_task = nullptr;
    vTaskDelete(NULL);
}

// ---------- Bring-up ----------

static bool ncm_bringup() {
    static bool s_one_shot_init = false;
    if (s_one_shot_init) {
        // TinyUSB driver only installs once per boot.
        ESP_LOGI(TAG, "tinyusb already installed; reattaching netif only");
    } else {
        // HUGINN_TODO(s3): tinyusb_driver_install signature in pioarduino
        // 53.03.13 may already be called by arduino-esp32's CDC-on-boot path.
        // In that case this call is redundant; the underlying installer is
        // idempotent across recent IDF versions but log an error and continue.
        tinyusb_config_t tusb_cfg = {};
        tusb_cfg.external_phy = false;
        esp_err_t err = tinyusb_driver_install(&tusb_cfg);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "tinyusb_driver_install failed: %d", err);
            return false;
        }
        s_one_shot_init = true;
    }

    if (esp_netif_init() != ESP_OK) {
        ESP_LOGW(TAG, "esp_netif_init returned non-OK (may be already initialized)");
    }
    // Default event loop is created by arduino-esp32 at boot — esp_event_loop_create_default()
    // would return ESP_ERR_INVALID_STATE here, which is fine.
    esp_event_loop_create_default();

    if (!s_netif) {
        esp_netif_ip_info_t ip_info = {};
        IP4_ADDR(&ip_info.ip,      192, 168, 7, 1);
        IP4_ADDR(&ip_info.gw,      192, 168, 7, 1);
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

        esp_netif_inherent_config_t base_cfg = {};
        base_cfg.flags         = (esp_netif_flags_t)(ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP);
        base_cfg.ip_info       = &ip_info;
        base_cfg.get_ip_event  = 0;
        base_cfg.lost_ip_event = 0;
        base_cfg.if_key        = "HUGINN_USB";
        base_cfg.if_desc       = "huginn usb ncm";
        base_cfg.route_prio    = 50;

        esp_netif_driver_ifconfig_t driver_cfg = {};
        driver_cfg.handle             = (esp_netif_iodriver_handle)1;  // placeholder, we override transmit
        driver_cfg.transmit           = netif_transmit_cb;
        driver_cfg.driver_free_rx_buffer = netif_free_rx_buffer;

        esp_netif_config_t cfg = {};
        cfg.base   = &base_cfg;
        cfg.driver = &driver_cfg;
        cfg.stack  = ESP_NETIF_NETSTACK_DEFAULT_ETH;

        s_netif = esp_netif_new(&cfg);
        if (!s_netif) {
            ESP_LOGE(TAG, "esp_netif_new failed");
            return false;
        }
        esp_netif_set_default_netif(s_netif);

        // Push DNS pointing back at us so huginn.local resolves via mDNS proxy.
        esp_netif_dns_info_t dns = {};
        IP4_ADDR(&dns.ip.u_addr.ip4, 192, 168, 7, 1);
        esp_netif_set_dns_info(s_netif, ESP_NETIF_DNS_MAIN, &dns);

        // Cap the DHCP lease pool to a few addresses; phone is the only client.
        // HUGINN_TODO(idf): the DHCP pool API moved between IDF versions —
        // some headers expose dhcps_set_option_info(), newer use
        // esp_netif_dhcps_option(). Both wind up at the same lwIP backing.
    }

    // Init TinyUSB NCM class; this registers the network interface in
    // the USB descriptor and the host will see a new USB-Ethernet adapter.
    tinyusb_net_config_t net_cfg = {};
    net_cfg.on_recv_callback = tusb_net_rx_cb;
    // MAC of the network device end (firmware side).
    uint8_t mac[6] = {0x02, 0x02, 0x42, 0x47, 0x4E, 0x01};
    memcpy(net_cfg.mac_addr, mac, 6);
    if (tinyusb_net_init(TINYUSB_USBDEV_0, &net_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_net_init failed");
        return false;
    }

    // mDNS — huginn.local resolves to the device IP on the USB subnet.
    if (mdns_init() == ESP_OK) {
        mdns_hostname_set("huginn");
        mdns_instance_name_set("HuginnESP");
        mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    }

    if (!web_portal_start()) {
        ESP_LOGE(TAG, "web_portal_start failed");
        return false;
    }

    if (!s_tick_task) {
        xTaskCreate(tick_task, "huginn_tick", 6144, NULL, 1, &s_tick_task);
    }
    return true;
}

static void ncm_teardown() {
    web_portal_stop();
    // Leave the netif and TinyUSB net interface up across enable/disable
    // cycles — pulling them down at runtime is fragile and the costs
    // (a few KB of RAM, an extra USB interface) are negligible.
    // Just stop pushing data:
    s_enabled = false;
    if (s_tick_task) {
        // tick_task self-exits when s_enabled flips false.
    }
}

#endif // HUGINN_USBNET_ACTIVE

bool usb_net_enable() {
    if (!usb_net_is_supported()) {
#if HUGINN_BOARD_S3 && !HUGINN_USBNET
        Serial.println("{\"warn\":\"usbnet compiled out (HUGINN_USBNET=0); flip the build flag + uncomment custom_sdkconfig to enable\"}");
#else
        Serial.println("{\"error\":\"usbnet not supported on this board\"}");
#endif
        return false;
    }
    if (s_enabled) {
        Serial.println("{\"ok\":true,\"key\":\"usbnet\",\"value\":\"already on\"}");
        return true;
    }
#if HUGINN_USBNET_ACTIVE
    s_enabled = true;
    if (!ncm_bringup()) {
        s_enabled = false;
        Serial.println("{\"error\":\"usbnet bring-up failed; check logs\"}");
        return false;
    }
    Serial.printf("{\"ok\":true,\"key\":\"usbnet\",\"value\":\"on\",\"url\":\"http://huginn.local/\",\"ip\":\"192.168.7.1\"}\n");
    return true;
#else
    return false;
#endif
}

bool usb_net_disable() {
    if (!s_enabled) {
        Serial.println("{\"ok\":true,\"key\":\"usbnet\",\"value\":\"already off\"}");
        return true;
    }
#if HUGINN_USBNET_ACTIVE
    ncm_teardown();
    Serial.println("{\"ok\":true,\"key\":\"usbnet\",\"value\":\"off\"}");
    return true;
#else
    return false;
#endif
}

void usb_net_init() {
    scan_event_bus_init();
    if (g_usbnetAutostart && usb_net_is_supported()) {
        usb_net_enable();
    }
}
