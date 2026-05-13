// =====================================================================
//  usb_net.cpp — Ethernet-over-USB (CDC-NCM) host
//
//  Design:
//    * The S3's USB-OTG controller is shared by arduino-esp32's CDC-ACM
//      port (managed automatically when ARDUINO_USB_CDC_ON_BOOT=1) and
//      our extra CDC-NCM interface, which we inject via arduino-esp32's
//      tinyusb_enable_interface2() before USB::begin() runs.
//    * Registration happens in a __attribute__((constructor)) function
//      so it executes before arduino-esp32's USB init reads the enabled
//      interface mask.
//    * Frames flowing from the host arrive in tud_network_recv_cb() and
//      are handed to lwIP via an esp_netif "Ethernet" netif. Frames lwIP
//      wants to send go out through netif_transmit() -> tud_network_xmit.
//    * esp_netif's built-in DHCP server hands the phone an address in
//      192.168.7.0/24. mDNS responder publishes "huginn.local" so Safari
//      can find us without typing an IP.
//
//  The whole stack is gated on HUGINN_USBNET_ACTIVE so the file compiles
//  cleanly when the framework rebuild with CONFIG_TINYUSB_NET_MODE_NCM
//  hasn't been opted into.
//
//  C5 is unsupported: its current build uses the USB-Serial-JTAG fixed-
//  function peripheral and cannot host additional USB classes.
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

#define HUGINN_USBNET_ACTIVE  (HUGINN_BOARD_S3 && HUGINN_USBNET)

#if HUGINN_USBNET_ACTIVE
  #include "esp32-hal-tinyusb.h"
  #include "tusb.h"
  #include "class/net/net_device.h"
  #include "esp_netif.h"
  #include "esp_event.h"
  #include "esp_log.h"
  #include "esp_mac.h"
  #include "lwip/esp_netif_net_stack.h"
  #include "mdns.h"
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static const char* TAG = "huginn_usbnet";
static bool s_enabled = false;

#if HUGINN_USBNET_ACTIVE
static esp_netif_t*       s_netif        = nullptr;
static TaskHandle_t       s_tick_task    = nullptr;
static SemaphoreHandle_t  s_tx_mux       = nullptr;
static uint8_t            s_mac_str_idx  = 0;

// TinyUSB net class expects this symbol with C linkage at fixed name.
extern "C" {
uint8_t tud_network_mac_address[6] = { 0x02, 0x02, 0x42, 0x47, 0x4E, 0x01 };
}
#endif

// ---------- Public surface ----------

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

// ---------- USB descriptor injection ----------
//
// Called by arduino-esp32 when it's stitching together the configuration
// descriptor. We emit the TinyUSB-provided NCM template (8B IAD + 9+5+5+13+6+7
// control interface = 53 bytes, plus 9+9+7+7 data interfaces = 32 bytes ⇒
// TUD_CDC_NCM_DESC_LEN bytes total). The macro fills in interface numbers,
// endpoint addresses, and the iMACAddress string index.

static uint16_t ncm_descriptor_cb(uint8_t* dst, uint8_t* itf) {
    uint8_t itf_num    = *itf;
    uint8_t ep_notif   = tinyusb_get_free_in_endpoint();
    uint8_t ep_in      = tinyusb_get_free_in_endpoint();
    uint8_t ep_out     = tinyusb_get_free_out_endpoint();
    if (!ep_notif || !ep_in || !ep_out) {
        ESP_LOGE(TAG, "ncm_descriptor_cb: out of endpoints (notif=%u in=%u out=%u)",
                 ep_notif, ep_in, ep_out);
        return 0;
    }

    const uint8_t desc[] = {
        TUD_CDC_NCM_DESCRIPTOR(itf_num, /*desc_stridx*/ 0, /*mac_stridx*/ s_mac_str_idx,
                               0x80 | ep_notif, /*ep_notif_size*/ 8,
                               ep_out, 0x80 | ep_in, /*ep_size*/ 64,
                               /*maxsegmentsize*/ CFG_TUD_NET_MTU)
    };
    memcpy(dst, desc, sizeof(desc));
    *itf += 2;                       // NCM consumes 2 interface numbers
    return (uint16_t)sizeof(desc);
}

