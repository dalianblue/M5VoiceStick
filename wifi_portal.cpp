#include "wifi_portal.h"
#include "config.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <M5Unified.h>

static DNSServer s_dnsServer;
static WebServer s_webServer(80);
static volatile bool s_configSubmitted = false;
static String s_subSSID, s_subPass, s_subApiKey;
static uint32_t s_lastWifiCheck = 0;

// ============================================================
// NVS 读写
// ============================================================
bool loadConfig(String &ssid, String &pass, String &apiKey) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) return false;
    ssid    = prefs.getString("ssid", "");
    pass    = prefs.getString("pass", "");
    apiKey  = prefs.getString("apikey", "");
    prefs.end();
    return ssid.length() > 0;
}

void saveConfig(const String &ssid, const String &pass, const String &apiKey) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return;
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.putString("apikey", apiKey);
    prefs.end();
}

String getApiKey() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) return "";
    String k = prefs.getString("apikey", "");
    prefs.end();
    return k;
}

// ============================================================
// ASR 花费统计（持久化累计 prompt_tokens）
// ============================================================
uint64_t loadTotalTokens() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) return 0;
    uint64_t t = prefs.getULong64("total_tokens", 0);
    prefs.end();
    return t;
}

void addTotalTokens(uint32_t delta) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return;
    uint64_t t = prefs.getULong64("total_tokens", 0) + delta;
    prefs.putULong64("total_tokens", t);
    prefs.end();
}

void clearTotalTokens() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return;
    prefs.putULong64("total_tokens", 0);
    prefs.end();
}

