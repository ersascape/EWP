#if defined(ARDUINO)

#include <Arduino.h>
#include "hal/esp32/esp32_apple_ble.h"
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLERemoteService.h>
#include <BLERemoteCharacteristic.h>
#include <BLE2902.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "core/debug_log.h"

#define ANCS_SERVICE_UUID           "7905f431-b5ce-4e99-a40f-4b1e122d00d0"
#define ANCS_CHAR_NOTIF_SOURCE      "9fbf120d-6301-42d9-8c58-25e699a21dbd"
#define ANCS_CHAR_CONTROL_POINT     "69d1d8f3-45e1-49a8-9821-9bbdfdaad9d9"
#define ANCS_CHAR_DATA_SOURCE       "22eac6e9-24d6-4bb5-be44-b36ace7c7bfb"

#define AMS_SERVICE_UUID            "89d3502b-0f36-433a-8ef4-c502ad55f8dc"
#define AMS_CHAR_REMOTE_CMD         "9b3c81d8-57b1-4a8a-b8df-0e56f7ca51c2"
#define AMS_CHAR_ENTITY_UPDATE      "2f7cabce-808d-411f-9a0c-bb92ba96c102"
#define AMS_CHAR_ENTITY_ATTR        "c6b2f38c-23ab-46d8-a6ab-a3a870bbd5d7"

namespace ersa {
namespace hal {

class Esp32AppleClient::Impl {
public:
    Esp32AppleClient* parent_{nullptr};
    BleCallCallback callCb_{nullptr};
    void* callUserData_{nullptr};
    BleMediaCallback mediaCb_{nullptr};
    void* mediaUserData_{nullptr};

    esp_bd_addr_t peerBda_{0};
    esp_ble_addr_type_t addrType_{BLE_ADDR_TYPE_RANDOM};

    TaskHandle_t taskHandle_{nullptr};
    volatile bool running_{false};

    BLEClient* pClient_{nullptr};
    BLERemoteCharacteristic* pAncsNotifSource_{nullptr};
    BLERemoteCharacteristic* pAncsControlPoint_{nullptr};
    BLERemoteCharacteristic* pAncsDataSource_{nullptr};

    BLERemoteCharacteristic* pAmsRemoteCmd_{nullptr};
    BLERemoteCharacteristic* pAmsEntityUpdate_{nullptr};

    uint32_t currentCallUid_{0};
    bool callActive_{false};

    char trackTitle_[32] = "";
    char trackArtist_[32] = "";
    bool isPlaying_{false};

    void handleAncsNotification(uint8_t* pData, size_t length) {
        if (length < 8) return;
        uint8_t eventId = pData[0];     // 0=Added, 1=Modified, 2=Removed
        uint8_t categoryId = pData[2];  // 1=IncomingCall, 2=MissedCall
        uint32_t uid = (uint32_t)pData[4] | ((uint32_t)pData[5] << 8) |
                       ((uint32_t)pData[6] << 16) | ((uint32_t)pData[7] << 24);

        DebugLog::log("ANCS: Notif event=%u cat=%u uid=%lu", eventId, categoryId, (unsigned long)uid);

        if (categoryId == 1) { // Incoming Call
            if (eventId == 0) { // Added
                currentCallUid_ = uid;
                callActive_ = true;

                // Fire event immediately so UI shows incoming call screen instantly
                if (callCb_) {
                    callCb_(BleCallAction::Incoming, "Incoming Call", "", callUserData_);
                }

                // Request Caller Name (Attribute 1) and Caller Number (Attribute 3)
                if (pAncsControlPoint_) {
                    uint8_t getAttrCmd[] = {
                        0x00, // Command: Get Notification Attributes
                        pData[4], pData[5], pData[6], pData[7], // UID
                        0x01, 0x20, 0x00, // Attribute 1 (Title/Caller), max 32 bytes
                        0x03, 0x20, 0x00  // Attribute 3 (Message/Number), max 32 bytes
                    };
                    pAncsControlPoint_->writeValue(getAttrCmd, sizeof(getAttrCmd));
                }
            } else if (eventId == 2) { // Removed (call answered or dismissed)
                if (callActive_ && uid == currentCallUid_) {
                    callActive_ = false;
                    DebugLog::log("ANCS: Incoming call ended/dismissed");
                    if (callCb_) {
                        callCb_(BleCallAction::Ended, "", "", callUserData_);
                    }
                }
            }
        }
    }

