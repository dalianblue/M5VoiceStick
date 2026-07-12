#include "ui_render.h"
#include "config.h"
#include <M5Unified.h>

#ifdef USE_CHINESE_FONT
#include "cnfont.h"
extern const GFXfont cnfont_subset16pt8b;
#endif

// 录音波形历史：120 个 RMS 采样 × 2px = 横铺 240px
#define WAVE_HISTORY 120
static uint16_t s_wave[WAVE_HISTORY];
static size_t   s_waveIdx = 0;
static bool     s_waveFull = false;

static UIPage   s_lastPage = (UIPage)-1;
static bool     s_screenOff = false;
static uint8_t  s_brightness = BRIGHTNESS_BOOT;

static void loadCNFont() {
#ifdef USE_CHINESE_FONT
    LCD.setFont(&cnfont_subset16pt8b);
#endif
}
static void resetFont() { LCD.setTextFont(0); }

void uiInit() {
    LCD.setRotation(SCREEN_ROTATION);
    LCD.setBrightness(BRIGHTNESS_BOOT);
    LCD.fillScreen(TFT_BLACK);
}

// ============================================================
// IDLE 页
// ============================================================
static void drawStatusIcons(bool wifiOK, bool bleConn) {
    uint16_t wColor = wifiOK  ? TFT_GREEN : TFT_RED;
    uint16_t bColor = bleConn ? TFT_GREEN : TFT_DARKGREY;
    LCD.fillRect(4,  8, 12, 12, wColor);   // WiFi
    LCD.fillRect(22, 8, 12, 12, bColor);   // BLE
    resetFont();
    LCD.setTextSize(1);
    LCD.setTextColor(TFT_DARKGREY, TFT_BLACK);
    LCD.setCursor(4, 24);
    LCD.print("W");
    LCD.setCursor(22, 24);
    LCD.print("B");
}

void uiDrawIdle(bool wifiOK, bool bleConn) {
    if (s_screenOff) return;
    if (s_lastPage == UIPage::Idle) {
        // 仅刷新状态图标区，避免整屏闪烁
        LCD.fillRect(0, 0, 40, 40, TFT_BLACK);
        drawStatusIcons(wifiOK, bleConn);
        return;
    }
    LCD.fillScreen(TFT_BLACK);
    s_lastPage = UIPage::Idle;

    // 右箭头先画，固定贴右边（指向 BtnA）
    const int tipX = 232, baseX = 218, shaftX = 202, ay = 67;
    LCD.fillTriangle(tipX, ay, baseX, ay - 8, baseX, ay + 8, TFT_YELLOW);
    LCD.fillRect(shaftX, ay - 3, baseX - shaftX, 6, TFT_YELLOW);

    // 文本：在箭头左侧的可用区域（x=4..198）居中
    loadCNFont();
    LCD.setTextColor(TFT_WHITE, TFT_BLACK);
    LCD.setTextSize(1);
    LCD.setTextWrap(false);   // 防止 cnfont 宽字符意外换行
    const char *label = "按下语音识别";
    int labelW = LCD.textWidth(label);
    int availW = shaftX - 4 - 4;   // 左右各留 4px 边距
    int textX = 4 + (availW - labelW) / 2;
    if (textX < 4) textX = 4;
    LCD.setCursor(textX, 58);
    LCD.print(label);
    LCD.setTextWrap(true);

    drawStatusIcons(wifiOK, bleConn);
}

// ============================================================
// RECORDING 页
// ============================================================
void uiRecordingStart() {
    if (s_screenOff) return;
    s_waveIdx = 0;
    s_waveFull = false;
    LCD.fillScreen(TFT_BLACK);
    s_lastPage = UIPage::Recording;
}

