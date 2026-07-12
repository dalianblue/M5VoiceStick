#pragma once
#include <WString.h>
#include <cstdint>

// WiFi 配网 + NVS 存储（ssid/pass/apiKey）
// 首次启动或长按 BtnA 触发 AP Captive Portal

bool loadConfig(String &ssid, String &pass, String &apiKey);
void saveConfig(const String &ssid, const String &pass, const String &apiKey);

// ASR 花费统计（累加 prompt_tokens 到 NVS）
uint64_t loadTotalTokens();
void     addTotalTokens(uint32_t delta);
void     clearTotalTokens();

// 启动时调用：尝试已存凭证，失败则进 AP 配网
// 成功后保持 WiFi STA 连接（不做省电关 WiFi，因为每次录音都要上传）
bool wifiBootConnect();

// 运行中重配网（长按触发）：清凭证、进 AP、保存后切 STA
bool wifiReconfigure();

// 周期性断线重连（loop 调用）
void wifiPollReconnect();

bool isWifiConnected();
String getApiKey();
