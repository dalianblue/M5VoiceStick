#include "mic_recorder.h"
#include "config.h"
#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <string.h>

MicRecorder Mic;

// WAV 头（44 字节标准 PCM）
struct __attribute__((packed)) WavHeader {
    char     riff[4]     = {'R','I','F','F'};
    uint32_t fileSize    = 0;
    char     wave[4]     = {'W','A','V','E'};
    char     fmt[4]      = {'f','m','t',' '};
    uint32_t fmtSize     = 16;
    uint16_t audioFmt    = 1;
    uint16_t channels    = MIC_CHANNELS;
    uint32_t sampleRate  = MIC_SAMPLE_RATE;
    uint32_t byteRate    = MIC_SAMPLE_RATE * MIC_CHANNELS * MIC_BITS / 8;
    uint16_t blockAlign  = MIC_CHANNELS * MIC_BITS / 8;
    uint16_t bitsPerSample = MIC_BITS;
    char     data[4]     = {'d','a','t','a'};
    uint32_t dataSize    = 0;
};

bool MicRecorder::begin() {
    _bufSamples = MIC_SAMPLE_RATE * RECORD_MAX_SECONDS;
    _buf = (int16_t*)heap_caps_malloc(_bufSamples * sizeof(int16_t),
                                       MALLOC_CAP_SPIRAM);
    if (!_buf) {
        Serial.println("[Mic] PSRAM 分配失败");
        return false;
    }
    memset(_buf, 0, _bufSamples * sizeof(int16_t));
    Serial.printf("[Mic] 缓冲 %u samples (%u bytes) in PSRAM\n",
                  (unsigned)_bufSamples, (unsigned)(_bufSamples * 2));
    return true;
}

void MicRecorder::start() {
    _samplesCaptured = 0;
    _recording = true;
    _pending = false;       // 尚未发起 record
    _lastRms = 0;
    Serial.println("[Mic] 开始录音");
}

void MicRecorder::stop() {
    if (!_recording) return;
    _recording = false;
    _pending = false;
    Serial.printf("[Mic] 停止，已录 %u samples (%ums)\n",
                  (unsigned)_samplesCaptured, (unsigned)elapsedMs());
}

// ponytail: 单缓冲 + poll isRecording()。每块 50ms。
// 升级路径：双缓冲 ping-pong（_rec_info[2]）可消除块间 5-10ms gap，
// 但 GLM-ASR 对 5% 丢包不敏感，先做简单版。
uint16_t MicRecorder::tick() {
    if (!_recording) return _lastRms;

    // 还没发起新块 → 立即发
    if (!_pending) {
        if (_samplesCaptured + RECORD_BLOCK_SAMPLES > _bufSamples) {
            stop();
            return 0;
        }
        if (M5.Mic.record(_block, RECORD_BLOCK_SAMPLES, MIC_SAMPLE_RATE)) {
            _pending = true;
        }
        return _lastRms;
    }

    // 等待当前块完成
    if (M5.Mic.isRecording() != 0) return _lastRms;

    // 块完成，写入 PSRAM
    memcpy(_buf + _samplesCaptured, _block, RECORD_BLOCK_BYTES);
    _samplesCaptured += RECORD_BLOCK_SAMPLES;
    _pending = false;

    // RMS（用绝对值平均简化，避免 sqrt）
    uint32_t sum = 0;
    for (size_t i = 0; i < RECORD_BLOCK_SAMPLES; i += 4) {
        int16_t s = _block[i];
        sum += (s < 0) ? (uint32_t)(-s) : (uint32_t)s;
    }
    _lastRms = (uint16_t)(sum / (RECORD_BLOCK_SAMPLES / 4));

    // 检查缓冲满
    if (_samplesCaptured + RECORD_BLOCK_SAMPLES > _bufSamples) {
        stop();
    }
    return _lastRms;
}

size_t MicRecorder::buildWav(uint8_t *dst) {
    WavHeader hdr;
    hdr.fileSize = 36 + _samplesCaptured * 2;
    hdr.dataSize = _samplesCaptured * 2;
    memcpy(dst, &hdr, sizeof(hdr));
    memcpy(dst + sizeof(hdr), _buf, _samplesCaptured * 2);
    return sizeof(hdr) + _samplesCaptured * 2;
}