    void handleAncsDataSource(uint8_t* pData, size_t length) {
        if (length < 8 || pData[0] != 0) return; // CommandID 0 = GetNotificationAttributes
        size_t idx = 5; // Skip CommandID (1) + UID (4)
        char caller[32] = "";
        char number[20] = "";

        while (idx + 3 <= length) {
            uint8_t attrId = pData[idx];
            uint16_t attrLen = (uint16_t)pData[idx + 1] | ((uint16_t)pData[idx + 2] << 8);
            idx += 3;
            if (idx + attrLen > length) break;

            if (attrId == 1) { // Title / Caller name
                size_t cpy = (attrLen < sizeof(caller) - 1) ? attrLen : (sizeof(caller) - 1);
                memcpy(caller, &pData[idx], cpy);
                caller[cpy] = '\0';
            } else if (attrId == 3) { // Message / Number
                size_t cpy = (attrLen < sizeof(number) - 1) ? attrLen : (sizeof(number) - 1);
                memcpy(number, &pData[idx], cpy);
                number[cpy] = '\0';
            }
            idx += attrLen;
        }

        if (strlen(caller) > 0 && callActive_) {
            DebugLog::log("ANCS: Resolved caller: '%s' (%s)", caller, number);
            if (callCb_) {
                callCb_(BleCallAction::Incoming, caller, number, callUserData_);
            }
        }
    }

    void handleAmsEntityUpdate(uint8_t* pData, size_t length) {
        if (length < 3) return;
        uint8_t entityId = pData[0];
        uint8_t attrId = pData[1];
        size_t strLen = length - 3;
        const char* strVal = reinterpret_cast<const char*>(&pData[3]);

        if (entityId == 2) { // EntityIDTrack
            if (attrId == 2) { // Title
                size_t cpy = (strLen < sizeof(trackTitle_) - 1) ? strLen : (sizeof(trackTitle_) - 1);
                memcpy(trackTitle_, strVal, cpy);
                trackTitle_[cpy] = '\0';
            } else if (attrId == 0) { // Artist
                size_t cpy = (strLen < sizeof(trackArtist_) - 1) ? strLen : (sizeof(trackArtist_) - 1);
                memcpy(trackArtist_, strVal, cpy);
                trackArtist_[cpy] = '\0';
            }
            DebugLog::log("AMS: Track updated: '%s' by '%s'", trackTitle_, trackArtist_);
            if (mediaCb_) {
                mediaCb_(isPlaying_, trackTitle_, trackArtist_, mediaUserData_);
            }
        } else if (entityId == 0) { // EntityIDPlayer
            if (attrId == 1 && strLen > 0) { // PlaybackInfo "{PlaybackState},{PlaybackRate},{ElapsedTime}"
                // PlaybackState: 0=Paused, 1=Playing, 2=Rewinding, 3=FastForwarding
                isPlaying_ = (strVal[0] == '1');
                DebugLog::log("AMS: Playback state updated: playing=%d", int(isPlaying_));
                if (mediaCb_) {
                    mediaCb_(isPlaying_, trackTitle_, trackArtist_, mediaUserData_);
                }
            }
        }
    }

    static void discoveryTaskEntry(void* param) {
        auto* self = static_cast<Impl*>(param);
        self->runDiscovery();
    }

