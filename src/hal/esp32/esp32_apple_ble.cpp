#if defined(ARDUINO)

#include <Arduino.h>
#include "hal/esp32/esp32_apple_ble.h"
#include <BLEDevice.h>
#include <BLEClient.h>
#include <esp_gattc_api.h>
#include <BLERemoteService.h>
#include <BLERemoteCharacteristic.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <atomic>
#include "ersa/protocols/apple_notifications.h"
#include "ersa/protocols/apple_media.h"
#include "ersa/protocols/ble_current_time.h"
#include "core/debug_log.h"

#define ANCS_SERVICE_UUID           "7905f431-b5ce-4e99-a40f-4b1e122d00d0"
#define ANCS_CHAR_NOTIF_SOURCE      "9fbf120d-6301-42d9-8c58-25e699a21dbd"
#define ANCS_CHAR_CONTROL_POINT     "69d1d8f3-45e1-49a8-9821-9bbdfdaad9d9"
#define ANCS_CHAR_DATA_SOURCE       "22eac6e9-24d6-4bb5-be44-b36ace7c7bfb"

#define AMS_SERVICE_UUID            "89d3502b-0f36-433a-8ef4-c502ad55f8dc"
#define CTS_SERVICE_UUID            "00001805-0000-1000-8000-00805f9b34fb"
#define CTS_CHAR_CURRENT_TIME       "00002a2b-0000-1000-8000-00805f9b34fb"
#define AMS_CHAR_REMOTE_CMD         "9b3c81d8-57b1-4a8a-b8df-0e56f7ca51c2"
#define AMS_CHAR_ENTITY_UPDATE      "2f7cabce-808d-411f-9a0c-bb92ba96c102"
#define AMS_CHAR_ENTITY_ATTR        "c6b2f38c-23ab-46d8-a6ab-a3a870bbd5d7"
#define CTS_SERVICE_UUID            "00001805-0000-1000-8000-00805f9b34fb"
#define CTS_CHAR_CURRENT_TIME       "00002a2b-0000-1000-8000-00805f9b34fb"

namespace ersa {
namespace hal {

namespace {
bool isPhoneNumberText(const char* text) {
    if (!text || !text[0]) return false;
    unsigned digits = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
        if (*p >= '0' && *p <= '9') ++digits;
        else if (*p != '+' && *p != '(' && *p != ')' && *p != '-' && *p != ' ' && *p != '.') return false;
    }
    return digits >= 5;
}
}

class Esp32AppleClient::Impl {
public:
    BleCallCallback callCb_{nullptr};
    void* callUserData_{nullptr};
    BleMediaCallback mediaCb_{nullptr};
    void* mediaUserData_{nullptr};
    BleNotificationCallback notifCb_{nullptr};
    void* notifUserData_{nullptr};
    BleTimeCallback timeCb_{nullptr};
    void* timeUserData_{nullptr};

    enum Kind : uint8_t { Notification, Attributes, Media, CallAction, NotificationAction, MediaAction, MediaCommands, ServicesChanged, CurrentTime };
    struct Packet {
        Kind kind;
        uint32_t session;
        uint32_t uid;
        uint32_t epoch;
        size_t size;
        uint8_t data[512];
    };
    // Notification Source emits an initial burst of 8-byte records. Keep these
    // separate from fragmented responses: losing history must not poison ANCS.
    struct SourcePacket { uint32_t session; uint32_t epoch; uint8_t data[8]; };
    QueueHandle_t sourceQueue_{xQueueCreate(64, sizeof(SourcePacket))};
    QueueHandle_t callQueue_{xQueueCreate(8, sizeof(SourcePacket))};
    QueueHandle_t queue_{xQueueCreate(24, sizeof(Packet))};
    std::atomic<uint32_t> ancsEpoch_{0}, droppedHistory_{0}, droppedOther_{0};
    TaskHandle_t task_{nullptr};
    portMUX_TYPE controlMux_ = portMUX_INITIALIZER_UNLOCKED;
    esp_bd_addr_t peer_{};
    esp_ble_addr_type_t addrType_{BLE_ADDR_TYPE_RANDOM};
    std::atomic<uint32_t> generation_{0};
    std::atomic<uint32_t> authenticatedGeneration_{UINT32_MAX};
    bool wanted_{false}; // protected by controlMux_
    std::atomic<bool> shutdown_{false}, exited_{false};
    std::atomic<bool> ancsReady_{false}, amsReady_{false}, overflow_{false};
    std::atomic<uint32_t> publishedCallUid_{0};
    uint32_t session_{0};