static void drawWaveform(uint16_t rms) {
    // 推入新值
    s_wave[s_waveIdx] = rms;
    s_waveIdx = (s_waveIdx + 1) % WAVE_HISTORY;
    if (s_waveIdx == 0) s_waveFull = true;

    // 波形区：x 0..240，y 25..95，中线 y=60，最大半高 32
    LCD.fillRect(0, 25, 240, 70, TFT_BLACK);
    size_t count = s_waveFull ? WAVE_HISTORY : s_waveIdx;
    if (count == 0) return;

    // 归一化基线（取窗口内最大值）
    uint32_t maxRms = 1;
    for (size_t i = 0; i < count; i++) {
        if (s_wave[i] > maxRms) maxRms = s_wave[i];
    }
    const int midY   = 60;
    const int maxHalf = 32;
    const int barW   = 2;
    for (size_t i = 0; i < count; i++) {
        size_t   idx = s_waveFull ? (s_waveIdx + i) % WAVE_HISTORY : i;
        uint16_t v   = s_wave[idx];
        int      h   = (int)((uint32_t)v * maxHalf / (maxRms + 1));
        int      x   = (int)i * barW;
        uint16_t color = (v > maxRms / 2) ? TFT_RED :
                         (v > maxRms / 4) ? TFT_YELLOW : TFT_GREEN;
        LCD.drawLine(x, midY - h, x, midY + h, color);
        LCD.drawLine(x + 1, midY - h, x + 1, midY + h, color);
    }
    // 中线参考（弱）
    LCD.drawFastHLine(0, midY, 240, TFT_DARKGREY);
}

void uiRecordingTick(uint16_t rms, uint32_t elapsedMs, bool autoStop) {
    if (s_screenOff) return;

    // 顶部状态条 y 0..18
    LCD.fillRect(0, 0, 240, 20, TFT_BLACK);
    LCD.fillCircle(10, 10, 5, (elapsedMs / 500) % 2 ? TFT_RED : TFT_DARKGREY);
    resetFont();   // 用默认小字体，跟圆点同高
    LCD.setTextColor(TFT_RED, TFT_BLACK);
    LCD.setTextSize(1);
    LCD.setCursor(22, 7);   // 圆点中心 y=10，5x8 字体居中 → y≈6
    LCD.print(autoStop ? "AUTO STOP" : "REC");

    // 波形（中部 y 25..95）
    drawWaveform(rms);

    // 时间 + 上限同行同字号 y 100..120
    LCD.fillRect(0, 100, 240, 30, TFT_BLACK);
    uint32_t sec = elapsedMs / 1000;
    char buf[20];
    snprintf(buf, sizeof(buf), "%u:%02u / %us",
             (unsigned)(sec / 60), (unsigned)(sec % 60),
             (unsigned)RECORD_MAX_SECONDS);
    resetFont();
    LCD.setTextColor(TFT_WHITE, TFT_BLACK);
    LCD.setTextSize(2);
    int timeW = LCD.textWidth(buf);
    LCD.setCursor((240 - timeW) / 2, 105);
    LCD.print(buf);

    s_lastPage = UIPage::Recording;
}

// ============================================================
// UPLOADING 页
// ============================================================
void uiDrawUploading(uint32_t elapsedMs) {
    if (s_screenOff) return;
    if (s_lastPage != UIPage::Uploading) {
        LCD.fillScreen(TFT_BLACK);
        s_lastPage = UIPage::Uploading;
    }

    // 顶部点动画（居中）
    LCD.fillRect(80, 35, 80, 20, TFT_BLACK);
    LCD.setTextColor(TFT_CYAN, TFT_BLACK);
    resetFont();
    LCD.setTextSize(2);
    int phase = (elapsedMs / 200) % 4;
    String dots = ".";
    for (int i = 0; i < phase; i++) dots += ".";
    int dotsW = LCD.textWidth(dots);
    LCD.setCursor((240 - dotsW) / 2, 35);
    LCD.print(dots);

    // 中央 "上传中..." 居中
    loadCNFont();
    LCD.setTextColor(TFT_WHITE, TFT_BLACK);
    LCD.setTextSize(1);
    const char *label = "上传中...";
    int lblW = LCD.textWidth(label);
    LCD.setCursor((240 - lblW) / 2, 70);
    LCD.print(label);

    // 底部秒数 居中
    char buf[12];
    snprintf(buf, sizeof(buf), "%us", (unsigned)(elapsedMs / 1000));
    resetFont();
    LCD.setTextColor(TFT_DARKGREY, TFT_BLACK);
    LCD.setTextSize(1);
    int secW = LCD.textWidth(buf);
    LCD.setCursor((240 - secW) / 2, 110);
    LCD.print(buf);
}

// ============================================================
// RESULT 页：显示本次花费
// ============================================================
// 费率：16 元 / 百万 tokens → cost = tokens * 16 / 1,000,000 元
// 输出格式：整数部分.4 位小数 元
static void formatCost(uint64_t tokens, char *buf, size_t n) {
    uint64_t microYuan = tokens * 16ULL;
    uint64_t intPart   = microYuan / 1000000ULL;
    uint64_t fracPart  = (microYuan % 1000000ULL + 50ULL) / 100ULL;  // 4 位 + 四舍五入
    if (fracPart >= 10000ULL) { intPart++; fracPart -= 10000ULL; }
    snprintf(buf, n, "%llu.%04llu",
             (unsigned long long)intPart, (unsigned long long)fracPart);
}

