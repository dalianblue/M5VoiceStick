#include "asr_client.h"
#include "config.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <string.h>

// ponytail: setInsecure 跳过证书校验，简化分发，ESP32 上常规做法
// 升级路径：硬编码 Sectigo/Harica 根证书指纹，证书轮换时刷固件

static const char BOUNDARY[] = "----M5VoiceStickBoundary7p1";

bool asrTranscribe(const uint8_t *wavBytes, size_t wavLen,
                   const String &apiKey, String &outText, int &httpCode,
                   uint32_t &outTokens) {
    Serial.printf("[ASR] 上传 %u bytes WAV\n", (unsigned)wavLen);
    httpCode = 0;
    outText = "";
    outTokens = 0;

    // 拼装 multipart body 到 PSRAM
    String head =
        String("--") + BOUNDARY + "\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
        ASR_MODEL "\r\n"
        + String("--") + BOUNDARY + "\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n";
    String tail = String("\r\n--") + BOUNDARY + "--\r\n";

    size_t totalLen = head.length() + wavLen + tail.length();
    uint8_t *body = (uint8_t*)heap_caps_malloc(totalLen, MALLOC_CAP_SPIRAM);
    if (!body) {
        Serial.println("[ASR] PSRAM 分配失败");
        httpCode = -1;
        return false;
    }
    memcpy(body, head.c_str(), head.length());
    memcpy(body + head.length(), wavBytes, wavLen);
    memcpy(body + head.length() + wavLen, tail.c_str(), tail.length());

    WiFiClientSecure tls;
    tls.setInsecure();
    HTTPClient http;
    if (!http.begin(tls, ASR_ENDPOINT)) {
        free(body);
        httpCode = -2;
        return false;
    }
    http.addHeader("Authorization", "Bearer " + apiKey);
    String ct = "multipart/form-data; boundary=";
    ct += BOUNDARY;
    http.addHeader("Content-Type", ct);
    http.setTimeout(ASR_TIMEOUT_MS);

    httpCode = http.sendRequest("POST", body, totalLen);
    free(body);

    Serial.printf("[ASR] HTTP %d\n", httpCode);
    if (httpCode != 200) {
        http.end();
        return false;
    }

    String resp = http.getString();
    http.end();

    // 解析 {"text": "...", ...}
    // ponytail: ArduinoJson 静态文档过滤未知字段
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (err) {
        Serial.printf("[ASR] JSON 解析失败: %s\n", err.c_str());
        httpCode = -3;
        return false;
    }
    outText = doc["text"] | String("");
    if (outText.length() == 0) {
        httpCode = -4;
        return false;
    }
    outText.trim();

    // 解析 token 用量
    JsonObject usage = doc["usage"];
    if (!usage.isNull()) {
        outTokens = usage["prompt_tokens"] | 0U;
    }
    Serial.printf("[ASR] 用量: %u tokens\n", (unsigned)outTokens);
    Serial.printf("[ASR] 识别结果 (%u chars): %s\n",
                  (unsigned)outText.length(), outText.substring(0, 80).c_str());
    return true;
}