    // The worker alone owns all remote pointers and protocol state. Keep one
    // BLEClient for the BLEDevice lifetime: the library retains its GAP pointer.
    BLEClient* client_{nullptr};
    BLERemoteCharacteristic* control_{nullptr};
    BLERemoteCharacteristic* remote_{nullptr};
    protocols::AncsCall call_;
    struct Request { uint32_t uid; bool call; bool removed; uint8_t flags; };
    Request pending_[16]{};
    size_t pendingCount_{0};
    Request active_{};
    struct DismissTarget { uint32_t uid{0}; bool allowed{false}; };
    DismissTarget dismissTargets_[16]{};
    bool waiting_{false};
    bool attributesBlocked_{false};
    uint8_t recoveryAttempts_{0};
    uint32_t recoveryAt_{0};
    uint32_t requestedAt_{0};
    protocols::AncsAttributes attributes_;
    protocols::AmsMedia media_;
    uint32_t supportedCommands_{0};
    BLERemoteCharacteristic* source_{nullptr};
    BLERemoteCharacteristic* data_{nullptr};
    BLERemoteCharacteristic* update_{nullptr};
    BLERemoteCharacteristic* entityAttribute_{nullptr};
    bool servicesChanged_{false};
    BLERemoteCharacteristic* changed_{nullptr};
    BLERemoteCharacteristic* currentTime_{nullptr};

    bool live() const { return !shutdown_ && session_ == generation_.load(); }

    void enqueue(Kind kind, uint32_t session, const uint8_t* bytes, size_t size,
                 uint32_t uid = 0, uint32_t epoch = 0) {
        if (session != generation_.load() || !queue_) return;
        if ((kind == Notification || kind == Attributes) && epoch != ancsEpoch_.load()) return;
        if (kind == Notification) {
            if (size != 8) return;
            SourcePacket source{};
            source.session = session; source.epoch = epoch;
            memcpy(source.data, bytes, 8);
            auto queue = bytes[2] == 1 ? callQueue_ : sourceQueue_;
            if (!queue) return;
            if (xQueueSend(queue, &source, 0) != pdTRUE) {
                SourcePacket oldest;
                xQueueReceive(queue, &oldest, 0);
                xQueueSend(queue, &source, 0);
                ++droppedHistory_;
            }
            return;
        }
        Packet p{};
        p.kind = kind; p.session = session; p.uid = uid; p.size = size; p.epoch = epoch;
        if (size > sizeof(p.data)) {
            if (kind == Attributes) overflow_ = true;
            else ++droppedOther_;
            return;
        }
        if (size) memcpy(p.data, bytes, size);
        if (xQueueSend(queue_, &p, 0) != pdTRUE) {
            if (kind == Attributes) overflow_ = true;
            else ++droppedOther_;
        }
    }

    bool requestNotificationDismiss(uint32_t uid) {
        if (!ancsReady_ || !uid || !queue_) return false;
        Packet packet{};
        packet.kind = NotificationAction;
        packet.session = generation_.load();
        packet.uid = uid;
        return xQueueSend(queue_, &packet, 0) == pdTRUE;
    }

