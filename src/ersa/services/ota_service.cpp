#include "ersa/services/ota_service.h"

#include "core/debug_log.h"
#include "core/watch_config.h"
#include "ersa/board/board.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_https_ota.h>
#include <esp_image_format.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <mbedtls/sha256.h>
#include <cstring>
#include <ctime>
#include <cstdlib>

namespace ersa {
namespace services {
namespace {
constexpr uint32_t BOOT_CONFIRM_DELAY_MS = 30000;
constexpr char OTA_BASE_URL[] = "https://pkgs-wearables.ersa.dev";
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr size_t MANIFEST_LIMIT = 768;

struct ManifestBuffer {
    char data[MANIFEST_LIMIT]{};
    size_t length{0};
    bool overflow{false};
};

struct OtaManifest {
    char deviceName[48]{};
    char codename[32]{};
    char manufacturer[48]{};
    char tag[32]{};
    char version[32]{};
    char firmwareUrl[192]{};
    char sha256[65]{};
    size_t size{0};
};

esp_err_t collectManifest(esp_http_client_event_t* event) {
    auto* buffer = static_cast<ManifestBuffer*>(event->user_data);
    if (event->event_id != HTTP_EVENT_ON_DATA || !buffer) return ESP_OK;
    const size_t amount = static_cast<size_t>(event->data_len);
    if (amount >= MANIFEST_LIMIT - buffer->length) {
        buffer->overflow = true;
        return ESP_FAIL;
    }
    memcpy(buffer->data + buffer->length, event->data, amount);
    buffer->length += amount;
    buffer->data[buffer->length] = '\0';
    return ESP_OK;
}

bool jsonString(const char* json, const char* key, char* output, size_t capacity) {
    char pattern[48];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char* value = strstr(json, pattern);
    if (!value) return false;
    value += strlen(pattern);
    const char* end = strchr(value, '"');
    if (!end || size_t(end - value) >= capacity) return false;
    const size_t length = size_t(end - value);
    memcpy(output, value, length);
    output[length] = '\0';
    return true;
}

bool fetchManifest(OtaManifest& manifest) {
    ManifestBuffer body;
    const auto& identity = board::Board::current().getDeviceInfo();
    char manifestUrl[192];
    snprintf(manifestUrl, sizeof(manifestUrl), "%s/ota/%s/ota.json", OTA_BASE_URL,
             identity.codename);
    esp_http_client_config_t config{};
    config.url = manifestUrl;
    config.timeout_ms = 15000;
    config.buffer_size = 512;
    // Use IDF's flash-resident certificate bundle. Parsing a 4 KB RSA root
    // certificate into heap here can fail while BLE and Wi-Fi coexist.
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.event_handler = collectManifest;
    config.user_data = &body;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return false;
    const esp_err_t result = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (result != ESP_OK || status != 200 || body.overflow) {
        DebugLog::log("OTA: manifest request failed status=%d result=0x%x", status, unsigned(result));
        return false;
    }
    if (!strstr(body.data, "\"schema\":1") ||
        !jsonString(body.data, "device_name", manifest.deviceName, sizeof(manifest.deviceName)) ||
        !jsonString(body.data, "codename", manifest.codename, sizeof(manifest.codename)) ||
        !jsonString(body.data, "manufacturer", manifest.manufacturer, sizeof(manifest.manufacturer)) ||
        !jsonString(body.data, "tag", manifest.tag, sizeof(manifest.tag)) ||
        !jsonString(body.data, "version", manifest.version, sizeof(manifest.version)) ||
        !jsonString(body.data, "firmware_url", manifest.firmwareUrl, sizeof(manifest.firmwareUrl)) ||
        !jsonString(body.data, "sha256", manifest.sha256, sizeof(manifest.sha256)) ||
        strlen(manifest.sha256) != 64) return false;

    for (const char* p = manifest.sha256; *p; ++p)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))) return false;
    char expectedUrl[192];
    snprintf(expectedUrl, sizeof(expectedUrl), "%s/firmware/%s/%s.bin", OTA_BASE_URL,
             identity.codename, manifest.tag);
    if (strcmp(manifest.deviceName, identity.name) != 0 ||
        strcmp(manifest.codename, identity.codename) != 0 ||
        strcmp(manifest.manufacturer, identity.manufacturer) != 0 ||
        strncmp(manifest.tag, "ewp-", 4) != 0 || strcmp(expectedUrl, manifest.firmwareUrl) != 0 ||
        strcmp(manifest.tag, manifest.version) != 0) return false;

