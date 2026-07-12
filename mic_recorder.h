#pragma once
#include <cstdint>
#include <cstddef>
#include "config.h"

// 录音模块：M5StickS3 内置 SPM1423 PDM mic via M5Unified
// 缓冲在 PSRAM，最大 30s @ 16kHz/16bit/mono

class MicRecorder {
public:
    bool begin();
    void start();
    void stop();
    bool isRecording() const { return _recording; }

    // loop 调用：内部用 M5.Mic.record() 非阻塞 + isRecording() poll
    // 返回最新 RMS（绝对值平均，0-32767 量级），供 UI 波形
    uint16_t tick();

    uint32_t elapsedMs() const { return _samplesCaptured * 1000UL / 16000UL; }
    size_t   samplesCaptured() const { return _samplesCaptured; }

    size_t buildWav(uint8_t *dst);

private:
    int16_t *_buf = nullptr;
    size_t   _bufSamples = 0;
    size_t   _samplesCaptured = 0;
    bool     _recording = false;
    bool     _pending = false;          // 当前块是否在录
    uint16_t _lastRms = 0;
    int16_t  _block[RECORD_BLOCK_SAMPLES];
};

extern MicRecorder Mic;
