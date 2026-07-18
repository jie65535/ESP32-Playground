#include "core/AppManager.h"

#include "ui/UiRuntime.h"

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
    if (appCount_ == 0 || !active_) {
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
    if (appCount_ > 0 && active_) {
        apps_[currentIndex_]->onTick(nowMs, context_);
    }
}

void AppManager::render() {
    if (appCount_ > 0 && active_) {
        apps_[currentIndex_]->onUpdateView(context_);
    }
}

void AppManager::nextApp() {
    if (appCount_ == 0) {
        return;
    }
    const uint8_t next = static_cast<uint8_t>((currentIndex_ + 1U) % appCount_);
    activate(apps_[next]->id());
}

void AppManager::previousApp() {
    if (appCount_ == 0) {
        return;
    }
    const uint8_t previous = currentIndex_ == 0 ? appCount_ - 1U
                                                : currentIndex_ - 1U;
    activate(apps_[previous]->id());
}

bool AppManager::activate(AppId id) {
    for (uint8_t index = 0; index < appCount_; ++index) {
        if (apps_[index]->id() != id) {
            continue;
        }
        if (active_ && apps_[currentIndex_] == apps_[index]) {
            return true;
        }
        if (active_) {
            apps_[currentIndex_]->onExit(context_);
        }
        currentIndex_ = index;
        apps_[currentIndex_]->onEnter(context_);
        lv_obj_t* page = apps_[currentIndex_]->onCreateView(context_);
        context_.ui.replacePage(page);
        active_ = true;
        apps_[currentIndex_]->onUpdateView(context_);
        return true;
    }
    return false;
}

AppId AppManager::currentId() const {
    return appCount_ == 0 || !active_ ? AppId::Launcher
                                      : apps_[currentIndex_]->id();
}

const char* AppManager::currentName() const {
    return appCount_ == 0 || !active_ ? "none" : apps_[currentIndex_]->name();
}