    const char* sizeField = strstr(body.data, "\"size\":");
    if (!sizeField) return false;
    char* end = nullptr;
    const unsigned long parsedSize = strtoul(sizeField + 7, &end, 10);
    if (end == sizeField + 7 || parsedSize == 0 || parsedSize > 2 * 1024 * 1024) return false;
    manifest.size = static_cast<size_t>(parsedSize);
    return true;
}

bool partitionMatchesManifest(const esp_partition_t* partition, size_t imageSize,
                              const char* expectedSha256) {
    if (!partition || imageSize > partition->size || strlen(expectedSha256) != 64) return false;
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    if (mbedtls_sha256_starts_ret(&sha, 0) != 0) {
        mbedtls_sha256_free(&sha);
        return false;
    }
    uint8_t bytes[512];
    size_t offset = 0;
    bool ok = true;
    while (offset < imageSize) {
        const size_t length = imageSize - offset < sizeof(bytes) ? imageSize - offset : sizeof(bytes);
        if (esp_partition_read(partition, offset, bytes, length) != ESP_OK ||
            mbedtls_sha256_update_ret(&sha, bytes, length) != 0) {
            ok = false;
            break;
        }
        offset += length;
        vTaskDelay(1);
    }
    uint8_t digest[32];
    if (ok) ok = mbedtls_sha256_finish_ret(&sha, digest) == 0;
    mbedtls_sha256_free(&sha);
    if (!ok) return false;
    static constexpr char HEX_DIGITS[] = "0123456789abcdef";
    char actual[65];
    for (size_t i = 0; i < sizeof(digest); ++i) {
        actual[i * 2] = HEX_DIGITS[digest[i] >> 4];
        actual[i * 2 + 1] = HEX_DIGITS[digest[i] & 0x0f];
    }
    actual[64] = '\0';
    return strcmp(actual, expectedSha256) == 0;
}

bool parseVersion(const char* value, uint32_t& major, uint32_t& minor, uint32_t& patch) {
    if (!value) return false;
    if (strncmp(value, "ewp-", 4) == 0) value += 4;
    char* end = nullptr;
    major = strtoul(value, &end, 10);
    if (end == value || *end++ != '.') return false;
    value = end;
    minor = strtoul(value, &end, 10);
    if (end == value || *end++ != '.') return false;
    value = end;
    patch = strtoul(value, &end, 10);
    return end != value;
}

bool isRemoteNewer(const char* remote, const char* current) {
    uint32_t rMajor, rMinor, rPatch, cMajor, cMinor, cPatch;
    if (!parseVersion(remote, rMajor, rMinor, rPatch) ||
        !parseVersion(current, cMajor, cMinor, cPatch)) return false;
    if (rMajor != cMajor) return rMajor > cMajor;
    if (rMinor != cMinor) return rMinor > cMinor;
    return rPatch > cPatch;
}

const char* imageStateName(esp_ota_img_states_t state) {
    switch (state) {
        case ESP_OTA_IMG_NEW: return "new";
        case ESP_OTA_IMG_PENDING_VERIFY: return "pending verify";
        case ESP_OTA_IMG_VALID: return "valid";
        case ESP_OTA_IMG_INVALID: return "invalid";
        case ESP_OTA_IMG_ABORTED: return "aborted";
        default: return "undefined";
    }
}

const esp_partition_t* findOtherOtaPartition() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!running) return nullptr;
    const esp_partition_subtype_t subtype = running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0
        ? ESP_PARTITION_SUBTYPE_APP_OTA_1 : ESP_PARTITION_SUBTYPE_APP_OTA_0;
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, subtype, nullptr);
}

