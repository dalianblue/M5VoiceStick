#pragma once
#include <WString.h>

// 自定义 GATT 文本通道（Nordic UART-like）
// RX (write):  伴侣→ESP32，发 "OK\n" 确认收到
// TX (notify): ESP32→伴侣，发送识别文本（多包，以 \n\n 结束）

void bleTextInit();
bool bleIsConnected();

// 发送文本，等待伴侣 ACK
// 成功返回 true（已粘贴到剪贴板）；超时/未连接返回 false
bool bleSendText(const String &text);
