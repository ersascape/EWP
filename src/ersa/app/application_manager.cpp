#include "ersa/app/application_manager.h"
#include <string.h>

namespace ersa {
namespace app {

ApplicationManager::ApplicationManager() {
    for (size_t i = 0; i < MAX_APPS; ++i) {
        apps_[i] = nullptr;
    }
}

ApplicationManager& ApplicationManager::instance() {
    static ApplicationManager s_manager;
    return s_manager;
}

bool ApplicationManager::registerApp(Application* app) {
    if (!app || appCount_ >= MAX_APPS) return false;
    for (size_t i = 0; i < appCount_; ++i) {
        if (apps_[i] == app || strcmp(apps_[i]->getId(), app->getId()) == 0) {
            return false; // Already registered
        }
    }
    apps_[appCount_++] = app;
    app->onCreate();

    if (!activeApp_) {
        activeApp_ = app;
        activeApp_->onStart();
        activeApp_->onResume();
        markDirty(true);
    }
    return true;
}

bool ApplicationManager::switchTo(const char* appId) {
    if (!appId) return false;
    for (size_t i = 0; i < appCount_; ++i) {
        if (strcmp(apps_[i]->getId(), appId) == 0) {
            return switchTo(i);
        }
    }
    return false;
}

bool ApplicationManager::switchTo(size_t index) {
    if (index >= appCount_) return false;
    Application* target = apps_[index];
    if (target == activeApp_) return true;

    if (activeApp_) {
        activeApp_->onPause();
        activeApp_->onStop();
    }
    activeApp_ = target;
    if (activeApp_) {
        activeApp_->onStart();
        activeApp_->onResume();
    }
    appSwitched_ = true;
    markDirty(false);
    return true;
}

Application* ApplicationManager::getActiveApp() const {
    return activeApp_;
}

size_t ApplicationManager::getAppCount() const {
    return appCount_;
}

Application* ApplicationManager::getApp(size_t index) const {
    if (index >= appCount_) return nullptr;
    return apps_[index];
}

bool ApplicationManager::handleEvent(const events::Event& event) {
    if (!activeApp_) return false;
    activeApp_->onEvent(event);
    markDirty(false);
    return true;
}

void ApplicationManager::markDirty(bool fullRefresh) {
    dirty_ = true;
    if (fullRefresh) {
        fullRefreshNeeded_ = true;
    }
}

void ApplicationManager::clearDirty() {
    dirty_ = false;
    fullRefreshNeeded_ = false;
}

bool ApplicationManager::isDirty() const {
    return dirty_;
}

bool ApplicationManager::isFullRefreshNeeded() const {
    return fullRefreshNeeded_;
}

bool ApplicationManager::isAppSwitched() const {
    return appSwitched_;
}

void ApplicationManager::clearAppSwitched() {
    appSwitched_ = false;
}

void ApplicationManager::render(hal::IDisplay& display) {
    if (!dirty_ || !activeApp_) return;

    const bool full = fullRefreshNeeded_ || appSwitched_;
    if (full) {
        activeApp_->render(display, true);
        display.refresh(fullRefreshNeeded_);
        appSwitched_ = false;
    } else {
        const Rect bounds = activeApp_->getPartialBounds();
        if (bounds.w >= display.width() && bounds.h >= display.height()) {
            activeApp_->render(display, false);
            display.refresh(false);
        } else {
            activeApp_->render(display, false);
            display.refreshRect(bounds);
        }
    }

    dirty_ = false;
    fullRefreshNeeded_ = false;
}

void ApplicationManager::tick() {
    if (activeApp_) {
        activeApp_->tick();
    }
}

} // namespace app
} // namespace ersa