bool isBootable(const esp_partition_t* partition) {
    if (!partition) return false;
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(partition, &state) == ESP_OK &&
        (state == ESP_OTA_IMG_INVALID || state == ESP_OTA_IMG_ABORTED)) return false;
    // Validate segment extents before the IDF verifier: a failed prior OTA can
    // leave an E9 header with erased 0xFFFFFFFF segment lengths, which makes
    // esp_image_verify() log an alarming parser error on every UI redraw.
    esp_image_header_t header{};
    if (esp_partition_read(partition, 0, &header, sizeof(header)) != ESP_OK ||
        header.magic != ESP_IMAGE_HEADER_MAGIC || header.segment_count == 0 ||
        header.segment_count > ESP_IMAGE_MAX_SEGMENTS) return false;
    size_t offset = sizeof(header);
    for (uint8_t i = 0; i < header.segment_count; ++i) {
        if (offset > partition->size || sizeof(esp_image_segment_header_t) > partition->size - offset)
            return false;
        esp_image_segment_header_t segment{};
        if (esp_partition_read(partition, offset, &segment, sizeof(segment)) != ESP_OK ||
            segment.data_len == 0 || segment.data_len > partition->size - offset - sizeof(segment))
            return false;
        offset += sizeof(segment) + segment.data_len;
    }
    const esp_partition_pos_t position{partition->address, partition->size};
    esp_image_metadata_t metadata{};
    return esp_image_verify(ESP_IMAGE_VERIFY, &position, &metadata) == ESP_OK;
}

bool connectWifi() {
    const auto& config = WatchConfig::get();
    if (!config.wifiSsid[0]) return false;
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true); // Required for ESP32-C3 Wi-Fi/BLE coexistence.
    WiFi.begin(config.wifiSsid, config.wifiPass);
    const uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < WIFI_CONNECT_TIMEOUT_MS)
        vTaskDelay(pdMS_TO_TICKS(200));
    DebugLog::log("OTA: Wi-Fi status=%d heap=%lu largest=%lu", int(WiFi.status()),
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    return WiFi.status() == WL_CONNECTED;
}

void disconnectWifi() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}
} // namespace

OtaService& OtaService::instance() {
    static OtaService service;
    return service;
}

void OtaService::begin() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    DebugLog::log("OTA: booted slot=%s image_state=%s", runningSlot(), runningImageState());
    if (running && esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {
        pendingConfirmation_ = true;
        confirmationStartedMs_ = millis();
        DebugLog::log("OTA: candidate image running; confirmation window=%lu ms",
                      static_cast<unsigned long>(BOOT_CONFIRM_DELAY_MS));
    }
}

void OtaService::tick() {
    if (!pendingConfirmation_ ||
        static_cast<uint32_t>(millis() - confirmationStartedMs_) < BOOT_CONFIRM_DELAY_MS) {
        return;
    }

    const esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
    if (result == ESP_OK) {
        pendingConfirmation_ = false;
        DebugLog::log("OTA: candidate image confirmed after stable runtime");
    } else {
        DebugLog::log("OTA: candidate confirmation failed status=0x%x", unsigned(result));
    }
}

bool OtaService::checkForUpdate() {
    UpdateState expected = updateState_.load();
    if (expected == UpdateState::Checking || expected == UpdateState::Installing) return false;
    updateState_.store(UpdateState::Checking);
    if (xTaskCreate(updateTask, "ota-check", 8192, this, 3, nullptr) != pdPASS) {
        updateState_.store(UpdateState::Failed);
        return false;
    }
    return true;
}

bool OtaService::installUpdate() {
    UpdateState expected = UpdateState::Available;
    if (!updateState_.compare_exchange_strong(expected, UpdateState::Installing)) return false;
    if (xTaskCreate(updateTask, "ota-install", 8192, this, 3, nullptr) != pdPASS) {
        updateState_.store(UpdateState::Available);
        return false;
    }
    return true;
}