    void changePeer(const uint8_t* address, esp_ble_addr_type_t type) {
        portENTER_CRITICAL(&controlMux_);
        wanted_ = address != nullptr;
        if (address) { memcpy(peer_, address, sizeof(peer_)); addrType_ = type; }
        ++generation_;
        authenticatedGeneration_ = UINT32_MAX;
        ancsReady_ = false; amsReady_ = false;
        portEXIT_CRITICAL(&controlMux_);
        // No waits or GATT operations on the Bluetooth callback thread.
        if (address && !task_ && queue_ && sourceQueue_ && callQueue_) {
            if (xTaskCreate(taskEntry, "apple_ble", 6144, this, 3, &task_) != pdPASS)
                DebugLog::log("BLE-Apple: Cannot start worker");
        }
    }

    void handleNotification(const uint8_t* data, size_t size) {
        if (size != 8 || data[0] > 2) return;
        const uint32_t uid = protocols::readLe32(data + 4);
        if (data[0] == 2) {
            for (auto& target : dismissTargets_) if (target.uid == uid) target = {};
            if (waiting_ && active_.uid == uid) active_.removed = true;
            for (size_t i = 0; i < pendingCount_; ++i)
                if (pending_[i].uid == uid) pending_[i].removed = true;
            if (notifCb_) notifCb_(nullptr, nullptr, "", uid, false, notifUserData_);
            if (call_.remove(uid) && callCb_)
                callCb_(BleCallAction::Ended, "", "", callUserData_);
            return;
        }
        const bool incoming = data[2] == 1;
        for (auto& target : dismissTargets_) if (target.uid == uid) target.allowed = false;
        if (incoming) {
            const bool fresh = !call_.ringing || call_.uid != uid;
            call_.update(uid, data[1]);
            publishedCallUid_ = uid;
            if (fresh && callCb_) callCb_(BleCallAction::Incoming, "", "", callUserData_);
        }
        // Coalesce queued modifications without mixing attributes across UIDs.
        for (size_t i = 0; i < pendingCount_; ++i) {
            if (pending_[i].uid == uid) { pending_[i] = {uid, incoming, false, data[1]}; return; }
        }
        if (attributesBlocked_) return;
        if (pendingCount_ == 16) {
            if (incoming) --pendingCount_;
            else {
                // Keep newest history, retaining any priority call at the front.
                const size_t oldest = pending_[0].call ? 1 : 0;
                for (size_t i = oldest + 1; i < pendingCount_; ++i) pending_[i - 1] = pending_[i];
                --pendingCount_;
            }
            ++droppedHistory_;
        }
        if (incoming) {
            for (size_t i = pendingCount_; i > 0; --i) pending_[i] = pending_[i - 1];
            pending_[0] = {uid, true, false, data[1]};
            ++pendingCount_;
        } else pending_[pendingCount_++] = {uid, false, false, data[1]};
    }

    void requestNext() {
        if (waiting_ || attributesBlocked_ || !control_ || !live()) return;
        while (pendingCount_) {
            active_ = pending_[0];
            for (size_t i = 1; i < pendingCount_; ++i) pending_[i - 1] = pending_[i];
            --pendingCount_;
            if (active_.removed) continue;
            uint8_t cmd[14];
            const bool requestNegativeLabel = !active_.call && (active_.flags & 16);
            protocols::AncsAttributes::request(active_.uid, requestNegativeLabel, cmd);
            attributes_.begin(active_.uid, requestNegativeLabel);
            waiting_ = true;
            requestedAt_ = millis();
            control_->writeValue(cmd, requestNegativeLabel ? 14 : 11, true);
            break;
        }
    }

