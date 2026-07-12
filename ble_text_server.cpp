#include "ble_text_server.h"
#include "config.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

static BLEServer         *s_server = nullptr;
static BLECharacteristic *s_txChar = nullptr;
static BLECharacteristic *s_rxChar = nullptr;

static volatile bool   s_connected = false;
static volatile bool   s_ackReceived = false;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer*) override {
        s_connected = true;
        Serial.println("[BLE] 已连接");
    }
    void onDisconnect(BLEServer*) override {
        s_connected = false;
        Serial.println("[BLE] 已断开，重启广播");
        BLEDevice::startAdvertising();
    }
};

class RxCbs : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *c) override {
        String v = c->getValue();
        // 任意写入都视为 ACK
        s_ackReceived = true;
        Serial.printf("[BLE] RX ACK (%u bytes)\n", (unsigned)v.length());
    }
};

void bleTextInit() {
    BLEDevice::init(BLE_DEVICE_NAME);
    // 协商更大 MTU（默认 23，notify payload 仅 20 字节）
    BLEDevice::setMTU(BLE_MTU);

    s_server = BLEDevice::createServer();
    s_server->setCallbacks(new ServerCallbacks());

    BLEService *svc = s_server->createService(BLE_SERVICE_UUID);

    s_rxChar = svc->createCharacteristic(
        BLE_RX_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    s_rxChar->setCallbacks(new RxCbs());
    s_rxChar->addDescriptor(new BLE2902());

    s_txChar = svc->createCharacteristic(
        BLE_TX_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    s_txChar->addDescriptor(new BLE2902());

    svc->start();

    BLEAdvertising *adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SERVICE_UUID);
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);  // iPhone 连接稳定性
    adv->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("[BLE] 文本服务已启动，广播中");
}

bool bleIsConnected() { return s_connected; }

bool bleSendText(const String &text) {
    if (!s_connected || !s_txChar) return false;

    // 终结符 \n\n 标识一帧结束
    String msg = text + BLE_MSG_TERMINATOR;
    size_t total = msg.length();
    size_t off = 0;

    s_ackReceived = false;
    uint32_t start = millis();

    while (off < total) {
        size_t chunk = min((size_t)BLE_CHUNK_PAYLOAD, total - off);
        s_txChar->setValue((uint8_t*)(msg.c_str() + off), chunk);
        s_txChar->notify();
        off += chunk;
        // ponytail: 20ms 间隔避免 GATT 拥塞，实测 BLE 5.0 稳定
        delay(20);
    }

    // 等待伴侣回 ACK
    while (!s_ackReceived && (millis() - start < BLE_ACK_TIMEOUT_MS)) {
        delay(10);
    }
    bool ok = s_ackReceived;
    Serial.printf("[BLE] 发送 %u bytes, ACK %s\n",
                  (unsigned)total, ok ? "OK" : "TIMEOUT");
    return ok;
}
