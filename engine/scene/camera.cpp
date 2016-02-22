#include "camera.h"

#include "../types/bounding_box.h"

#include <glm/detail/func_common.hpp>
#include <glm/gtc/matrix_transform.hpp>

Camera::~Camera()
{
}

Camera::Camera() : clipPlaneFar(10000.0f), clipPlaneNear(0.3f),
    horizontalFoV(60.0f), aspectRatio(16.0f / 9.0f)
{
    name = "Default Camera";
    projectionChanged = frustumChanged = false;
    ComputeViewMatrix();
    ComputeProjectionMatrix();
    frustum.ExtractPlanes(projectionMatrix * viewMatrix);
}

float Camera::ClipPlaneFar() const
{
    return clipPlaneFar;
}

void Camera::ClipPlaneFar(float val)
{
    projectionChanged = true;
    clipPlaneFar = glm::max(val, 0.01f);
}

float Camera::ClipPlaneNear() const
{
    return clipPlaneNear;
}

void Camera::ClipPlaneNear(float val)
{
    projectionChanged = true;
    clipPlaneNear = glm::max(val, 0.01f);
}

float Camera::HorizontalFoV() const
{
    return horizontalFoV;
}

void Camera::HorizontalFoV(float val)
{
    projectionChanged = true;;
    horizontalFoV = glm::clamp(val, 1.0f, 179.0f);
}

float Camera::AspectRatio() const
{
    return aspectRatio;
}

void Camera::AspectRatio(float val)
{
    projectionChanged = true;
    aspectRatio = val;
}

const glm::mat4x4 &Camera::ViewMatrix()
{
    if (transform.changed)
    {
        ComputeViewMatrix();
        frustumChanged = true;
        transform.changed = false;
    }

    return viewMatrix;
}

const glm::mat4x4 &Camera::ProjectionMatrix()
{
    if (projectionChanged)
    {
        ComputeProjectionMatrix();
        frustumChanged = true;
        projectionChanged = false;
    }

    return projectionMatrix;
}

bool Camera::ParametersChanged() const
{
    if (changedActive)
    {
        changedActive = false;
        return true;
    }

    return projectionChanged || transform.changed || frustumChanged;
}

bool Camera::InFrustum(const BoundingBox &volume)
{
    if (frustumChanged || projectionChanged || transform.changed)
    {
        frustum.ExtractPlanes(ProjectionMatrix() * ViewMatrix());
        frustumChanged = false;
    }

    return frustum.InFrustum(volume);
}

void Camera::ComputeViewMatrix()
{
    viewMatrix = lookAt
                 (
                     transform.Position(),
                     transform.Position() +
                     transform.Forward(),
                     transform.Up()
                 );
}

void Camera::ComputeProjectionMatrix()
{
    projectionMatrix = glm::perspective
                       (
                           glm::radians(horizontalFoV),
                           aspectRatio,
                           clipPlaneNear, clipPlaneFar
                       );
}