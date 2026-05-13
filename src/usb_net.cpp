// =====================================================================
//  usb_net.cpp — USB Ethernet-over-USB (CDC-NCM) host
//
//  Stage 1 (this commit): command surface + autostart wiring only.
//  The actual TinyUSB NCM interface, lwIP netif, DHCP server, and
//  HTTP server land in subsequent commits so each piece can be
//  verified on hardware before the next one is layered on.
//
//  Calling usb_net_enable() today prints a structured "pending"
//  status line so a host can already detect the feature surface.
// =====================================================================

#include "usb_net.h"
#include "runtime_config.h"

static bool s_enabled = false;

bool usb_net_is_supported() {
#if HUGINN_BOARD_S3
    return true;
#else
    // C5: USB-Serial-JTAG only in the current build; composite NCM
    // would require switching the C5 to USB-OTG + TinyUSB, which is
    // out of scope for the S3-first rollout.
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
    s += ",\"stage\":\"scaffold\"}";
    return s;
}

bool usb_net_enable() {
    if (!usb_net_is_supported()) {
        Serial.println("{\"error\":\"usbnet not supported on this board\"}");
        return false;
    }
    if (s_enabled) {
        Serial.println("{\"ok\":true,\"key\":\"usbnet\",\"value\":\"already on\"}");
        return true;
    }
    // TODO(next commit): bring up TinyUSB NCM interface, force re-enum,
    // attach lwIP netif, start DHCP server, start esp_http_server, mDNS.
    Serial.println("{\"warn\":\"usbnet enable pending — NCM stack lands in the next commit\"}");
    s_enabled = true;
    return true;
}

bool usb_net_disable() {
    if (!s_enabled) {
        Serial.println("{\"ok\":true,\"key\":\"usbnet\",\"value\":\"already off\"}");
        return true;
    }
    // TODO(next commit): stop HTTP server, mDNS, DHCP, lwIP netif,
    // tear down NCM interface, force re-enum back to CDC-only.
    Serial.println("{\"warn\":\"usbnet disable pending — NCM stack lands in the next commit\"}");
    s_enabled = false;
    return true;
}

void usb_net_init() {
    if (g_usbnetAutostart && usb_net_is_supported()) {
        usb_net_enable();
    }
}