// Runs before main() / before arduino-esp32 reads the enabled-interface
// mask, so by the time USB::begin() builds the descriptor, NCM is in it.
__attribute__((constructor))
static void huginn_register_ncm(void) {
    // Stamp MAC: 02:xx:xx with the bottom 3 bytes from the EFUSE so two
    // devices on the same Pi don't fight for the same MAC.
    uint8_t base[6] = {0};
    esp_efuse_mac_get_default(base);
    tud_network_mac_address[0] = 0x02;
    tud_network_mac_address[1] = base[3];
    tud_network_mac_address[2] = base[4];
    tud_network_mac_address[3] = base[5];
    tud_network_mac_address[4] = 0x4E;
    tud_network_mac_address[5] = 0x01;

    // Register the MAC string descriptor; iMACAddress in the ECM
    // functional descriptor must point here.
    char mac_str[13];
    snprintf(mac_str, sizeof(mac_str), "%02X%02X%02X%02X%02X%02X",
             tud_network_mac_address[0], tud_network_mac_address[1],
             tud_network_mac_address[2], tud_network_mac_address[3],
             tud_network_mac_address[4], tud_network_mac_address[5]);
    s_mac_str_idx = (uint8_t)tinyusb_add_string_descriptor(mac_str);

    // Ask arduino-esp32 to call us back when it builds the config
    // descriptor. USB_INTERFACE_VENDOR is the most flexible slot — the
    // descriptor bytes carry their own bInterfaceClass=0x02 (Comm) so
    // TinyUSB's NCM driver still picks them up.
    tinyusb_enable_interface2(USB_INTERFACE_VENDOR,
                              TUD_CDC_NCM_DESC_LEN,
                              ncm_descriptor_cb,
                              /*reserve_endpoints*/ false);
}

// ---------- TinyUSB net <-> lwIP bridge ----------

// Host -> firmware. Take ownership of the frame, hand to lwIP.
extern "C" bool tud_network_recv_cb(const uint8_t* src, uint16_t size) {
    if (!s_netif) {
        tud_network_recv_renew();
        return true;
    }
    void* buf = malloc(size);
    if (!buf) {
        // Drop and keep the link healthy.
        tud_network_recv_renew();
        return true;
    }
    memcpy(buf, src, size);
    if (esp_netif_receive(s_netif, buf, size, NULL) != ESP_OK) {
        free(buf);
    }
    tud_network_recv_renew();
    return true;
}

// Firmware -> host. TinyUSB pulls the next frame from us.
// We stuff (buffer, length) into the (ref, arg) cookie pair on the xmit
// call and just memcpy it out here.
extern "C" uint16_t tud_network_xmit_cb(uint8_t* dst, void* ref, uint16_t arg) {
    if (ref && arg) memcpy(dst, ref, arg);
    return arg;
}

extern "C" void tud_network_init_cb(void) {
    // Nothing — descriptor + buffers already wired up.
}