    void handleAttributes(const uint8_t* data, size_t size) {
        if (!waiting_) return;
        const auto result = attributes_.feed(data, size);
        if (result == protocols::AncsAttributes::Result::Invalid) {
            // A missing fragment cannot be resynchronized by guessing a header.
            overflow_ = true;
            return;
        }
        if (result != protocols::AncsAttributes::Result::Complete) return;
        waiting_ = false;
        if (active_.removed || !live()) return;
        if (active_.call) {
            if (call_.ringing && call_.uid == active_.uid && callCb_) {
                const bool messageIsNumber = isPhoneNumberText(attributes_.message);
                const char* caller = attributes_.title[0] ? attributes_.title :
                                     (messageIsNumber ? "" : attributes_.message);
                const char* number = messageIsNumber ? attributes_.message : "";
                callCb_(BleCallAction::Incoming, caller, number, callUserData_);
            }
        } else if (notifCb_) {
            const bool canDismiss = (active_.flags & 16) && attributes_.negativeActionIsDismissal();
            DismissTarget* slot = nullptr;
            for (auto& target : dismissTargets_) if (target.uid == active_.uid) { slot = &target; break; }
            if (!slot) for (auto& target : dismissTargets_) if (!target.uid) { slot = &target; break; }
            if (!slot) slot = &dismissTargets_[0];
            *slot = {active_.uid, canDismiss};
            notifCb_(attributes_.title[0] ? attributes_.title : "notification",
                     attributes_.message, "iPhone", active_.uid, canDismiss, notifUserData_);
        }
    }

    void process(const Packet& p) {
        if (p.session != session_ || !live()) return;
        if (p.kind == Attributes && p.epoch != ancsEpoch_.load()) return;
        switch (p.kind) {
            case Notification: handleNotification(p.data, p.size); break;
            case Attributes: handleAttributes(p.data, p.size); break;
            case Media: handleAmsEntityUpdate(p.data, p.size); break;
            case CallAction: {
                uint8_t command[6];
                if (control_ && call_.action(p.uid, p.data[0] == 0, command)) {
                    control_->writeValue(command, sizeof(command), true);
                    // Submission is not proof that a call became active.
                }
                break;
            }
            case NotificationAction: {
                if (!control_) break;
                bool allowed = false;
                for (const auto& target : dismissTargets_)
                    if (target.uid == p.uid) { allowed = target.allowed; break; }
                if (!allowed) break;
                uint8_t command[6] = {2};
                protocols::writeLe32(command + 1, p.uid);
                command[5] = 1; // ANCS negative action, validated against the advertised label.
                control_->writeValue(command, sizeof(command), true);
                break;
            }
            case ServicesChanged: servicesChanged_ = true; break;
            case CurrentTime: {
                uint32_t epoch = 0;
                if (protocols::decodeCurrentTime(p.data, p.size, epoch) && timeCb_)
                    timeCb_(epoch, timeUserData_);
                else
                    DebugLog::log("BLE-CTS: ignored invalid Current Time value (%u bytes)", unsigned(p.size));
                break;
            }
            case MediaCommands:
                supportedCommands_ = 0;
                for (size_t i = 0; i < p.size; ++i)
                    if (p.data[i] < 32) supportedCommands_ |= 1UL << p.data[i];
                break;
            case MediaAction:
                if (remote_ && p.data[0] < 32 && (supportedCommands_ & (1UL << p.data[0]))) {
                    uint8_t command = p.data[0];
                    remote_->writeValue(&command, 1, true);
                }
                break;
        }
    }

