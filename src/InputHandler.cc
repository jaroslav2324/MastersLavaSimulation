#include "pch.h"
#include "framework/InputHandler.h"

#include <Windows.h>

#include "framework/SimulationSystem.h"

InputHandler::InputHandler()
{
}

void InputHandler::OnKeyDown(WPARAM key)
{
    // std::cout << (int)key << std::endl;
    if (key < _countof(m_keys))
        m_keys[key] = true;
}

void InputHandler::OnKeyUp(WPARAM key)
{
    if (key < _countof(m_keys))
        m_keys[key] = false;
}

void InputHandler::OnMouseMove(int x, int y)
{
    if (!m_rightMouseDown)
        return;

    int dx = x - m_lastMousePos.x;
    int dy = y - m_lastMousePos.y;
    m_lastMousePos.x = x;
    m_lastMousePos.y = y;

    m_camYaw += dx * m_mouseSensitivity;
    m_camPitch += dy * m_mouseSensitivity;

    const float limit = DirectX::XM_PIDIV2 - 0.01f;
    if (m_camPitch > limit)
        m_camPitch = limit;
    if (m_camPitch < -limit)
        m_camPitch = -limit;
}

void InputHandler::OnMouseDown(bool right, int x, int y)
{
    if (right)
    {
        m_rightMouseDown = true;
        m_lastMousePos.x = x;
        m_lastMousePos.y = y;
    }
}

void InputHandler::OnMouseUp(bool right)
{
    if (right)
        m_rightMouseDown = false;
}

void InputHandler::ProcessInput(float deltaSeconds, Camera &camera)
{
    if (deltaSeconds <= 0.0f)
        return;

    // Build rotation matrix from yaw/pitch
    Matrix rot = Matrix::CreateFromYawPitchRoll(m_camYaw, m_camPitch, 0.0f);
    Vector3 forward = Vector3::Transform(Vector3(0, 0, 1), rot);
    Vector3 right = Vector3::Transform(Vector3(1, 0, 0), rot);
    Vector3 up = Vector3::Transform(Vector3(0, 1, 0), rot);

    Vector3 moveDelta = Vector3::Zero;

    if (m_keys['W'])
        moveDelta -= forward;
    if (m_keys['S'])
        moveDelta += forward;
    if (m_keys['A'])
        moveDelta -= right;
    if (m_keys['D'])
        moveDelta += right;
    if (m_keys['E'])
        moveDelta += up;
    if (m_keys['Q'])
        moveDelta -= up;

    static float timeout;
    static bool runSimulation;
    timeout += deltaSeconds;

    if (m_keys[VK_SPACE])
    {
        if (timeout > 0.5f)
        {
            SimulationSystem::SetSimulationRunning(runSimulation);
            runSimulation = !runSimulation;
            timeout = 0.0f;
        }
    }

    Vector3 camPos = camera.GetPosition();
    if (moveDelta.Length() > 0.0f)
    {
        moveDelta.Normalize();
        camPos += moveDelta * (m_moveSpeed * deltaSeconds);
        camera.SetPosition(camPos.x, camPos.y, camPos.z);
    }

    // Apply rotation to camera (Camera::SetRotation takes pitch,yaw,roll but uses CreateFromYawPitchRoll(yaw,pitch,roll) internally)
    camera.SetRotation(m_camPitch, m_camYaw, 0.0f);
}
