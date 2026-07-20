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
    // Directional input belongs to the foreground app.  The launcher uses it
    // to move its selection; an app may use it for its own lists or ignore it.
    // Never interpret a direction as an application switch while an app is open.
    current.onCommand(command, context_);
    const AppId requested = current.requestedApp();
    if (requested != AppId::Count) {
        push(requested);
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

bool AppManager::activate(AppId id, UiPageTransition transition) {
    historyDepth_ = 0;
    return switchTo(id, transition);
}

bool AppManager::back() {
    if (historyDepth_ == 0) {
        return activate(AppId::Launcher, UiPageTransition::Backward);
    }
    const AppId previous = history_[--historyDepth_];
    return switchTo(previous, UiPageTransition::Backward);
}

bool AppManager::home() {
    historyDepth_ = 0;
    return switchTo(AppId::Launcher, UiPageTransition::Backward);
}

bool AppManager::switchTo(AppId id, UiPageTransition transition) {
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
        context_.ui.replacePage(page, transition);
        active_ = true;
        apps_[currentIndex_]->onUpdateView(context_);
        return true;
    }
    return false;
}

bool AppManager::push(AppId id) {
    if (!active_ || id == currentId()) {
        return switchTo(id, UiPageTransition::Forward);
    }
    if (historyDepth_ >= MAX_HISTORY) {
        for (uint8_t index = 1; index < MAX_HISTORY; ++index) {
            history_[index - 1U] = history_[index];
        }
        historyDepth_ = MAX_HISTORY - 1U;
    }
    history_[historyDepth_++] = currentId();
    if (switchTo(id, UiPageTransition::Forward)) {
        return true;
    }
    --historyDepth_;
    return false;
}

AppId AppManager::currentId() const {
    return appCount_ == 0 || !active_ ? AppId::Launcher
                                      : apps_[currentIndex_]->id();
}

const char* AppManager::currentName() const {
    return appCount_ == 0 || !active_ ? "none" : apps_[currentIndex_]->name();
}
