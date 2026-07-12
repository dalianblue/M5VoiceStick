#pragma once

// ============================================================
// 全局配置（改参数只动这一个文件）
// ============================================================

// ---------- 录音 ----------
#define MIC_SAMPLE_RATE       16000
#define MIC_BITS              16
#define MIC_CHANNELS          1
#define RECORD_MAX_SECONDS    30
#define RECORD_BLOCK_SAMPLES  800       // 每 50ms 一块（16k 单声道）
#define RECORD_BLOCK_BYTES    (RECORD_BLOCK_SAMPLES * 2)
// 缓冲：16000 * 2 * 30 = 960KB，放 PSRAM
#define RECORD_BUFFER_BYTES   (MIC_SAMPLE_RATE * (MIC_BITS / 8) * MIC_CHANNELS * RECORD_MAX_SECONDS)
#define RECORD_MIN_MS         500       // <500ms 视为误触，不上传

// ---------- WiFi（AP 模式 Captive Portal） ----------
#define WIFI_CONNECT_TIMEOUT_MS     15000
#define CONFIG_PORTAL_TIMEOUT_MS    300000
#define WIFI_RECONNECT_INTERVAL_MS  30000

#define AP_SSID         "M5VoiceStick-Cfg"
#define AP_PASSWORD     ""

// ---------- ASR（智谱 BigModel） ----------
#define ASR_ENDPOINT    "https://open.bigmodel.cn/api/paas/v4/audio/transcriptions"
#define ASR_MODEL       "glm-asr-2512"
#define ASR_TIMEOUT_MS  30000

// ---------- BLE 文本通道 ----------
#define BLE_DEVICE_NAME         "M5VoiceStick"
#define BLE_SERVICE_UUID        "0000ff01-0000-1000-8000-00805f9b34fb"
#define BLE_RX_CHAR_UUID        "0000ff02-0000-1000-8000-00805f9b34fb"  // 伴侣→ESP32 (write)
#define BLE_TX_CHAR_UUID        "0000ff03-0000-1000-8000-00805f9b34fb"  // ESP32→伴侣 (notify)
#define BLE_MSG_TERMINATOR      "\n\n"
#define BLE_ACK_TIMEOUT_MS      3000
#define BLE_MTU                 247
#define BLE_CHUNK_PAYLOAD       244

// ---------- NVS ----------
#define NVS_NAMESPACE "voice_stick"

// ---------- 屏幕 ----------
#define SCREEN_ROTATION   1     // 1=横屏 240x135，A 键在右侧
#define BRIGHTNESS_BOOT   128

// ---------- 省电 ----------
#define POWER_CPU_FREQ_MHZ     80
#define POWER_SCREEN_DIM_MS    30000
#define POWER_SCREEN_OFF_MS    60000
#define POWER_SCREEN_DIM_LEVEL 20
#define POWER_LOOP_DELAY_MS    20

// ---------- 中文字体 ----------
#define USE_CHINESE_FONT
