#pragma once
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

struct Transform
{
    Vector3 position;
    Quaternion rotation;
    Vector3 scale;

    Transform() : position(0.0f, 0.0f, 0.0f),
                  rotation(0.0f, 0.0f, 0.0f, 1.0f), // Identity quaternion
                  scale(1.0f, 1.0f, 1.0f)
    {
    }

    void SetRotation(float pitch, float yaw, float roll)
    {
        rotation = Quaternion::CreateFromYawPitchRoll(
            yaw,
            pitch,
            roll);
    }

    void SetRotationQuaternion(float x, float y, float z, float w)
    {
        rotation = Quaternion(x, y, z, w);
    }

    Matrix GetModelMatrix() const
    {
        return Matrix::CreateScale(scale) *
               Matrix::CreateFromQuaternion(rotation) *
               Matrix::CreateTranslation(position);
    }
};
