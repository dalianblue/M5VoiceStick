// M5StickS3 语音输入终端
// 按住 BtnA 录音 → 上传 GLM-ASR → BLE GATT 推送文字 → 伴侣脚本粘贴

#include <M5Unified.h>
#include "config.h"
#include "wifi_portal.h"
#include "mic_recorder.h"
#include "asr_client.h"
#include "ble_text_server.h"
#include "ui_render.h"

enum class State : uint8_t {
    Boot, Idle, Recording, Uploading, Result, Failed, Total
};

static State    s_state = State::Boot;
static uint32_t s_stateEnter = 0;
static uint32_t s_lastActivity = 0;

// 录音数据
static uint8_t *s_wavBuf = nullptr;     // PSRAM，存放完整 WAV
static size_t   s_wavLen = 0;
static String   s_asrText;
static int      s_lastHttpCode = 0;
static uint32_t s_lastTokens = 0;       // 本次 ASR 用量
static uint64_t s_totalTokens = 0;      // 累计 ASR 用量（开机加载）

// 上传任务状态
static volatile bool    s_uploadDone = false;
static volatile bool    s_uploadOk   = false;

// 重配网长按计时
static uint32_t s_btnBPressStart = 0;
static bool     s_btnBFired = false;

// ponytail: 上传跑在独立 task，UI 主循环继续刷新动画
static void uploadTask(void *) {
    String apiKey = getApiKey();
    if (apiKey.length() == 0) {
        s_lastHttpCode = -5;
        s_uploadOk = false;
    } else {
        s_uploadOk = asrTranscribe(s_wavBuf, s_wavLen, apiKey,
                                    s_asrText, s_lastHttpCode, s_lastTokens);
    }
    s_uploadDone = true;
    vTaskDelete(nullptr);
}

static void startUploadTask() {
    s_uploadDone = false;
    s_uploadOk = false;
    s_asrText = "";
    s_lastTokens = 0;

    // 把已录 PCM 封装成 WAV，存 PSRAM
    size_t wavBytes = Mic.samplesCaptured() * 2 + 44;
    if (s_wavBuf) { free(s_wavBuf); s_wavBuf = nullptr; }
    s_wavBuf = (uint8_t*)heap_caps_malloc(wavBytes, MALLOC_CAP_SPIRAM);
    if (!s_wavBuf) {
        // 内存不够：直接失败
        s_uploadDone = true;
        s_uploadOk = false;
        s_lastHttpCode = -6;
        return;
    }
    s_wavLen = Mic.buildWav(s_wavBuf);

    xTaskCreatePinnedToCore(uploadTask, "upload", 8192, nullptr,
                            1, nullptr, 0);
}

static void enterState(State s) {
    s_state = s;
    s_stateEnter = millis();
    Serial.printf("[State] -> %d\n", (int)s);

    switch (s) {
    case State::Idle:
        uiDrawIdle(isWifiConnected(), bleIsConnected());
        break;
    case State::Recording:
        uiRecordingStart();
        // 先响提示音，等播完再开 mic（否则 I2S 抢占让 tone 静音）
        M5.Speaker.tone(1500, 100);
        delay(120);
        Mic.start();
        break;
    case State::Uploading:
        uiDrawUploading(0);
        startUploadTask();
        break;
    case State::Result:
        // 由 SENDING 转入；这里仅显示
        break;
    case State::Total:
        uiDrawTotal(s_totalTokens);
        break;
    case State::Failed:
        // 由调用方设好错误信息后再 uiDrawFailed
        break;
    default: break;
    }
}

static void showFailedThenIdle(const char *msg) {
    uiDrawFailed(msg);
    s_state = State::Failed;
    s_stateEnter = millis();
}

// ============================================================
// 按键
// ============================================================
static void handleButtons() {
    uint32_t now = millis();
    State cur = s_state;

    // 任何按键唤醒屏幕
    if ((M5.BtnA.isPressed() || M5.BtnB.isPressed()) && uiIsOff()) {
        uiWake();
        s_lastActivity = now;
        return;  // 唤醒帧不再处理动作
    }
    if (M5.BtnA.isPressed() || M5.BtnB.wasReleased() || M5.BtnA.wasReleased()) {
        s_lastActivity = now;
    }

    // BtnA：IDLE → 开始录音；RECORDING → 松开后停止
    if (cur == State::Idle && M5.BtnA.isPressed() && !M5.BtnA.wasReleased()) {
        if (isWifiConnected()) {
            enterState(State::Recording);
        } else {
            // WiFi 没连，提示
            showFailedThenIdle("WiFi 未连");
        }
        return;
    }
    if (cur == State::Recording && M5.BtnA.wasReleased()) {
        Mic.stop();
        if (Mic.elapsedMs() < RECORD_MIN_MS) {
            // 误触，回到 Idle
            enterState(State::Idle);
        } else {
            enterState(State::Uploading);
        }
        return;
    }

    // BtnB：短按切换 Idle↔Total；长按 3s 上下文相关（Idle=重配网，Total=清零）
    if (M5.BtnB.isPressed()) {
        if (s_btnBPressStart == 0) s_btnBPressStart = now;
        if (!s_btnBFired && (now - s_btnBPressStart > 3000)) {
            s_btnBFired = true;
            if (cur == State::Total) {
                clearTotalTokens();
                s_totalTokens = 0;
                uiDrawTotal(0);
                M5.Speaker.tone(600, 80);
            } else {
                // Idle / 其他：重配网
                wifiReconfigure();
                enterState(State::Idle);
            }
        }
    } else {
        // 松开：若没触发长按，算短按
        if (M5.BtnB.wasReleased() && !s_btnBFired) {
            if (cur == State::Idle) {
                enterState(State::Total);
            } else if (cur == State::Total) {
                enterState(State::Idle);
            }
        }
        s_btnBPressStart = 0;
        s_btnBFired = false;
    }
}

