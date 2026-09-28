#include "ersa/services/network_manager.h"
#include "ersa/common/logging.h"

#if defined(ARDUINO)
#include <WiFi.h>
#endif

namespace ersa {
namespace services {

static NetworkManager* s_networkManagerInstance = nullptr;

NetworkHandle::NetworkHandle() = default;

NetworkHandle::NetworkHandle(NetworkManager* mgr)
    : mgr_(mgr), active_(false) {
    if (mgr_) {
        mgr_->acquire();
        active_ = true;
    }
}

NetworkHandle::~NetworkHandle() {
    release();
}

NetworkHandle::NetworkHandle(NetworkHandle&& other) noexcept
    : mgr_(other.mgr_), active_(other.active_) {
    other.mgr_ = nullptr;
    other.active_ = false;
}

NetworkHandle& NetworkHandle::operator=(NetworkHandle&& other) noexcept {
    if (this != &other) {
        release();
        mgr_ = other.mgr_;
        active_ = other.active_;
        other.mgr_ = nullptr;
        other.active_ = false;
    }
    return *this;
}

void NetworkHandle::release() {
    if (active_ && mgr_) {
        mgr_->release();
        active_ = false;
        mgr_ = nullptr;
    }
}

NetworkManager& NetworkManager::instance() {
    return *s_networkManagerInstance;
}

void NetworkManager::setInstance(NetworkManager* instance) {
    s_networkManagerInstance = instance;
}

NetworkManager::NetworkManager(events::EventBus& bus)
    : bus_(bus) {}

Result<void> NetworkManager::init() {
    connected_ = false;
    connecting_ = false;
    activeHandles_ = 0;
    return Result<void>();
}

NetworkHandle NetworkManager::requestInternet() {
    return NetworkHandle(this);
}

void NetworkManager::acquire() {
    activeHandles_++;
    if (!connected_ && !connecting_) {
        connecting_ = true;
        ERSA_LOG_INFO("NetworkManager: radio wake requested (active handles=%zu)", activeHandles_);
#if defined(ARDUINO)
        // Handled via net_sync / WiFi connection
#else
        connected_ = true;
        connecting_ = false;
        events::Event evt(events::EventType::NetworkConnected);
        evt.network.connected = true;
        bus_.publish(evt);
#endif
    }
}

void NetworkManager::release() {
    if (activeHandles_ > 0) {
        activeHandles_--;
    }
    if (activeHandles_ == 0 && (connected_ || connecting_)) {
        ERSA_LOG_INFO("NetworkManager: radio idle, powering down");
        connected_ = false;
        connecting_ = false;
#if defined(ARDUINO)
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
#endif
        events::Event evt(events::EventType::NetworkDisconnected);
        evt.network.connected = false;
        bus_.publish(evt);
    }
}

bool NetworkManager::isConnected() const {
#if defined(ARDUINO)
    return WiFi.status() == WL_CONNECTED;
#else
    return connected_;
#endif
}

bool NetworkManager::isConnecting() const {
    return connecting_;
}

size_t NetworkManager::getActiveHandleCount() const {
    return activeHandles_;
}

void NetworkManager::tick(uint32_t currentUptimeMs) {
    (void)currentUptimeMs;
}

} // namespace services
} // namespace ersa