// 居中绘制大号数字（font 0 textSize 3）
static void drawCostCentered(const char *costBuf, int y, uint16_t numColor) {
    resetFont();
    LCD.setTextColor(numColor, TFT_BLACK);
    LCD.setTextSize(3);
    int w = LCD.textWidth(costBuf);
    LCD.setCursor((240 - w) / 2, y);
    LCD.print(costBuf);
    resetFont();      // 复位，避免污染后续渲染
    LCD.setTextSize(1);
}

void uiDrawResult(uint32_t tokens, bool pasted) {
    if (s_screenOff) return;
    LCD.fillScreen(TFT_BLACK);
    s_lastPage = UIPage::Result;

    loadCNFont();
    LCD.setTextColor(TFT_DARKGREY, TFT_BLACK);
    LCD.setTextSize(1);
    LCD.setCursor(4, 4);
    LCD.print("本次花费 (元)");

    char costBuf[24];
    formatCost(tokens, costBuf, sizeof(costBuf));
    drawCostCentered(costBuf, 55, TFT_GREEN);

    // 仅在 BLE 没连时提示
    if (!pasted) {
        loadCNFont();
        LCD.setTextSize(1);   // 必须显式设
        LCD.setTextColor(TFT_ORANGE, TFT_BLACK);
        LCD.setCursor((240 - LCD.textWidth("BLE 未连")) / 2, 110);
        LCD.print("BLE 未连");
    }
}

// ============================================================
// TOTAL 页：累计花费
// ============================================================
void uiDrawTotal(uint64_t totalTokens) {
    if (s_screenOff) return;
    LCD.fillScreen(TFT_BLACK);
    s_lastPage = UIPage::Total;

    loadCNFont();
    LCD.setTextColor(TFT_DARKGREY, TFT_BLACK);
    LCD.setTextSize(1);
    LCD.setCursor(4, 4);
    LCD.print("总计花费 (元)");

    char costBuf[32];
    formatCost(totalTokens, costBuf, sizeof(costBuf));
    drawCostCentered(costBuf, 55, TFT_CYAN);

    // 底部操作提示
    loadCNFont();
    LCD.setTextSize(1);   // 必须显式设，因为 drawCostCentered 设了 3
    LCD.setTextColor(TFT_DARKGREY, TFT_BLACK);
    LCD.setTextWrap(false);
    const char *hint = "B:返回 长按清零";
    int hintW = LCD.textWidth(hint);
    LCD.setCursor((240 - hintW) / 2, 105);
    LCD.print(hint);
    LCD.setTextWrap(true);
}

// ============================================================
// BOOT 页（启动画面，不带「失败」前缀）
// ============================================================
void uiDrawBoot(const char *msg) {
    if (s_screenOff) return;
    LCD.fillScreen(TFT_BLACK);
    s_lastPage = (UIPage)-1;   // 不属于任何状态页，强制下帧重绘
    loadCNFont();
    LCD.setTextColor(TFT_CYAN, TFT_BLACK);
    LCD.setTextSize(1);
    int w = LCD.textWidth(msg);
    LCD.setCursor((240 - w) / 2, 60);
    LCD.print(msg);
    resetFont();
}

// ============================================================
// FAILED 页
// ============================================================
void uiDrawFailed(const char *msg) {
    if (s_screenOff) return;
    LCD.fillScreen(TFT_BLACK);
    s_lastPage = UIPage::Failed;
    loadCNFont();
    LCD.setTextColor(TFT_RED, TFT_BLACK);
    LCD.setTextSize(2);
    LCD.setCursor(30, 30);
    LCD.print("失败");
    LCD.setTextColor(TFT_WHITE, TFT_BLACK);
    LCD.setTextSize(1);
    LCD.setCursor(20, 75);
    LCD.print(msg);
    resetFont();
}

// ============================================================
// 省电
// ============================================================
void uiDim()  { if (!s_screenOff) { LCD.setBrightness(POWER_SCREEN_DIM_LEVEL); } }
void uiOff()  { s_screenOff = true; LCD.setBrightness(0); }
void uiWake() {
    s_screenOff = false;
    LCD.setBrightness(s_brightness);
    s_lastPage = (UIPage)-1;
}
bool uiIsOff() { return s_screenOff; }
