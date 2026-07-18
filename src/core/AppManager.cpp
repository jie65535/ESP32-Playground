#include "core/AppManager.h"

AppManager::AppManager(AppContext& context) : context_(context) {}

void AppManager::registerApp(IApp& app) {
    if (appCount_ < MAX_APPS) {
        apps_[appCount_++] = &app;
    }
}

void AppManager::begin(AppId initial) {
    activate(initial);
}

void AppManager::handleCommand(const AppCommand& command) {
    if (appCount_ == 0) {
        return;
    }
    IApp& current = *apps_[currentIndex_];
    if (command.type == AppCommandType::Previous &&
        !current.handlesNavigation()) {
        previousApp();
    } else if (command.type == AppCommandType::Next &&
               !current.handlesNavigation()) {
        nextApp();
    } else {
        current.onCommand(command, context_);
        const AppId requested = current.requestedApp();
        if (requested != AppId::Count) {
            activate(requested);
        }
    }
}

void AppManager::tick(uint32_t nowMs) {
    if (appCount_ > 0) {
        apps_[currentIndex_]->onTick(nowMs, context_);
    }
}

void AppManager::render() {
    if (appCount_ > 0) {
        apps_[currentIndex_]->onRender(context_);
    }
}

void AppManager::nextApp() {
    if (appCount_ == 0) {
        return;
    }
    apps_[currentIndex_]->onExit(context_);
    currentIndex_ = static_cast<uint8_t>((currentIndex_ + 1U) % appCount_);
    apps_[currentIndex_]->onEnter(context_);
}

void AppManager::previousApp() {
    if (appCount_ == 0) {
        return;
    }
    apps_[currentIndex_]->onExit(context_);
    currentIndex_ = currentIndex_ == 0 ? appCount_ - 1U : currentIndex_ - 1U;
    apps_[currentIndex_]->onEnter(context_);
}

bool AppManager::activate(AppId id) {
    for (uint8_t index = 0; index < appCount_; ++index) {
        if (apps_[index]->id() != id) {
            continue;
        }
        if (appCount_ > 0 && apps_[currentIndex_] != apps_[index]) {
            apps_[currentIndex_]->onExit(context_);
        }
        currentIndex_ = index;
        apps_[currentIndex_]->onEnter(context_);
        return true;
    }
    return false;
}

AppId AppManager::currentId() const {
    return appCount_ == 0 ? AppId::Launcher : apps_[currentIndex_]->id();
}

const char* AppManager::currentName() const {
    return appCount_ == 0 ? "none" : apps_[currentIndex_]->name();
}
