#pragma once
#include <WString.h>
#include <cstdint>

// 上传 WAV 字节到智谱 BigModel GLM-ASR-2512，返回识别文本
// 成功 true，文本写入 outText，token 用量写入 outTokens；失败 false
bool asrTranscribe(const uint8_t *wavBytes, size_t wavLen,
                   const String &apiKey, String &outText, int &httpCode,
                   uint32_t &outTokens);
