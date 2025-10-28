#include "Camera.h"

Camera::Camera()
{
    position = Vector3(0.0f, 0.0f, -5.0f);
    rotation = Quaternion::Identity;
}

void Camera::SetPosition(float x, float y, float z)
{
    position = Vector3(x, y, z);
}

void Camera::SetRotation(float pitch, float yaw, float roll)
{
    rotation = Quaternion::CreateFromYawPitchRoll(
        yaw,
        pitch,
        roll);
}

void Camera::SetRotationQuaternion(float x, float y, float z, float w)
{
    rotation = Quaternion(x, y, z, w);
}

Vector3 Camera::GetPosition() const
{
    return position;
}

Quaternion Camera::GetRotation() const
{
    return rotation;
}

Matrix Camera::GetViewMatrix() const
{
    Vector3 forward = Vector3::Transform(Vector3::Forward, rotation);
    Vector3 target = position + forward;
    return Matrix::CreateLookAt(position, target, Vector3::Up);
}
