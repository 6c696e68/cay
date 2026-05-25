#pragma once

#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include "CayEngine.h"

class CayimeEngine : public fcitx::InputMethodEngineV2 {
public:
    CayimeEngine(fcitx::Instance* instance);
    ~CayimeEngine() override;

    void keyEvent(const fcitx::InputMethodEntry& entry, fcitx::KeyEvent& keyEvent) override;
    void reset(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override;

private:
    fcitx::Instance* instance_;
    Cay::TelexEngine engine_;

    // Helper to convert fcitx::Key to Cay::KeyEvent
    bool convertKeyEvent(fcitx::KeyEvent& fcitxEvent, Cay::KeyEvent& cayEvent);
};