void OtaService::resumeAfterCheck() {
    const UpdateState state = updateState_.load();
    if (updateTaskActive_.load() || state == UpdateState::Checking || state == UpdateState::Installing) {
        resumeComponentsRequested_.store(true);
        return;
    }
    auto& board = board::Board::current();
    board.getCompanionSource().resumeFromMaintenance();
    board.getBluetooth().resumeAfterMaintenance();
}

void OtaService::updateTask(void* context) {
    auto* self = static_cast<OtaService*>(context);
    const bool install = self->updateState_.load() == UpdateState::Installing;
    self->updateTaskActive_.store(true);
    self->runUpdate(install);
    self->updateTaskActive_.store(false);
    if (self->resumeComponentsRequested_.exchange(false)) {
        auto& board = board::Board::current();
        board.getCompanionSource().resumeFromMaintenance();
        board.getBluetooth().resumeAfterMaintenance();
    }
    vTaskDelete(nullptr);
}

void OtaService::runUpdate(bool install) {
    auto& bluetooth = board::Board::current().getBluetooth();
    if (!bluetooth.suspendForMaintenance()) {
        updateState_.store(UpdateState::Failed);
        DebugLog::log("OTA: cannot suspend BLE for maintenance");
        return;
    }
    auto& source = board::Board::current().getCompanionSource();
    if (!source.pauseForMaintenance()) {
        source.resumeFromMaintenance();
        bluetooth.resumeAfterMaintenance();
        updateState_.store(UpdateState::Failed);
        DebugLog::log("OTA: companion source could not pause for maintenance");
        return;
    }
    bool keepCommunicationPaused = false;
    struct ResumeComponents {
        hal::IBluetooth& bluetooth;
        hal::ICompanionSource& source;
        bool& keepPaused;
        std::atomic<bool>& resumeRequested;
        ~ResumeComponents() {
            if (!keepPaused || resumeRequested.exchange(false)) {
                source.resumeFromMaintenance();
                bluetooth.resumeAfterMaintenance();
            }
        }
    } resumeComponents{bluetooth, source, keepCommunicationPaused, resumeComponentsRequested_};

    DebugLog::log("OTA: BLE suspended; heap=%u largest=%u",
                  unsigned(ESP.getFreeHeap()),
                  unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    // With the flash-resident certificate bundle, OTA can proceed with a
    // smaller contiguous block than the old PEM-based TLS path required.
    if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < 12 * 1024 ||
        ESP.getFreeHeap() < 45000) {
        updateState_.store(UpdateState::Failed);
        DebugLog::log("OTA: update stopped; insufficient free heap");
        return;
    }
    if (!connectWifi()) {
        updateState_.store(UpdateState::Failed);
        DebugLog::log("OTA: update check failed; Wi-Fi unavailable");
        disconnectWifi();
        return;
    }

    OtaManifest manifest;
    if (!fetchManifest(manifest)) {
        updateState_.store(UpdateState::Failed);
        DebugLog::log("OTA: manifest rejected or unreachable (epoch=%ld)", static_cast<long>(time(nullptr)));
        disconnectWifi();
        return;
    }
    strlcpy(updateVersion_, manifest.version, sizeof(updateVersion_));
    const esp_app_desc_t* current = esp_ota_get_app_description();
    const bool remoteNewer = current && isRemoteNewer(manifest.version, current->version);
    if (!install) {
        DebugLog::log("OTA: manifest current=%s available=%s size=%lu sha256=%s", current ? current->version : "unknown",
                      manifest.version, static_cast<unsigned long>(manifest.size), manifest.sha256);
        disconnectWifi();
        // Keep the radio off between "Update available" and Install. Restarting
        // BLE here fragments the heap again before the next button action.
        keepCommunicationPaused = remoteNewer;
        updateState_.store(remoteNewer ? UpdateState::Available : UpdateState::UpToDate);
        return;
    }
    if (!remoteNewer) {
        disconnectWifi();
        updateState_.store(UpdateState::UpToDate);
        return;
    }

    esp_http_client_config_t httpConfig{};
    httpConfig.url = manifest.firmwareUrl;
    httpConfig.timeout_ms = 20000;
    httpConfig.buffer_size = 1024;
    httpConfig.buffer_size_tx = 512;
    httpConfig.crt_bundle_attach = esp_crt_bundle_attach;
    esp_https_ota_config_t otaConfig{};
    otaConfig.http_config = &httpConfig;
    otaConfig.partial_http_download = true;
    otaConfig.max_http_request_size = 4096;

    esp_https_ota_handle_t handle = nullptr;
    esp_err_t result = esp_https_ota_begin(&otaConfig, &handle);
    esp_app_desc_t remote{};
    if (result == ESP_OK) result = esp_https_ota_get_img_desc(handle, &remote);
    if (result != ESP_OK || strcmp(remote.version, manifest.version) != 0) {
        if (handle) esp_https_ota_abort(handle);
        updateState_.store(UpdateState::Failed);
        DebugLog::log("OTA: image metadata mismatch or download failed status=0x%x", unsigned(result));
        disconnectWifi();
        return;
    }

    do {
        result = esp_https_ota_perform(handle);
        vTaskDelay(1);
    } while (result == ESP_ERR_HTTPS_OTA_IN_PROGRESS);
    const int imageLength = esp_https_ota_get_image_len_read(handle);
    const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
    if (result == ESP_OK && esp_https_ota_is_complete_data_received(handle) &&
        imageLength >= 0 && static_cast<size_t>(imageLength) == manifest.size &&
        partitionMatchesManifest(target, manifest.size, manifest.sha256)) {
        result = esp_https_ota_finish(handle);
    } else {
        DebugLog::log("OTA: downloaded image failed manifest verification bytes=%d expected=%lu",
                      imageLength, static_cast<unsigned long>(manifest.size));
        esp_https_ota_abort(handle);
        if (result == ESP_OK) result = ESP_FAIL;
    }
    if (result == ESP_OK) {
        DebugLog::log("OTA: installed version=%s; rebooting", updateVersion_);
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
    updateState_.store(UpdateState::Failed);
    DebugLog::log("OTA: install failed status=0x%x", unsigned(result));
    disconnectWifi();
}

const char* OtaService::updateMessage() const {
    switch (updateState_.load()) {
        case UpdateState::Checking: return "Checking releases";
        case UpdateState::UpToDate: return "Already up to date";
        case UpdateState::Available: return "Update available";
        case UpdateState::Installing: return "Installing update";
        case UpdateState::Failed: return "Update check failed";
        default: return "Ready to check";
    }
}

const char* OtaService::runningSlot() const {
    const esp_partition_t* partition = esp_ota_get_running_partition();
    if (!partition) return "unknown";
    if (partition->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0) return "ota_0";
    if (partition->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) return "ota_1";
    return "other";
}

const char* OtaService::runningImageState() const {
    const esp_partition_t* partition = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    return partition && esp_ota_get_state_partition(partition, &state) == ESP_OK
               ? imageStateName(state) : "unavailable";
}

const char* OtaService::otherSlot() const {
    const esp_partition_t* partition = findOtherOtaPartition();
    if (!partition) return "none";
    return partition->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 ? "ota_0" : "ota_1";
}

const char* OtaService::otherImageState() const {
    const esp_partition_t* partition = findOtherOtaPartition();
    esp_ota_img_states_t state;
    if (!partition) return "missing";
    if (esp_ota_get_state_partition(partition, &state) != ESP_OK) return "undefined";
    return imageStateName(state);
}

bool OtaService::otherSlotBootable() const {
    return isBootable(findOtherOtaPartition());
}

bool OtaService::selectOtherSlot() {
    const esp_partition_t* partition = findOtherOtaPartition();
    if (!isBootable(partition)) return false;
    const esp_err_t result = esp_ota_set_boot_partition(partition);
    if (result == ESP_OK) {
        DebugLog::log("OTA: selected alternate slot=%s for next boot",
                      partition->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 ? "ota_0" : "ota_1");
        return true;
    }
    DebugLog::log("OTA: alternate slot selection failed status=0x%x", unsigned(result));
    return false;
}

} // namespace services
} // namespace ersa
