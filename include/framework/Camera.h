#pragma once
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

class Camera
{
private:
    Vector3 position;
    Quaternion rotation;

public:
    Camera();

    // Setters
    void SetPosition(float x, float y, float z);
    void SetRotation(float pitch, float yaw, float roll);
    void SetRotationQuaternion(float x, float y, float z, float w);

    // Getters
    Vector3 GetPosition() const;
    Quaternion GetRotation() const;

    Matrix GetViewMatrix() const;
};
