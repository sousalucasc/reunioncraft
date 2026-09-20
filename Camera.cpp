#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

//Cima do mundo. Nao gira com a camera, senao ela tomba.
static const glm::vec3 WORLD_UP(0.0f, 1.0f, 0.0f);

Camera::Camera(glm::vec3 startPosition)
    : position(startPosition),
    front(0.0f, 0.0f, -1.0f),
    up(WORLD_UP),
    right(1.0f, 0.0f, 0.0f),
    yaw(-90.0f),
    pitch(0.0f),
    fov(70.0f),
    speed(5.0f),
    sensitivity(0.1f)
{
    updateVectors();
}

void Camera::updateVectors()
{
    glm::vec3 dir;
    dir.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
    dir.y = std::sin(glm::radians(pitch));
    dir.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));

    front = glm::normalize(dir);
    right = glm::normalize(glm::cross(front, WORLD_UP));
    up = glm::normalize(glm::cross(right, front));
}

glm::mat4 Camera::getView() const
{
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::getProjection(float aspect) const
{
    return glm::perspective(glm::radians(fov), aspect, 0.1f, 1000.0f);
}

void Camera::processKeyboard(CameraMovement direction, float deltaTime)
{
    float velocity = speed * deltaTime;

    switch (direction)
    {
    case CameraMovement::FORWARD:  position += front * velocity;    break;
    case CameraMovement::BACKWARD: position -= front * velocity;    break;
    case CameraMovement::LEFT:     position -= right * velocity;    break;
    case CameraMovement::RIGHT:    position += right * velocity;    break;
    case CameraMovement::UP:       position += WORLD_UP * velocity; break;
    case CameraMovement::DOWN:     position -= WORLD_UP * velocity; break;
    }
}

void Camera::processMouse(float xoffset, float yoffset)
{
    yaw += xoffset * sensitivity;
    pitch += yoffset * sensitivity;

    //Trava antes de 90 pra nao inverter o vetor up.
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    updateVectors();
}
