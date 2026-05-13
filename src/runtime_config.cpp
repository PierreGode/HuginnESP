#include "runtime_config.h"
#include "config.h"
#include <vector>

volatile uint32_t g_wifiScanDurationMs = WIFI_SCAN_DURATION;
volatile uint32_t g_bleSpamThreshold   = BLE_SPAM_THRESHOLD;
volatile uint32_t g_wardriveWifiMs     = WARDRIVE_WIFI_DURATION_MS;
volatile uint32_t g_wardriveBleMs      = WARDRIVE_BLE_DURATION_MS;
volatile uint32_t g_usbnetAutostart    = 0;  // default off so Ragnar setups are unchanged

static SemaphoreHandle_t  s_skimmerMutex = nullptr;
static std::vector<String> s_skimmerNames;

static void seedDefaultSkimmerNames() {
    for (int i = 0; SKIMMER_NAMES_DEFAULT[i] != nullptr; i++) {
        s_skimmerNames.emplace_back(SKIMMER_NAMES_DEFAULT[i]);
    }
}

void runtime_config_init() {
    if (s_skimmerMutex) return;
    s_skimmerMutex = xSemaphoreCreateMutex();
    seedDefaultSkimmerNames();
}

bool isSkimmerName(const String& name) {
    if (!s_skimmerMutex || name.length() == 0) return false;
    bool found = false;
    if (xSemaphoreTake(s_skimmerMutex, portMAX_DELAY) == pdTRUE) {
        for (const auto& n : s_skimmerNames) {
            if (name.equalsIgnoreCase(n.c_str())) { found = true; break; }
        }
        xSemaphoreGive(s_skimmerMutex);
    }
    return found;
}

String getSkimmerNamesCsv() {
    String out;
    if (!s_skimmerMutex) return out;
    if (xSemaphoreTake(s_skimmerMutex, portMAX_DELAY) == pdTRUE) {
        for (size_t i = 0; i < s_skimmerNames.size(); i++) {
            if (i) out += ",";
            out += s_skimmerNames[i];
        }
        xSemaphoreGive(s_skimmerMutex);
    }
    return out;
}

static void setSkimmerNamesFromCsv(const String& csv) {
    std::vector<String> parsed;
    int start = 0;
    int len = (int)csv.length();
    while (start <= len) {
        int comma = csv.indexOf(',', start);
        if (comma < 0) comma = len;
        String item = csv.substring(start, comma);
        item.trim();
        if (item.length() > 0) parsed.push_back(item);
        start = comma + 1;
    }
    if (xSemaphoreTake(s_skimmerMutex, portMAX_DELAY) == pdTRUE) {
        s_skimmerNames.swap(parsed);
        xSemaphoreGive(s_skimmerMutex);
    }
}

static void printErr(const char* msg) {
    Serial.printf("[HUGINN] {\"error\":\"%s\"}\n", msg);
}
static void printOkUint(const char* key, uint32_t v) {
    Serial.printf("[HUGINN] {\"ok\":true,\"key\":\"%s\",\"value\":%u}\n", key, (unsigned)v);
}
static void printOkStr(const char* key, const String& v) {
    Serial.printf("[HUGINN] {\"ok\":true,\"key\":\"%s\",\"value\":\"%s\"}\n", key, v.c_str());
}

static bool parseUint(const String& s, uint32_t& out) {
    if (s.length() == 0) return false;
    for (size_t i = 0; i < s.length(); i++) {
        if (!isDigit(s[i])) return false;
    }
    out = (uint32_t)s.toInt();
    return true;
}

static bool handleSet(const String& key, const String& value) {
    if (key == "wifi_scan_duration_ms") {
        uint32_t v;
        if (!parseUint(value, v) || v < 500 || v > 600000) {
            printErr("bad value (range 500..600000)");
            return true;
        }
        g_wifiScanDurationMs = v;
        printOkUint(key.c_str(), v);
        return true;
    }
    if (key == "ble_spam_threshold") {
        uint32_t v;
        if (!parseUint(value, v) || v < 1 || v > 10000) {
            printErr("bad value (range 1..10000)");
            return true;
        }
        g_bleSpamThreshold = v;
        printOkUint(key.c_str(), v);
        return true;
    }
    if (key == "wardrive_wifi_ms") {
        uint32_t v;
        if (!parseUint(value, v) || v < 1000 || v > 30000) {
            printErr("bad value (range 1000..30000)");
            return true;
        }
        g_wardriveWifiMs = v;
        printOkUint(key.c_str(), v);
        return true;
    }
    if (key == "wardrive_ble_ms") {
        uint32_t v;
        if (!parseUint(value, v) || v < 500 || v > 30000) {
            printErr("bad value (range 500..30000)");
            return true;
        }
        g_wardriveBleMs = v;
        printOkUint(key.c_str(), v);
        return true;
    }
    if (key == "skimmer_names") {
        setSkimmerNamesFromCsv(value);
        printOkStr(key.c_str(), getSkimmerNamesCsv());
        return true;
    }
    if (key == "usbnet_autostart") {
        uint32_t v;
        if (!parseUint(value, v) || v > 1) {
            printErr("bad value (0 or 1)");
            return true;
        }
        g_usbnetAutostart = v;
        printOkUint(key.c_str(), v);
        return true;
    }
    printErr("unknown key");
    return true;
}

static bool handleGet(const String& key) {
    if (key == "wifi_scan_duration_ms") { printOkUint(key.c_str(), g_wifiScanDurationMs); return true; }
    if (key == "ble_spam_threshold")    { printOkUint(key.c_str(), g_bleSpamThreshold);   return true; }
    if (key == "wardrive_wifi_ms")      { printOkUint(key.c_str(), g_wardriveWifiMs);     return true; }
    if (key == "wardrive_ble_ms")       { printOkUint(key.c_str(), g_wardriveBleMs);      return true; }
    if (key == "skimmer_names")         { printOkStr (key.c_str(), getSkimmerNamesCsv()); return true; }
    if (key == "usbnet_autostart")      { printOkUint(key.c_str(), g_usbnetAutostart);    return true; }
    if (key == "all") {
        printOkUint("wifi_scan_duration_ms", g_wifiScanDurationMs);
        printOkUint("ble_spam_threshold",    g_bleSpamThreshold);
        printOkUint("wardrive_wifi_ms",      g_wardriveWifiMs);
        printOkUint("wardrive_ble_ms",       g_wardriveBleMs);
        printOkStr ("skimmer_names",         getSkimmerNamesCsv());
        printOkUint("usbnet_autostart",      g_usbnetAutostart);
        return true;
    }
    printErr("unknown key");
    return true;
}

bool runtime_config_handle(const String& line) {
    if (line.startsWith("set ")) {
        int sp = line.indexOf(' ', 4);
        if (sp < 0) { printErr("usage: set <key> <value>"); return true; }
        String key = line.substring(4, sp);
        String val = line.substring(sp + 1);
        key.trim(); val.trim();
        if (key.length() == 0) { printErr("missing key"); return true; }
        return handleSet(key, val);
    }
    if (line.startsWith("get ")) {
        String key = line.substring(4);
        key.trim();
        if (key.length() == 0) { printErr("missing key"); return true; }
        return handleGet(key);
    }
    return false;
}
