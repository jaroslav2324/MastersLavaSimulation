#pragma once

#include <Windows.h>
#include <SimpleMath.h>
#include "Camera.h"

using namespace DirectX::SimpleMath;

class InputHandler
{
public:
    InputHandler();

    void OnKeyDown(WPARAM key);
    void OnKeyUp(WPARAM key);
    void OnMouseMove(int x, int y);
    void OnMouseDown(bool right, int x, int y);
    void OnMouseUp(bool right);

    // Update camera based on current input state
    void ProcessInput(float deltaSeconds, Camera &camera);

private:
    bool m_keys[256] = {};
    bool m_rightMouseDown = false;
    POINT m_lastMousePos{};
    float m_moveSpeed = 5.0f;
    float m_mouseSensitivity = 0.0025f;

    // keep yaw/pitch here and apply to Camera as quaternion
    float m_camYaw = 0.0f;
    float m_camPitch = 0.0f;
};
