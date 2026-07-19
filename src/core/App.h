#pragma once

#include "core/AppTypes.h"

#include <Arduino.h>
#include <lvgl.h>

class DisplayService;
class AudioService;
class TimeService;
class RgbService;
class WifiService;
class ServerService;
class RuntimeMonitorService;
class UiRuntime;

struct AppContext {
    DisplayService& display;
    AudioService& audio;
    TimeService& time;
    RgbService& rgb;
    WifiService& wifi;
    ServerService& server;
    Stream& console;
    UiRuntime& ui;
    RuntimeMonitorService& runtime;
};

class IApp {
public:
    virtual ~IApp() = default;
    virtual AppId id() const = 0;
    virtual const char* name() const = 0;
    virtual void onEnter(AppContext& context) = 0;
    virtual void onExit(AppContext& context) = 0;
    virtual void onCommand(const AppCommand& command, AppContext& context) = 0;
    virtual void onTick(uint32_t nowMs, AppContext& context) = 0;
    virtual lv_obj_t* onCreateView(AppContext& context) = 0;
    virtual void onUpdateView(AppContext& context) = 0;
    virtual AppId requestedApp() const { return AppId::Count; }
};