// ============================================================
// Captive Portal HTML
// ============================================================
static const char CONFIG_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>M5VoiceStick Setup</title>
<style>
body{font-family:system-ui,sans-serif;padding:24px;max-width:420px;margin:auto;background:#0c1424;color:#fff}
h2{color:#00d4ff;margin:0 0 20px}
label{display:block;margin:14px 0 4px;font-size:14px;color:#9fb3c8}
input{width:100%;padding:12px;font-size:16px;box-sizing:border-box;border:1px solid #2a3f5f;border-radius:8px;background:#1a2538;color:#fff}
button{width:100%;padding:14px;margin-top:20px;background:#00d4ff;color:#000;border:none;border-radius:8px;font-size:16px;font-weight:bold}
.note{font-size:12px;color:#9fb3c8;margin-top:16px;text-align:center}
</style></head><body>
<h2>M5VoiceStick Setup</h2>
<form action="/save" method="post">
<label>WiFi Name (SSID)</label>
<input type="text" name="ssid" placeholder="Your WiFi name" required autocomplete="off">
<label>WiFi Password</label>
<input type="password" name="pass" placeholder="Password" autocomplete="off">
<label>Zhipu BigModel API Key</label>
<input type="text" name="apikey" placeholder="xxxxxxxx.xxxxxxxxxxxx" required autocomplete="off">
<button type="submit">Save &amp; Connect</button>
</form>
<div class="note">凭据保存在设备 NVS。提交后约 10s 重启。</div>
</body></html>
)HTML";

static void handleRoot() {
    s_webServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    s_webServer.sendHeader("Pragma", "no-cache");
    s_webServer.sendHeader("Expires", "-1");
    s_webServer.send(200, "text/html", FPSTR(CONFIG_PAGE));
}

static void handleSave() {
    if (!s_webServer.hasArg("ssid") || !s_webServer.hasArg("apikey")) {
        s_webServer.send(400, "text/plain", "Missing fields");
        return;
    }
    s_subSSID   = s_webServer.arg("ssid");
    s_subPass   = s_webServer.hasArg("pass") ? s_webServer.arg("pass") : "";
    s_subApiKey = s_webServer.arg("apikey");
    s_webServer.send(200, "text/html",
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>body{font-family:system-ui;padding:30px;text-align:center;color:#0c1424}"
        "h2{color:#00a86b}</style></head><body>"
        "<h2>Saved!</h2><p>Connecting...</p></body></html>");
    s_configSubmitted = true;
}

static void handleRedirect() {
    s_webServer.sendHeader("Location", "http://192.168.4.1/", true);
    s_webServer.send(302, "text/plain", "");
}

// ============================================================
// AP 配网（阻塞）
// ============================================================
static bool runConfigPortal(uint32_t timeoutMs) {
    s_configSubmitted = false;
    auto &d = M5.Display;
    d.fillScreen(TFT_BLACK);
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setTextSize(2);
    d.setCursor(8, 6);
    d.print("Setup");
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setTextSize(1);
    d.setCursor(3, 40);
    d.print("1. Phone WiFi ->");
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setCursor(10, 54);
    d.print(AP_SSID);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(3, 74);
    d.print("2. Browser:");
    d.setTextColor(TFT_GREEN, TFT_BLACK);
    d.setCursor(10, 88);
    d.print("192.168.4.1");
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(3, 108);
    d.print("3. Fill & Save");

    Serial.println("[WiFi] AP 配置模式");
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) return false;
    IPAddress apIP = WiFi.softAPIP();

    s_dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    s_dnsServer.start(53, "*", apIP);

    s_webServer.on("/", HTTP_GET, handleRoot);
    s_webServer.on("/save", HTTP_POST, handleSave);
    s_webServer.on("/generate_204", handleRedirect);
    s_webServer.on("/gen_204", handleRedirect);
    s_webServer.on("/hotspot-detect.html", handleRedirect);
    s_webServer.on("/library/test/success.html", handleRedirect);
    s_webServer.on("/connecttest.txt", handleRedirect);
    s_webServer.on("/ncsi.txt", handleRedirect);
    s_webServer.on("/fwlink", handleRedirect);
    s_webServer.onNotFound(handleRedirect);
    s_webServer.begin();

    uint32_t start = millis();
    while (!s_configSubmitted && (millis() - start < timeoutMs)) {
        s_dnsServer.processNextRequest();
        s_webServer.handleClient();
        delay(10);
    }
    s_webServer.stop();
    s_dnsServer.stop();
    WiFi.softAPdisconnect(true);

    if (!s_configSubmitted) return false;
    saveConfig(s_subSSID, s_subPass, s_subApiKey);
    Serial.printf("[WiFi] 凭证已保存: %s\n", s_subSSID.c_str());
    return true;
}

// ============================================================
// STA 连接（阻塞）
// ============================================================
static bool staConnect(const String &ssid, const String &pass, uint32_t timeoutMs) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeoutMs) return false;
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n[WiFi] 已连接 %s, IP %s\n",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    return true;
}

// ============================================================
// 公开接口
// ============================================================
bool wifiBootConnect() {
    String ssid, pass, apiKey;
    if (loadConfig(ssid, pass, apiKey) && ssid.length() > 0) {
        Serial.printf("[WiFi] 已存凭证: %s\n", ssid.c_str());
        if (staConnect(ssid, pass, WIFI_CONNECT_TIMEOUT_MS)) return true;
    }
    // 没凭证或连不上 → AP 配网
    if (!runConfigPortal(CONFIG_PORTAL_TIMEOUT_MS)) return false;
    return staConnect(s_subSSID, s_subPass, WIFI_CONNECT_TIMEOUT_MS);
}

bool wifiReconfigure() {
    WiFi.disconnect(true, true);
    delay(100);
    uint32_t prevCpu = getCpuFrequencyMhz();
    if (prevCpu < 160) setCpuFrequencyMhz(160);
    bool ok = false;
    if (runConfigPortal(CONFIG_PORTAL_TIMEOUT_MS)) {
        ok = staConnect(s_subSSID, s_subPass, WIFI_CONNECT_TIMEOUT_MS);
    }
    if (prevCpu < 160) setCpuFrequencyMhz(prevCpu);
    return ok;
}

void wifiPollReconnect() {
    uint32_t now = millis();
    if (now - s_lastWifiCheck < WIFI_RECONNECT_INTERVAL_MS) return;
    s_lastWifiCheck = now;
    if (WiFi.status() == WL_CONNECTED) return;
    Serial.println("[WiFi] 断线，尝试重连");
    String ssid, pass, apiKey;
    if (loadConfig(ssid, pass, apiKey)) staConnect(ssid, pass, WIFI_CONNECT_TIMEOUT_MS);
}

bool isWifiConnected() { return WiFi.status() == WL_CONNECTED; }
