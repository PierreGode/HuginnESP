#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

#include <stdbool.h>
#include <stdint.h>

// =====================================================================
//  Web portal — HTTP/1.1 + WebSocket served on the USB-NCM netif.
//
//  Endpoints:
//    GET  /                     -> embedded mobile dashboard
//    GET  /api/status           -> JSON status (board, mode, counts, seq)
//    GET  /api/scans?limit=N    -> JSON snapshot of most-recent events
//    POST /api/cmd              -> accept any serial command verb (body=verb)
//    GET  /ws                   -> WebSocket: 1 Hz status + delta events
//
//  Bound to port 80 on the lwIP netif owned by usb_net.cpp. The portal
//  starts up only when usb_net_enable() is called and tears down when
//  usb_net_disable() is called. While the portal is down, no firmware
//  resources are spent on HTTP/WebSocket.
// =====================================================================

#ifdef __cplusplus
extern "C" {
#endif

bool web_portal_start();   // called by usb_net.cpp once the netif has a lease
void web_portal_stop();
bool web_portal_is_running();

// Broadcast a single line to all connected WebSocket clients. Used by
// usb_net.cpp's periodic tick to push delta events + a status frame.
void web_portal_ws_broadcast(const char* payload, size_t len);

#ifdef __cplusplus
}
#endif

#endif // WEB_PORTAL_H
