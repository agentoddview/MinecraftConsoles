#pragma once

#include <emscripten/html5.h>

namespace web {

class WebInput {
public:
    bool Initialize();
    void BeginFrame();

    bool MoveForward() const;
    bool MoveBackward() const;
    bool StrafeLeft() const;
    bool StrafeRight() const;
    bool MoveUp() const;
    bool MoveDown() const;

    void ConsumeLookDelta(double& deltaX, double& deltaY);
    bool ConsumePauseToggle();
    bool ConsumeInventoryToggle();
    bool ConsumeChatToggle();
    bool ConsumeFullscreenToggle();
    bool ConsumeHudToggle();

private:
    static EM_BOOL OnKeyDown(int eventType, const EmscriptenKeyboardEvent* event, void* userData);
    static EM_BOOL OnKeyUp(int eventType, const EmscriptenKeyboardEvent* event, void* userData);
    static EM_BOOL OnMouseMove(int eventType, const EmscriptenMouseEvent* event, void* userData);

    bool forward_ = false;
    bool backward_ = false;
    bool left_ = false;
    bool right_ = false;
    bool up_ = false;
    bool down_ = false;

    bool pauseToggleRequested_ = false;
    bool inventoryToggleRequested_ = false;
    bool chatToggleRequested_ = false;
    bool fullscreenToggleRequested_ = false;
    bool hudToggleRequested_ = false;

    double lookDeltaX_ = 0.0;
    double lookDeltaY_ = 0.0;
};

} // namespace web