    void subscribe(BLERemoteCharacteristic* characteristic, Kind kind, bool notifications = true) {
        const uint32_t session = session_;
        const uint32_t epoch = ancsEpoch_;
        DebugLog::log("BLE-Apple: registering notify kind=%u handle=0x%04x", unsigned(kind), characteristic->getHandle());
        // Finish local registration before issuing a remote CCCD write. The
        // Arduino helper otherwise overlaps those two asynchronous operations.
        characteristic->registerForNotify([this, kind, session, epoch](BLERemoteCharacteristic*, uint8_t* data, size_t size, bool) {
            enqueue(kind, session, data, size, 0, epoch);
        }, notifications, false);
        if (!live() || !client_->isConnected()) return;
        DebugLog::log("BLE-Apple: enabling CCCD kind=%u", unsigned(kind));
        auto* descriptor = characteristic->getDescriptor(BLEUUID(uint16_t(0x2902)));
        if (descriptor) {
            uint8_t value[] = {uint8_t(notifications ? 1 : 2), 0};
            if (kind == ServicesChanged) {
                // This optional indication may be rejected by iOS. The Arduino
                // descriptor helper waits forever for a response in that case.
                const auto error = esp_ble_gattc_write_char_descr(
                    client_->getGattcIf(), client_->getConnId(), descriptor->getHandle(),
                    sizeof(value), value, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
                DebugLog::log("BLE-Apple: Service Changed CCCD submitted rc=0x%x", unsigned(error));
            } else {
                descriptor->writeValue(value, sizeof(value), true);
            }
        } else DebugLog::log("BLE-Apple: missing CCCD kind=%u", unsigned(kind));
        DebugLog::log("BLE-Apple: subscription complete kind=%u", unsigned(kind));
    }

    bool discover() {
        // getServices() deletes the previous service objects. Never call it
        // after subscriptions have been installed or retain pointers across it.
        DebugLog::log("BLE-Apple: discovering services");
        client_->getServices();
        if (!live() || !client_->isConnected()) return false;
        DebugLog::log("BLE-Apple: service search complete");
        auto* ancs = client_->getService(BLEUUID(ANCS_SERVICE_UUID));
        auto* ams = client_->getService(BLEUUID(AMS_SERVICE_UUID));
        auto* cts = client_->getService(BLEUUID(CTS_SERVICE_UUID));
        DebugLog::log("BLE: optional services ANCS=%d AMS=%d CTS=%d", ancs != nullptr, ams != nullptr, cts != nullptr);
        if (cts && live()) {
            currentTime_ = cts->getCharacteristic(BLEUUID(CTS_CHAR_CURRENT_TIME));
            if (currentTime_ && currentTime_->canRead()) {
                const std::string value = currentTime_->readValue();
                uint32_t epoch = 0;
                if (protocols::decodeCurrentTime(reinterpret_cast<const uint8_t*>(value.data()), value.size(), epoch) && timeCb_) {
                    timeCb_(epoch, timeUserData_);
                    DebugLog::log("BLE-CTS: initial time read submitted");
                } else {
                    DebugLog::log("BLE-CTS: initial read unavailable or invalid (%u bytes)", unsigned(value.size()));
                }
            }
            if (currentTime_ && currentTime_->canNotify() && live()) {
                subscribe(currentTime_, CurrentTime);
                DebugLog::log("BLE-CTS: time-change notifications enabled");
            } else if (currentTime_) {
                DebugLog::log("BLE-CTS: characteristic has no notify property");
            }
        } else {
            DebugLog::log("BLE-CTS: service unavailable; network/manual time sources remain available");
        }
        if (ancs) {
            DebugLog::log("BLE-Apple: resolving ANCS characteristics");
            auto* source = source_ = ancs->getCharacteristic(BLEUUID(ANCS_CHAR_NOTIF_SOURCE));
            auto* data = data_ = ancs->getCharacteristic(BLEUUID(ANCS_CHAR_DATA_SOURCE));
            control_ = ancs->getCharacteristic(BLEUUID(ANCS_CHAR_CONTROL_POINT));
            if (source && data && control_) {
                // Subscribe after AMS setup so the worker can drain the initial burst immediately.
            } else control_ = nullptr;
        }
        if (ams) {
            DebugLog::log("BLE-Apple: resolving AMS characteristics");
            auto* update = update_ = ams->getCharacteristic(BLEUUID(AMS_CHAR_ENTITY_UPDATE));
            entityAttribute_ = ams->getCharacteristic(BLEUUID(AMS_CHAR_ENTITY_ATTR));
            remote_ = ams->getCharacteristic(BLEUUID(AMS_CHAR_REMOTE_CMD));
            if (update && remote_) {
                subscribe(remote_, MediaCommands);
                subscribe(update, Media);
                uint8_t track[] = {2, 0, 2};
                uint8_t player[] = {0, 1};
                DebugLog::log("BLE-Apple: requesting AMS track updates");
                update->writeValue(track, sizeof(track), true);
                DebugLog::log("BLE-Apple: requesting AMS playback updates");
                update->writeValue(player, sizeof(player), true);
                amsReady_ = live();
            } else remote_ = nullptr;
        }
        if (control_ && live()) {
            subscribe(data_, Attributes);
            subscribe(source_, Notification);
            ancsReady_ = live();
        }
        DebugLog::log("BLE-Apple: ANCS=%d AMS=%d", int(ancsReady_.load()), int(amsReady_.load()));
        if ((control_ || remote_) && live()) {
            auto* gatt = client_->getService(BLEUUID(uint16_t(0x1801)));
            changed_ = gatt ? gatt->getCharacteristic(BLEUUID(uint16_t(0x2a05))) : nullptr;
            if (changed_) subscribe(changed_, ServicesChanged, false);
        }
        return control_ || remote_ || currentTime_;
    }

    void restartAncs() {
        if (!live() || !client_->isConnected() || !control_) return;
        ancsReady_ = false;
        ++ancsEpoch_;
        source_->registerForNotify(nullptr);
        data_->registerForNotify(nullptr);
        waiting_ = false; pendingCount_ = 0;
        xQueueReset(sourceQueue_); xQueueReset(callQueue_);
        if (call_.ringing && callCb_) callCb_(BleCallAction::Ended, "", "", callUserData_);
        call_ = {};
        for (auto& target : dismissTargets_) target = {};
        if (notifCb_) notifCb_(nullptr, nullptr, nullptr, 0, false, notifUserData_);
        if (!live() || !client_->isConnected()) return;
        subscribe(data_, Attributes);
        subscribe(source_, Notification);
        attributesBlocked_ = false;
        ancsReady_ = live();
        DebugLog::log("ANCS: subscriptions restored, attempt=%u", unsigned(recoveryAttempts_));
    }

    void resetSession() {
        ancsReady_ = false; amsReady_ = false;
        control_ = remote_ = source_ = data_ = update_ = entityAttribute_ = changed_ = currentTime_ = nullptr;
        waiting_ = false; pendingCount_ = 0; overflow_ = false;
        for (auto& target : dismissTargets_) target = {};
        attributesBlocked_ = false; recoveryAttempts_ = 0;
        ++ancsEpoch_; droppedHistory_ = 0; droppedOther_ = 0;
        xQueueReset(sourceQueue_); xQueueReset(callQueue_);
        call_ = {}; publishedCallUid_ = 0;
        media_ = {}; supportedCommands_ = 0; servicesChanged_ = false;
        Packet ignored;
        while (xQueueReceive(queue_, &ignored, 0) == pdTRUE) {}
    }

    void pause(uint32_t ms) {
        const uint32_t start = millis();
        while (live() && uint32_t(millis() - start) < ms) vTaskDelay(pdMS_TO_TICKS(50));
    }

    static void taskEntry(void* value) { static_cast<Impl*>(value)->run(); }
    void run() {
        DebugLog::log("BLE-Apple: burst-safe worker started (separate history/call queues)");
        client_ = BLEDevice::createClient();
        while (!shutdown_) {
            esp_bd_addr_t peer;
            esp_ble_addr_type_t type;
            portENTER_CRITICAL(&controlMux_);
            session_ = generation_.load();
            const bool wanted = wanted_;
            memcpy(peer, peer_, sizeof(peer)); type = addrType_;
            portEXIT_CRITICAL(&controlMux_);
            resetSession();
            if (!wanted) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
            DebugLog::log("BLE-Apple: waiting for authentication (session=%lu)", (unsigned long)session_);
            uint32_t lastWaitLog = millis();
            while (live() && authenticatedGeneration_.load() != session_) {
                vTaskDelay(pdMS_TO_TICKS(50));
                if (uint32_t(millis() - lastWaitLog) >= 5000) {
                    DebugLog::log("BLE-Apple: still waiting for authentication");
                    lastWaitLog = millis();
                }
            }
            if (!live()) continue;
            DebugLog::log("BLE-Apple: attaching GATT client");
            if (!client_->connect(BLEAddress(peer), type)) {
                DebugLog::log("BLE-Apple: connect failed; retrying");
                pause(3000);
                continue;
            }
            bool ready = false;
            while (live() && client_->isConnected() && !ready) {
                ready = discover();
                if (!ready) pause(2000);
            }
            while (live() && client_->isConnected() && ready) {
                Packet packet;
                const bool received = xQueueReceive(queue_, &packet, pdMS_TO_TICKS(50)) == pdTRUE;
                SourcePacket source;
                while (xQueueReceive(callQueue_, &source, 0) == pdTRUE) {
                    if (source.session == session_ && source.epoch == ancsEpoch_.load() && live())
                        handleNotification(source.data, 8);
                }
                // Bound work per iteration so responses and media remain responsive.
                for (unsigned i = 0; i < 8 && xQueueReceive(sourceQueue_, &source, 0) == pdTRUE; ++i) {
                    if (source.session == session_ && source.epoch == ancsEpoch_.load() && live())
                        handleNotification(source.data, 8);
                }
                // Apply removals before validating a queued button action.
                if (received) process(packet);
                const auto dropped = droppedHistory_.exchange(0);
                if (dropped) DebugLog::log("ANCS: history burst, skipped %lu records; BLE stays connected", (unsigned long)dropped);
                const auto other = droppedOther_.exchange(0);
                if (other) DebugLog::log("BLE-Apple: dropped %lu queued updates", (unsigned long)other);
                if (overflow_.exchange(false) || (waiting_ && uint32_t(millis() - requestedAt_) > 10000)) {
                    DebugLog::log("ANCS: attribute stream lost uid=%lu; recovering subscriptions, keeping BLE", (unsigned long)active_.uid);
                    waiting_ = false; pendingCount_ = 0; attributesBlocked_ = true;
                    recoveryAt_ = millis();
                }
                if (attributesBlocked_ && recoveryAttempts_ < 3 && uint32_t(millis() - recoveryAt_) >= 2000) {
                    ++recoveryAttempts_;
                    restartAncs();
                }
                if (servicesChanged_ && live()) {
                    DebugLog::log("BLE-Apple: service change; rediscovering on existing connection");
                    if (ancsReady_) { source_->registerForNotify(nullptr); data_->registerForNotify(nullptr); }
                    if (amsReady_) { update_->registerForNotify(nullptr); remote_->registerForNotify(nullptr); }
                    if (changed_) changed_->registerForNotify(nullptr, false);
                    if (call_.ringing && callCb_) callCb_(BleCallAction::Ended, "", "", callUserData_);
                    if (notifCb_) notifCb_(nullptr, nullptr, nullptr, 0, false, notifUserData_);
                    resetSession();
                    ready = discover();
                    // Missing Apple services must not close the phone link.
                    while (live() && client_->isConnected() && !ready) { pause(2000); ready = discover(); }
                }
                requestNext();

            }
            ancsReady_ = false; amsReady_ = false;
            if (client_->isConnected()) client_->disconnect();
            // Do not reuse the client until its disconnect callback has completed.
            while (client_->isConnected()) vTaskDelay(pdMS_TO_TICKS(50));
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        exited_ = true;
        vTaskDelete(nullptr);
    }
    void handleAmsEntityUpdate(const uint8_t* bytes, size_t length) {
        if (!media_.update(bytes, length)) return;
        // Truncated track values can be read through Entity Attribute.
        if (length >= 3 && (bytes[2] & 1) && bytes[0] == 2 && entityAttribute_ && live()) {
            uint8_t attribute[] = {bytes[0], bytes[1]};
            entityAttribute_->writeValue(attribute, sizeof(attribute), true);
            const std::string value = entityAttribute_->readValue();
            uint8_t update[34] = {bytes[0], bytes[1], 0};
            const size_t count = value.size() < 31 ? value.size() : 31;
            memcpy(update + 3, value.data(), count);
            media_.update(update, count + 3);
        }
        if (live() && mediaCb_) mediaCb_(media_.playing, media_.title, media_.artist, mediaUserData_);
    }

};

Esp32AppleClient::Esp32AppleClient() : pImpl_(new Impl()) {}
Esp32AppleClient::~Esp32AppleClient() {
    stop();
    pImpl_->shutdown_ = true;
    while (pImpl_->task_ && !pImpl_->exited_) vTaskDelay(pdMS_TO_TICKS(50));
    if (pImpl_->queue_) vQueueDelete(pImpl_->queue_);
    if (pImpl_->sourceQueue_) vQueueDelete(pImpl_->sourceQueue_);
    if (pImpl_->callQueue_) vQueueDelete(pImpl_->callQueue_);
    delete pImpl_;
}
void Esp32AppleClient::setCallCallback(BleCallCallback cb, void* user) { pImpl_->callCb_ = cb; pImpl_->callUserData_ = user; }
void Esp32AppleClient::setMediaCallback(BleMediaCallback cb, void* user) { pImpl_->mediaCb_ = cb; pImpl_->mediaUserData_ = user; }
void Esp32AppleClient::setNotificationCallback(BleNotificationCallback cb, void* user) { pImpl_->notifCb_ = cb; pImpl_->notifUserData_ = user; }
void Esp32AppleClient::setTimeCallback(BleTimeCallback cb, void* user) { pImpl_->timeCb_ = cb; pImpl_->timeUserData_ = user; }
void Esp32AppleClient::startDiscovery(const esp_bd_addr_t bda, esp_ble_addr_type_t type) { pImpl_->changePeer(bda, type); }
void Esp32AppleClient::authenticationComplete(bool success) {
    // A latched generation survives authentication completing before the worker
    // starts waiting. Disconnect/new connection invalidates it in changePeer().
    pImpl_->authenticatedGeneration_ = success ? pImpl_->generation_.load() : UINT32_MAX;
}
void Esp32AppleClient::stop() { pImpl_->changePeer(nullptr, BLE_ADDR_TYPE_RANDOM); }
bool Esp32AppleClient::isAncsActive() const { return pImpl_->ancsReady_; }
bool Esp32AppleClient::isAmsActive() const { return pImpl_->amsReady_; }
bool Esp32AppleClient::dismissNotification(uint32_t uid) {
    return pImpl_->requestNotificationDismiss(uid);
}
void Esp32AppleClient::acceptCall() {
    const uint8_t action = 0;
    pImpl_->enqueue(Impl::CallAction, pImpl_->generation_, &action, 1, pImpl_->publishedCallUid_);
}
void Esp32AppleClient::rejectCall() {
    const uint8_t action = 1;
    pImpl_->enqueue(Impl::CallAction, pImpl_->generation_, &action, 1, pImpl_->publishedCallUid_);
}
void Esp32AppleClient::mediaCommand(BleMediaAction action) {
    uint8_t command;
    switch (action) {
        case BleMediaAction::Play: command = 0; break;
        case BleMediaAction::Pause: command = 1; break;
        case BleMediaAction::Toggle: command = 2; break;
        case BleMediaAction::Next: command = 3; break;
        case BleMediaAction::Previous: command = 4; break;
        case BleMediaAction::VolumeUp: command = 5; break;
        case BleMediaAction::VolumeDown: command = 6; break;
        default: return;
    }
    pImpl_->enqueue(Impl::MediaAction, pImpl_->generation_, &command, 1);
}

} // namespace hal
} // namespace ersa
#endif
