#include "WebInput.h"

namespace {

bool SetDigitalAction(bool isDown, bool& action)
{
    action = isDown;
    return true;
}

} // namespace

namespace web {

bool WebInput::Initialize()
{
    const EMSCRIPTEN_RESULT keyDownResult =
        emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, false, OnKeyDown);
    const EMSCRIPTEN_RESULT keyUpResult =
        emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, false, OnKeyUp);
    const EMSCRIPTEN_RESULT mouseMoveResult =
        emscripten_set_mousemove_callback("#game-canvas", this, false, OnMouseMove);

    return keyDownResult == EMSCRIPTEN_RESULT_SUCCESS &&
           keyUpResult == EMSCRIPTEN_RESULT_SUCCESS &&
           mouseMoveResult == EMSCRIPTEN_RESULT_SUCCESS;
}

void WebInput::BeginFrame()
{
}

bool WebInput::MoveForward() const
{
    return forward_;
}

bool WebInput::MoveBackward() const
{
    return backward_;
}

bool WebInput::StrafeLeft() const
{
    return left_;
}

bool WebInput::StrafeRight() const
{
    return right_;
}

bool WebInput::MoveUp() const
{
    return up_;
}

bool WebInput::MoveDown() const
{
    return down_;
}

void WebInput::ConsumeLookDelta(double& deltaX, double& deltaY)
{
    deltaX = lookDeltaX_;
    deltaY = lookDeltaY_;
    lookDeltaX_ = 0.0;
    lookDeltaY_ = 0.0;
}

bool WebInput::ConsumePauseToggle()
{
    const bool requested = pauseToggleRequested_;
    pauseToggleRequested_ = false;
    return requested;
}

bool WebInput::ConsumeInventoryToggle()
{
    const bool requested = inventoryToggleRequested_;
    inventoryToggleRequested_ = false;
    return requested;
}

bool WebInput::ConsumeChatToggle()
{
    const bool requested = chatToggleRequested_;
    chatToggleRequested_ = false;
    return requested;
}

bool WebInput::ConsumeFullscreenToggle()
{
    const bool requested = fullscreenToggleRequested_;
    fullscreenToggleRequested_ = false;
    return requested;
}

bool WebInput::ConsumeHudToggle()
{
    const bool requested = hudToggleRequested_;
    hudToggleRequested_ = false;
    return requested;
}

EM_BOOL WebInput::OnKeyDown(int, const EmscriptenKeyboardEvent* event, void* userData)
{
    WebInput* input = static_cast<WebInput*>(userData);
    if (input == nullptr) {
        return EM_FALSE;
    }

    switch (event->keyCode) {
    case 87:
        return SetDigitalAction(true, input->forward_) ? EM_TRUE : EM_FALSE;
    case 83:
        return SetDigitalAction(true, input->backward_) ? EM_TRUE : EM_FALSE;
    case 65:
        return SetDigitalAction(true, input->left_) ? EM_TRUE : EM_FALSE;
    case 68:
        return SetDigitalAction(true, input->right_) ? EM_TRUE : EM_FALSE;
    case 32:
        return SetDigitalAction(true, input->up_) ? EM_TRUE : EM_FALSE;
    case 16:
        return SetDigitalAction(true, input->down_) ? EM_TRUE : EM_FALSE;
    case 27:
        input->pauseToggleRequested_ = true;
        return EM_TRUE;
    case 69:
        input->inventoryToggleRequested_ = true;
        return EM_TRUE;
    case 84:
        input->chatToggleRequested_ = true;
        return EM_TRUE;
    case 112:
        input->hudToggleRequested_ = true;
        return EM_TRUE;
    case 122:
        input->fullscreenToggleRequested_ = true;
        return EM_TRUE;
    default:
        return EM_FALSE;
    }
}

EM_BOOL WebInput::OnKeyUp(int, const EmscriptenKeyboardEvent* event, void* userData)
{
    WebInput* input = static_cast<WebInput*>(userData);
    if (input == nullptr) {
        return EM_FALSE;
    }

    switch (event->keyCode) {
    case 87:
        return SetDigitalAction(false, input->forward_) ? EM_TRUE : EM_FALSE;
    case 83:
        return SetDigitalAction(false, input->backward_) ? EM_TRUE : EM_FALSE;
    case 65:
        return SetDigitalAction(false, input->left_) ? EM_TRUE : EM_FALSE;
    case 68:
        return SetDigitalAction(false, input->right_) ? EM_TRUE : EM_FALSE;
    case 32:
        return SetDigitalAction(false, input->up_) ? EM_TRUE : EM_FALSE;
    case 16:
        return SetDigitalAction(false, input->down_) ? EM_TRUE : EM_FALSE;
    default:
        return EM_FALSE;
    }
}

EM_BOOL WebInput::OnMouseMove(int, const EmscriptenMouseEvent* event, void* userData)
{
    WebInput* input = static_cast<WebInput*>(userData);
    if (input == nullptr) {
        return EM_FALSE;
    }

    input->lookDeltaX_ += static_cast<double>(event->movementX);
    input->lookDeltaY_ += static_cast<double>(event->movementY);
    return EM_TRUE;
}

} // namespace web
