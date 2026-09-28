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
    if (!activeApp_) {
        activeApp_ = app;
        activeApp_->onEnter();
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
        activeApp_->onExit();
    }
    activeApp_ = target;
    if (activeApp_) {
        activeApp_->onEnter();
    }
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
    const bool handled = activeApp_->onEvent(event);
    if (handled) {
        markDirty(false);
    }
    return handled;
}

void ApplicationManager::markDirty(bool fullRefresh) {
    dirty_ = true;
    if (fullRefresh) {
        fullRefreshNeeded_ = true;
    }
}

bool ApplicationManager::isDirty() const {
    return dirty_;
}

bool ApplicationManager::isFullRefreshNeeded() const {
    return fullRefreshNeeded_;
}

void ApplicationManager::render(hal::IDisplay& display) {
    if (!dirty_ || !activeApp_) return;

    activeApp_->render(display, fullRefreshNeeded_);
    display.refresh(fullRefreshNeeded_);

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