    void runDiscovery() {
        DebugLog::log("BLE-Apple: Waiting for BLE encryption / bonding handshake...");
        vTaskDelay(pdMS_TO_TICKS(1200));

        if (!running_) {
            vTaskDelete(NULL);
            return;
        }

        DebugLog::log("BLE-Apple: Connecting GATT client to peer for ANCS / AMS...");
        pClient_ = BLEDevice::createClient();
        if (!pClient_) {
            DebugLog::log("BLE-Apple: Failed to allocate BLEClient");
            vTaskDelete(NULL);
            return;
        }

        BLEAddress addr(peerBda_);
        if (!pClient_->connect(addr, addrType_)) {
            DebugLog::log("BLE-Apple: Client connect failed to %s", addr.toString().c_str());
            delete pClient_;
            pClient_ = nullptr;
            vTaskDelete(NULL);
            return;
        }

        DebugLog::log("BLE-Apple: Connected to peer! Searching for Apple services...");

        // 1. Discover ANCS Service
        BLERemoteService* pAncsService = pClient_->getService(BLEUUID(ANCS_SERVICE_UUID));
        if (pAncsService) {
            DebugLog::log("BLE-Apple: Found ANCS Service!");
            pAncsNotifSource_ = pAncsService->getCharacteristic(BLEUUID(ANCS_CHAR_NOTIF_SOURCE));
            pAncsControlPoint_ = pAncsService->getCharacteristic(BLEUUID(ANCS_CHAR_CONTROL_POINT));
            pAncsDataSource_ = pAncsService->getCharacteristic(BLEUUID(ANCS_CHAR_DATA_SOURCE));

            if (pAncsNotifSource_) {
                pAncsNotifSource_->registerForNotify([this](BLERemoteCharacteristic*, uint8_t* pData, size_t length, bool) {
                    this->handleAncsNotification(pData, length);
                });
            }
            if (pAncsDataSource_) {
                pAncsDataSource_->registerForNotify([this](BLERemoteCharacteristic*, uint8_t* pData, size_t length, bool) {
                    this->handleAncsDataSource(pData, length);
                });
            }
        } else {
            DebugLog::log("BLE-Apple: ANCS Service not available on peer");
        }

        // 2. Discover AMS Service
        BLERemoteService* pAmsService = pClient_->getService(BLEUUID(AMS_SERVICE_UUID));
        if (pAmsService) {
            DebugLog::log("BLE-Apple: Found AMS Service!");
            pAmsRemoteCmd_ = pAmsService->getCharacteristic(BLEUUID(AMS_CHAR_REMOTE_CMD));
            pAmsEntityUpdate_ = pAmsService->getCharacteristic(BLEUUID(AMS_CHAR_ENTITY_UPDATE));

            if (pAmsEntityUpdate_) {
                pAmsEntityUpdate_->registerForNotify([this](BLERemoteCharacteristic*, uint8_t* pData, size_t length, bool) {
                    this->handleAmsEntityUpdate(pData, length);
                });

                // Subscribe to Track attributes: 0=Artist, 1=Album, 2=Title, 3=Duration
                uint8_t trackSub[] = { 0x02, 0x00, 0x01, 0x02, 0x03 };
                pAmsEntityUpdate_->writeValue(trackSub, sizeof(trackSub));

                // Subscribe to Player attributes: 1=PlaybackInfo
                uint8_t playerSub[] = { 0x00, 0x01 };
                pAmsEntityUpdate_->writeValue(playerSub, sizeof(playerSub));
                DebugLog::log("BLE-Apple: Subscribed to AMS Track & Player updates");
            }
        } else {
            DebugLog::log("BLE-Apple: AMS Service not available on peer");
        }

        while (running_ && pClient_ && pClient_->isConnected()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        DebugLog::log("BLE-Apple: Client discovery task finished");
        if (pClient_) {
            pClient_->disconnect();
            delete pClient_;
            pClient_ = nullptr;
        }
        pAncsNotifSource_ = nullptr;
        pAncsControlPoint_ = nullptr;
        pAncsDataSource_ = nullptr;
        pAmsRemoteCmd_ = nullptr;
        pAmsEntityUpdate_ = nullptr;
        taskHandle_ = nullptr;
        vTaskDelete(NULL);
    }
};

Esp32AppleClient::Esp32AppleClient() : pImpl_(new Impl()) {
    pImpl_->parent_ = this;
}

Esp32AppleClient::~Esp32AppleClient() {
    stop();
    delete pImpl_;
}

void Esp32AppleClient::setCallCallback(BleCallCallback cb, void* userData) {
    pImpl_->callCb_ = cb;
    pImpl_->callUserData_ = userData;
}

void Esp32AppleClient::setMediaCallback(BleMediaCallback cb, void* userData) {
    pImpl_->mediaCb_ = cb;
    pImpl_->mediaUserData_ = userData;
}

void Esp32AppleClient::startDiscovery(const esp_bd_addr_t bda, esp_ble_addr_type_t addrType) {
    stop();
    memcpy(pImpl_->peerBda_, bda, sizeof(esp_bd_addr_t));
    pImpl_->addrType_ = addrType;
    pImpl_->running_ = true;

    xTaskCreate(&Impl::discoveryTaskEntry, "apple_ble", 4096, pImpl_, 5, &pImpl_->taskHandle_);
}

void Esp32AppleClient::stop() {
    pImpl_->running_ = false;
    if (pImpl_->taskHandle_) {
        // Wait briefly for task to terminate itself
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

bool Esp32AppleClient::isAncsActive() const {
    return pImpl_->pAncsControlPoint_ != nullptr;
}

bool Esp32AppleClient::isAmsActive() const {
    return pImpl_->pAmsRemoteCmd_ != nullptr;
}

void Esp32AppleClient::acceptCall() {
    if (pImpl_->pAncsControlPoint_ && pImpl_->callActive_) {
        DebugLog::log("ANCS: Performing ActionIDPositive (Accept Call)");
        uint8_t cmd[6] = {
            0x02, // Action command
            static_cast<uint8_t>(pImpl_->currentCallUid_),
            static_cast<uint8_t>(pImpl_->currentCallUid_ >> 8),
            static_cast<uint8_t>(pImpl_->currentCallUid_ >> 16),
            static_cast<uint8_t>(pImpl_->currentCallUid_ >> 24),
            0x00  // ActionIDPositive (Answer Call)
        };
        pImpl_->pAncsControlPoint_->writeValue(cmd, sizeof(cmd));
    }
}

void Esp32AppleClient::rejectCall() {
    if (pImpl_->pAncsControlPoint_ && pImpl_->callActive_) {
        DebugLog::log("ANCS: Performing ActionIDNegative (Decline Call)");
        uint8_t cmd[6] = {
            0x02, // Action command
            static_cast<uint8_t>(pImpl_->currentCallUid_),
            static_cast<uint8_t>(pImpl_->currentCallUid_ >> 8),
            static_cast<uint8_t>(pImpl_->currentCallUid_ >> 16),
            static_cast<uint8_t>(pImpl_->currentCallUid_ >> 24),
            0x01  // ActionIDNegative (Decline Call)
        };
        pImpl_->pAncsControlPoint_->writeValue(cmd, sizeof(cmd));
    }
}

void Esp32AppleClient::mediaCommand(BleMediaAction action) {
    if (!pImpl_->pAmsRemoteCmd_) return;

    uint8_t cmd = 0xFF;
    switch (action) {
        case BleMediaAction::Toggle:
            cmd = 0x02; // RemoteCommandIDTogglePlayPause
            break;
        case BleMediaAction::Play:
            cmd = 0x00; // RemoteCommandIDPlay
            break;
        case BleMediaAction::Pause:
            cmd = 0x01; // RemoteCommandIDPause
            break;
        case BleMediaAction::Next:
            cmd = 0x03; // RemoteCommandIDNextTrack
            break;
        case BleMediaAction::Previous:
            cmd = 0x04; // RemoteCommandIDPreviousTrack
            break;
        default:
            break;
    }

    if (cmd != 0xFF) {
        DebugLog::log("AMS: Sending Remote Command 0x%02X", cmd);
        pImpl_->pAmsRemoteCmd_->writeValue(&cmd, 1);
    }
}

} // namespace hal
} // namespace ersa

#endif
