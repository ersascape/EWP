#include "hal/esp32/esp32_bluetooth.h"
#include <string.h>

#if defined(ARDUINO) && defined(CONFIG_IDF_TARGET_ESP32C3)
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "core/debug_log.h"

#define SERVICE_UUID        "0000FFE0-0000-1000-8000-00805F9B34FB"
#define CHAR_CALL_UUID      "0000FFE1-0000-1000-8000-00805F9B34FB"
#define CHAR_MEDIA_UUID     "0000FFE2-0000-1000-8000-00805F9B34FB"
#define CHAR_RECENTS_UUID   "0000FFE3-0000-1000-8000-00805F9B34FB"

namespace ersa {
namespace hal {

class BleSecCallbacks : public BLESecurityCallbacks {
public:
    uint32_t onPassKeyRequest() override { return 123456; }
    void onPassKeyNotify(uint32_t pass_key) override { (void)pass_key; }
    bool onConfirmPIN(uint32_t pass_key) override { (void)pass_key; return true; }
    bool onSecurityRequest() override { return true; }
    void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
        DebugLog::log("BLE: Auth complete success=%d fail_reason=0x%x", cmpl.success, cmpl.fail_reason);
    }
};

class Esp32Bluetooth::Impl : public BLEServerCallbacks, public BLECharacteristicCallbacks {
public:
    Esp32Bluetooth* parent_{nullptr};
    BLEServer* pServer_{nullptr};
    BLEService* pService_{nullptr};
    BLECharacteristic* pCallChar_{nullptr};
    BLECharacteristic* pMediaChar_{nullptr};
    BLECharacteristic* pRecentsChar_{nullptr};
    bool connected_{false};
    bool initialized_{false};

    void onConnect(BLEServer* pServer) override {
        (void)pServer;
        connected_ = true;
        DebugLog::log("BLE: Central connected");
        if (parent_ && parent_->connCb_) {
            parent_->connCb_(true, parent_->connUserData_);
        }
    }

    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) override {
        (void)pServer;
        connected_ = true;
        DebugLog::log("BLE: Central connected (with connection params)");
        if (param) {
            esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT);
        }
        if (parent_ && parent_->connCb_) {
            parent_->connCb_(true, parent_->connUserData_);
        }
    }

    void onDisconnect(BLEServer* pServer) override {
        (void)pServer;
        connected_ = false;
        DebugLog::log("BLE: Central disconnected, restarting advertising");
        if (parent_ && parent_->connCb_) {
            parent_->connCb_(false, parent_->connUserData_);
        }
        BLEDevice::startAdvertising();
    }

    void onWrite(BLECharacteristic* pCharacteristic) override {
        std::string rxVal = pCharacteristic->getValue();
        if (rxVal.empty()) return;

        if (pCharacteristic == pCallChar_) {
            // First byte = action: 0=Incoming, 1=Answered, 2=Rejected, 3=Ended
            uint8_t actionByte = static_cast<uint8_t>(rxVal[0]);
            BleCallAction action = BleCallAction::Incoming;
            if (actionByte == 1) action = BleCallAction::Answered;
            else if (actionByte == 2) action = BleCallAction::Rejected;
            else if (actionByte == 3) action = BleCallAction::Ended;

            char caller[32] = "";
            char number[20] = "";

            if (rxVal.length() > 1) {
                const char* payload = rxVal.c_str() + 1;
                const char* sep = strchr(payload, '\t');
                if (!sep) sep = strchr(payload, ',');

                if (sep) {
                    size_t cLen = sep - payload;
                    if (cLen >= sizeof(caller)) cLen = sizeof(caller) - 1;
                    strncpy(caller, payload, cLen);
                    caller[cLen] = '\0';
                    strncpy(number, sep + 1, sizeof(number) - 1);
                    number[sizeof(number) - 1] = '\0';
                } else {
                    strncpy(caller, payload, sizeof(caller) - 1);
                    caller[sizeof(caller) - 1] = '\0';
                }
            }

            DebugLog::log("BLE: Call event action=%d caller='%s' number='%s'",
                          int(action), caller, number);

            if (parent_ && parent_->callCb_) {
                parent_->callCb_(action, caller, number, parent_->callUserData_);
            }
        } else if (pCharacteristic == pMediaChar_) {
            // First byte: 1=Playing, 0=Paused
            bool playing = (static_cast<uint8_t>(rxVal[0]) == 1);
            char title[32] = "";
            char artist[32] = "";

            if (rxVal.length() > 1) {
                const char* payload = rxVal.c_str() + 1;
                const char* sep = strchr(payload, '\t');
                if (sep) {
                    size_t tLen = sep - payload;
                    if (tLen >= sizeof(title)) tLen = sizeof(title) - 1;
                    strncpy(title, payload, tLen);
                    title[tLen] = '\0';
                    strncpy(artist, sep + 1, sizeof(artist) - 1);
                    artist[sizeof(artist) - 1] = '\0';
                } else {
                    strncpy(title, payload, sizeof(title) - 1);
                    title[sizeof(title) - 1] = '\0';
                }
            }

            DebugLog::log("BLE: Media track: playing=%d title='%s' artist='%s'",
                          int(playing), title, artist);

            if (parent_ && parent_->mediaCb_) {
                parent_->mediaCb_(playing, title, artist, parent_->mediaUserData_);
            }
        }
    }
};

