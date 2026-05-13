#ifndef USB_NET_H
#define USB_NET_H

#include <Arduino.h>

// =====================================================================
//  USB Ethernet-over-USB (CDC-NCM) for the iPhone/iPad dashboard.
//
//  HuginnESP normally enumerates as a single USB-CDC serial device for
//  Ragnar to read. When `usbnet on` is issued, the device re-enumerates
//  as a composite CDC-ACM + CDC-NCM device:
//
//    Interface 0..1  CDC-ACM   →  Ragnar's host (unchanged JSON stream)
//    Interface 2..3  CDC-NCM   →  iPhone/iPad USB-Ethernet adapter
//
//  iOS binds the NCM interface as a network adapter, gets a DHCP lease
//  from the firmware (192.168.7.1/24), and Safari opens the on-device
//  dashboard at http://huginn.local/.
//
//  The CDC-ACM side is byte-for-byte identical, so Ragnar keeps working.
//  Re-enumeration is forced via tud_disconnect()/tud_connect() so the
//  host re-binds with the new descriptor.
//
//  Currently implemented on the S3 only — the C5 build uses USB-Serial-
//  JTAG instead of USB-OTG and cannot add additional USB classes.
// =====================================================================

void usb_net_init();              // called from setup(); honors autostart
bool usb_net_enable();            // bring up NCM + DHCP + HTTP server
bool usb_net_disable();           // tear down, revert to CDC-only descriptor
bool usb_net_is_enabled();
bool usb_net_is_supported();      // false on C5 (no USB-OTG composite support)

// Status one-liner for `usbnet status` / `usbnet` with no arg.
// Returns a JSON status string ready to printf.
String usb_net_status_json();

#endif // USB_NET_H
