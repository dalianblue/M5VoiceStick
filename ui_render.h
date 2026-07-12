#pragma once
#include <WString.h>

#define LCD M5.Display

enum class UIPage { Idle, Recording, Uploading, Result, Failed, Total };

void uiInit();

void uiDrawIdle(bool wifiOK, bool bleConn);
void uiDrawBoot(const char *msg);   // 启动画面（不带「失败」前缀）
void uiRecordingStart();
void uiRecordingTick(uint16_t rms, uint32_t elapsedMs, bool autoStop);
void uiDrawUploading(uint32_t elapsedMs);
void uiDrawResult(uint32_t tokens, bool pasted);
void uiDrawTotal(uint64_t totalTokens);
void uiDrawFailed(const char *msg);

// 屏幕省电
void uiDim();
void uiOff();
void uiWake();
bool uiIsOff();