// ============================================================
// 主循环状态处理
// ============================================================
static void tickRecording() {
    uint16_t rms = Mic.tick();
    bool autoStop = (Mic.elapsedMs() >= RECORD_MAX_SECONDS * 1000UL);
    uiRecordingTick(rms, Mic.elapsedMs(), autoStop);
    if (autoStop || !Mic.isRecording()) {
        // 录音已自动截断
        if (s_state == State::Recording && !Mic.isRecording()) {
            Mic.stop();
            enterState(State::Uploading);
        }
    }
}

static void tickUploading() {
    uint32_t elapsed = millis() - s_stateEnter;
    uiDrawUploading(elapsed);

    if (s_uploadDone) {
        if (s_uploadOk) {
            // 累计花费
            if (s_lastTokens > 0) {
                addTotalTokens(s_lastTokens);
                s_totalTokens += s_lastTokens;
            }
            // 发送到伴侣脚本
            bool pasted = false;
            if (bleIsConnected()) {
                pasted = bleSendText(s_asrText);
            }
            uiDrawResult(s_lastTokens, pasted);
            s_state = State::Result;
            s_stateEnter = millis();
        } else {
            char msg[40];
            snprintf(msg, sizeof(msg), "HTTP %d", s_lastHttpCode);
            showFailedThenIdle(msg);
        }
    }
}

// ============================================================
// Arduino 入口
// ============================================================
void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);
    // ponytail: 沿用 LifeLine_M5 的方式——M5.begin 内部已初始化 Speaker，
    // 关键是 setVolume()，默认 0 听不见
    M5.Speaker.setVolume(128);
    Serial.println("[Speaker] volume=128");

    Serial.println("\n=== M5VoiceStick boot ===");

    uiInit();
    uiDrawBoot("启动中...");

    if (!Mic.begin()) {
        uiDrawFailed("Mic PSRAM FAIL");
        while (1) delay(1000);
    }

    bleTextInit();   // BLE 早启动，可与 WiFi 并行

    if (!wifiBootConnect()) {
        // 配网失败也允许进 Idle（用户可长按 BtnB 重配）
        Serial.println("[Boot] WiFi 初始未就绪");
    }

    setCpuFrequencyMhz(POWER_CPU_FREQ_MHZ);
    s_totalTokens = loadTotalTokens();
    s_lastActivity = millis();
    enterState(State::Idle);
}

void loop() {
    M5.update();
    uint32_t now = millis();

    handleButtons();

    switch (s_state) {
    case State::Recording: tickRecording(); break;
    case State::Uploading: tickUploading(); break;
    case State::Result:
        // 显示 2 秒后回 Idle
        if (now - s_stateEnter > 2000) {
            enterState(State::Idle);
        }
        break;
    case State::Failed:
        if (now - s_stateEnter > 3000) {
            enterState(State::Idle);
        }
        break;
    case State::Idle: {
        uint32_t idle = now - s_lastActivity;
        if (idle > POWER_SCREEN_OFF_MS && !uiIsOff()) uiOff();
        else if (idle > POWER_SCREEN_DIM_MS && !uiIsOff()) uiDim();
        // 周期性刷新 Idle 状态图标
        static uint32_t lastIdleRefresh = 0;
        if (now - lastIdleRefresh > 2000) {
            lastIdleRefresh = now;
            uiDrawIdle(isWifiConnected(), bleIsConnected());
        }
        break;
    }
    case State::Total:
        // 静态显示，超时回到 Idle
        if (now - s_lastActivity > POWER_SCREEN_DIM_MS) {
            enterState(State::Idle);
        }
        break;
    default: break;
    }

    wifiPollReconnect();
    delay(POWER_LOOP_DELAY_MS);
}