Esp32Bluetooth::Esp32Bluetooth() : pImpl_(new Impl()) {
    pImpl_->parent_ = this;
}

Esp32Bluetooth::~Esp32Bluetooth() {
    delete pImpl_;
}

Result<void> Esp32Bluetooth::init() {
    if (pImpl_->initialized_) {
        return Result<void>();
    }
    DebugLog::log("BLE: initializing 'Ersa Wearable' BLE peripheral");
    BLEDevice::init("Ersa Wearable");

    pImpl_->pServer_ = BLEDevice::createServer();
    pImpl_->pServer_->setCallbacks(pImpl_);

    // Custom Ersa Service (0xFFE0) with Call, Media & Recents characteristics
    pImpl_->pService_ = pImpl_->pServer_->createService(SERVICE_UUID);

    // Call Characteristic
    pImpl_->pCallChar_ = pImpl_->pService_->createCharacteristic(
        CHAR_CALL_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pImpl_->pCallChar_->addDescriptor(new BLE2902());
    pImpl_->pCallChar_->setCallbacks(pImpl_);

    // Media Characteristic
    pImpl_->pMediaChar_ = pImpl_->pService_->createCharacteristic(
        CHAR_MEDIA_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pImpl_->pMediaChar_->addDescriptor(new BLE2902());
    pImpl_->pMediaChar_->setCallbacks(pImpl_);

    // Recents Characteristic
    pImpl_->pRecentsChar_ = pImpl_->pService_->createCharacteristic(
        CHAR_RECENTS_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pImpl_->pRecentsChar_->addDescriptor(new BLE2902());
    pImpl_->pRecentsChar_->setCallbacks(pImpl_);

    pImpl_->pService_->start();

    // Standard Device Information Service (0x180A)
    BLEService* pDisService = pImpl_->pServer_->createService(BLEUUID((uint16_t)0x180A));
    BLECharacteristic* pMfrChar = pDisService->createCharacteristic(
        BLEUUID((uint16_t)0x2A29), BLECharacteristic::PROPERTY_READ);
    pMfrChar->setValue("Ersa");
    BLECharacteristic* pModelChar = pDisService->createCharacteristic(
        BLEUUID((uint16_t)0x2A24), BLECharacteristic::PROPERTY_READ);
    pModelChar->setValue("T1E Wearable");
    BLECharacteristic* pFwChar = pDisService->createCharacteristic(
        BLEUUID((uint16_t)0x2A26), BLECharacteristic::PROPERTY_READ);
    pFwChar->setValue("1.0.0");
    pDisService->start();

    // Standard Battery Service (0x180F)
    BLEService* pBatService = pImpl_->pServer_->createService(BLEUUID((uint16_t)0x180F));
    BLECharacteristic* pBatLevelChar = pBatService->createCharacteristic(
        BLEUUID((uint16_t)0x2A19),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    pBatLevelChar->addDescriptor(new BLE2902());
    uint8_t battPct = 100;
    pBatLevelChar->setValue(&battPct, 1);
    pBatService->start();

    pImpl_->pCallChar_->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
    pImpl_->pMediaChar_->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);

    // Configure BLE Security Bonding for native iOS Pairing & ANCS / AMS access
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
    BLEDevice::setSecurityCallbacks(new BleSecCallbacks());
    BLESecurity* pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
    pSecurity->setCapability(ESP_IO_CAP_NONE);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    pImpl_->initialized_ = true;
    return Result<void>();
}

void Esp32Bluetooth::startAdvertising() {
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();

    // Primary Advertisement Data: Flags + ANCS 128-bit Service Solicitation (21 bytes <= 31 max)
    BLEAdvertisementData advData;
    advData.setFlags(0x06); // General Discoverable + BR/EDR Not Supported

    // 128-bit ANCS Solicitation UUID: 7905f431-b5ce-4e99-a40f-4b1e122d00d0
    BLEUUID ancsUUID("7905f431-b5ce-4e99-a40f-4b1e122d00d0");
    char solData[2];
    solData[0] = 17;   // Length of AD element (1 byte type + 16 bytes UUID)
    solData[1] = 0x15; // AD Type: 128-bit Service Solicitation
    advData.addData(std::string(solData, 2) + std::string(reinterpret_cast<const char*>(ancsUUID.getNative()->uuid.uuid128), 16));
    pAdvertising->setAdvertisementData(advData);

    // Scan Response Data: Full device name ("Ersa Wearable") + 16-bit Service UUID (19 bytes <= 31 max)
    BLEAdvertisementData scanResponse;
    scanResponse.setName("Ersa Wearable");
    scanResponse.setCompleteServices(BLEUUID((uint16_t)0xFFE0));
    pAdvertising->setScanResponseData(scanResponse);

    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    DebugLog::log("BLE: advertising started with ANCS solicitation (name='Ersa Wearable', addr=%s)", getDeviceAddress());
}

void Esp32Bluetooth::stopAdvertising() {
    BLEDevice::stopAdvertising();
}

const char* Esp32Bluetooth::getDeviceName() const {
    return "Ersa Wearable";
}

const char* Esp32Bluetooth::getDeviceAddress() const {
    static char s_addrBuf[24] = "00:00:00:00:00:00";
    std::string s = BLEDevice::getAddress().toString();
    if (!s.empty()) {
        strncpy(s_addrBuf, s.c_str(), sizeof(s_addrBuf) - 1);
        s_addrBuf[sizeof(s_addrBuf) - 1] = '\0';
    }
    return s_addrBuf;
}

bool Esp32Bluetooth::isConnected() const {
    return pImpl_ && pImpl_->connected_;
}

void Esp32Bluetooth::setCallCallback(BleCallCallback cb, void* userData) {
    callCb_ = cb;
    callUserData_ = userData;
}

void Esp32Bluetooth::setMediaCallback(BleMediaCallback cb, void* userData) {
    mediaCb_ = cb;
    mediaUserData_ = userData;
}

void Esp32Bluetooth::setConnectionCallback(BleConnectionCallback cb, void* userData) {
    connCb_ = cb;
    connUserData_ = userData;
}

void Esp32Bluetooth::acceptCall() {
    DebugLog::log("BLE: Command -> ACCEPT CALL");
    if (pImpl_ && pImpl_->pCallChar_ && pImpl_->connected_) {
        uint8_t val = 0x01; // Accept
        pImpl_->pCallChar_->setValue(&val, 1);
        pImpl_->pCallChar_->notify();
    }
}

void Esp32Bluetooth::rejectCall() {
    DebugLog::log("BLE: Command -> REJECT CALL");
    if (pImpl_ && pImpl_->pCallChar_ && pImpl_->connected_) {
        uint8_t val = 0x02; // Reject
        pImpl_->pCallChar_->setValue(&val, 1);
        pImpl_->pCallChar_->notify();
    }
}

void Esp32Bluetooth::hangupCall() {
    DebugLog::log("BLE: Command -> HANG UP CALL");
    if (pImpl_ && pImpl_->pCallChar_ && pImpl_->connected_) {
        uint8_t val = 0x02; // Hangup
        pImpl_->pCallChar_->setValue(&val, 1);
        pImpl_->pCallChar_->notify();
    }
}

void Esp32Bluetooth::dial(const char* number) {
    DebugLog::log("BLE: Command -> DIAL '%s'", number ? number : "");
    if (pImpl_ && pImpl_->pCallChar_ && pImpl_->connected_) {
        char buf[32];
        buf[0] = 0x03; // Dial command
        if (number) {
            strncpy(buf + 1, number, sizeof(buf) - 2);
            buf[sizeof(buf) - 1] = '\0';
        } else {
            buf[1] = '\0';
        }
        pImpl_->pCallChar_->setValue(reinterpret_cast<uint8_t*>(buf), strlen(buf + 1) + 1);
        pImpl_->pCallChar_->notify();
    }
}

void Esp32Bluetooth::mediaCommand(BleMediaAction action) {
    DebugLog::log("BLE: Command -> MEDIA ACTION %d", int(action));
    if (pImpl_ && pImpl_->pMediaChar_ && pImpl_->connected_) {
        uint8_t val = static_cast<uint8_t>(action);
        pImpl_->pMediaChar_->setValue(&val, 1);
        pImpl_->pMediaChar_->notify();
    }
}

} // namespace hal
} // namespace ersa

#else

// Host stub implementation for tests/desktop builds
namespace ersa {
namespace hal {

Esp32Bluetooth::Esp32Bluetooth() = default;
Esp32Bluetooth::~Esp32Bluetooth() = default;

Result<void> Esp32Bluetooth::init() { return Result<void>(); }
void Esp32Bluetooth::startAdvertising() {}
void Esp32Bluetooth::stopAdvertising() {}
bool Esp32Bluetooth::isConnected() const { return false; }
const char* Esp32Bluetooth::getDeviceName() const { return "Ersa Wearable"; }
const char* Esp32Bluetooth::getDeviceAddress() const { return "24:DC:C3:01:23:45"; }

void Esp32Bluetooth::setCallCallback(BleCallCallback cb, void* userData) {
    callCb_ = cb;
    callUserData_ = userData;
}

void Esp32Bluetooth::setMediaCallback(BleMediaCallback cb, void* userData) {
    mediaCb_ = cb;
    mediaUserData_ = userData;
}

void Esp32Bluetooth::setConnectionCallback(BleConnectionCallback cb, void* userData) {
    connCb_ = cb;
    connUserData_ = userData;
}

void Esp32Bluetooth::acceptCall() {}
void Esp32Bluetooth::rejectCall() {}
void Esp32Bluetooth::hangupCall() {}
void Esp32Bluetooth::dial(const char*) {}
void Esp32Bluetooth::mediaCommand(BleMediaAction) {}

} // namespace hal
} // namespace ersa

#endif