// esp_netif driver: lwIP wants to send a frame.
static esp_err_t netif_transmit(void* h, void* buffer, size_t len) {
    if (!s_tx_mux) return ESP_FAIL;
    if (xSemaphoreTake(s_tx_mux, pdMS_TO_TICKS(100)) != pdTRUE) return ESP_FAIL;
    int spins = 100;  // up to ~50 ms for the host to drain
    while (!tud_network_can_xmit(len) && spins-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (spins <= 0) {
        xSemaphoreGive(s_tx_mux);
        return ESP_ERR_TIMEOUT;
    }
    tud_network_xmit(buffer, (uint16_t)len);
    xSemaphoreGive(s_tx_mux);
    return ESP_OK;
}

static void netif_free_rx_buffer(void* h, void* buffer) {
    free(buffer);
}

// ---------- WebSocket tick ----------

static void tick_task(void*) {
    uint32_t last_seq = 0;
    char buf[8192];
    while (s_enabled) {
        uint32_t cur = scan_event_bus_seq();
        if (cur != last_seq) {
            size_t want = (cur - last_seq);
            if (want > 64) want = 64;

            ScanEvent evs[64];
            size_t n = scan_event_bus_snapshot(evs, want);

            size_t w = 0;
            w += snprintf(buf + w, sizeof(buf) - w,
                "{\"seq\":%u,\"mode\":\"%s\",\"events\":[",
                (unsigned)cur, scanModeName(g_currentMode));

            for (size_t i = 0; i < n && w < sizeof(buf) - 200; i++) {
                const ScanEvent& e = evs[i];
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
                    e.type, (unsigned)e.ts_ms, e.mac, esc_name,
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

// ---------- Bring-up / tear-down ----------

static bool ncm_bringup() {
    if (!s_tx_mux) s_tx_mux = xSemaphoreCreateMutex();

    // esp_netif core may already be initialized by arduino-esp32's WiFi
    // bring-up path; both calls are idempotent (return ESP_ERR_INVALID_STATE
    // when already done) and we don't propagate that as an error.
    esp_netif_init();
    esp_event_loop_create_default();

    if (!s_netif) {
        esp_netif_ip_info_t ip_info = {};
        IP4_ADDR(&ip_info.ip,      192, 168, 7, 1);
        IP4_ADDR(&ip_info.gw,      192, 168, 7, 1);
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

        esp_netif_inherent_config_t base_cfg = {};
        base_cfg.flags         = (esp_netif_flags_t)(ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP);
        base_cfg.ip_info       = &ip_info;
        base_cfg.if_key        = "HUGINN_USB";
        base_cfg.if_desc       = "huginn usb ncm";
        base_cfg.route_prio    = 50;

        esp_netif_driver_ifconfig_t driver_cfg = {};
        driver_cfg.handle                  = (esp_netif_iodriver_handle)1;
        driver_cfg.transmit                = netif_transmit;
        driver_cfg.driver_free_rx_buffer   = netif_free_rx_buffer;

        esp_netif_config_t cfg = {};
        cfg.base   = &base_cfg;
        cfg.driver = &driver_cfg;
        cfg.stack  = ESP_NETIF_NETSTACK_DEFAULT_ETH;

        s_netif = esp_netif_new(&cfg);
        if (!s_netif) {
            ESP_LOGE(TAG, "esp_netif_new failed");
            return false;
        }
        // Tell esp_netif our MAC so DHCP/ARP work correctly.
        esp_netif_set_mac(s_netif, tud_network_mac_address);

        // Push DNS so phone clients can resolve huginn.local without
        // configuring extra DNS — we run an mDNS responder on the same
        // gateway address.
        esp_netif_dns_info_t dns = {};
        dns.ip.u_addr.ip4.addr = ip_info.gw.addr;
        dns.ip.type = ESP_IPADDR_TYPE_V4;
        esp_netif_set_dns_info(s_netif, ESP_NETIF_DNS_MAIN, &dns);

        esp_netif_action_start(s_netif, NULL, 0, NULL);
    }

    static bool s_mdns_up = false;
    if (!s_mdns_up) {
        if (mdns_init() == ESP_OK) {
            mdns_hostname_set("huginn");
            mdns_instance_name_set("HuginnESP");
            mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
            s_mdns_up = true;
        }
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
    // Leave the netif and TinyUSB descriptor in place across enable/
    // disable cycles. The mDNS service stays advertised; HTTP server is
    // gone so probes get connection refused, which is the right signal.
    // tick_task self-exits when s_enabled goes false.
}

#endif // HUGINN_USBNET_ACTIVE

bool usb_net_enable() {
    if (!usb_net_is_supported()) {
#if HUGINN_BOARD_S3 && !HUGINN_USBNET
        Serial.println("[HUGINN] {\"warn\":\"usbnet compiled out (HUGINN_USBNET=0); flip the build flag + uncomment custom_sdkconfig to enable\"}");
#else
        Serial.println("[HUGINN] {\"error\":\"usbnet not supported on this board\"}");
#endif
        return false;
    }
    if (s_enabled) {
        Serial.println("[HUGINN] {\"ok\":true,\"key\":\"usbnet\",\"value\":\"already on\"}");
        return true;
    }
#if HUGINN_USBNET_ACTIVE
    s_enabled = true;
    if (!ncm_bringup()) {
        s_enabled = false;
        Serial.println("[HUGINN] {\"error\":\"usbnet bring-up failed; check logs\"}");
        return false;
    }
    Serial.printf("[HUGINN] {\"ok\":true,\"key\":\"usbnet\",\"value\":\"on\",\"url\":\"http://huginn.local/\",\"ip\":\"192.168.7.1\"}\n");
    return true;
#else
    return false;
#endif
}

bool usb_net_disable() {
    if (!s_enabled) {
        Serial.println("[HUGINN] {\"ok\":true,\"key\":\"usbnet\",\"value\":\"already off\"}");
        return true;
    }
#if HUGINN_USBNET_ACTIVE
    s_enabled = false;
    ncm_teardown();
    Serial.println("[HUGINN] {\"ok\":true,\"key\":\"usbnet\",\"value\":\"off\"}");
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
