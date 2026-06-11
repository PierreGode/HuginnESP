#include "runtime_config.h"
#include "config.h"
#include <vector>

static SemaphoreHandle_t  s_skimmerMutex = nullptr;
static std::vector<String> s_skimmerNames;

static String normalizeName(const String& in) {
    String out;
    out.reserve(in.length());
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        if (c == ' ' || c == '-' || c == '_' || c == '.') continue;
        out += (char)toupper((unsigned char)c);
    }
    return out;
}

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
    const String nrm = normalizeName(name);
    bool found = false;
    if (xSemaphoreTake(s_skimmerMutex, portMAX_DELAY) == pdTRUE) {
        for (const auto& configured : s_skimmerNames) {
            const String cn = normalizeName(configured);
            if (cn.length() == 0) continue;
            if (nrm == cn || nrm.startsWith(cn) || nrm.indexOf(cn) >= 0) {
                found = true;
                break;
            }
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
