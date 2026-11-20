#include "Camera.hpp"

namespace gps {
    //Camera constructor
    Camera::Camera(glm::vec3 cameraPosition, glm::vec3 cameraTarget, glm::vec3 cameraUp) {
        this->cameraPosition = cameraPosition;
        this->cameraTarget = cameraTarget;
        this->cameraUpDirection = cameraUp;

        this->cameraFrontDirection = glm::normalize(cameraTarget - cameraPosition);
        this->cameraRightDirection = glm::normalize(glm::cross(cameraFrontDirection, cameraUpDirection));
    }

    //return the view matrix, using the glm::lookAt() function
    glm::mat4 Camera::getViewMatrix() {
        return glm::lookAt(cameraPosition, cameraTarget, cameraUpDirection);
    }

    //update the camera internal parameters following a camera move event
    void Camera::move(MOVE_DIRECTION direction, float speed) {
        switch (direction) {
        case MOVE_FORWARD:  cameraPosition += cameraFrontDirection * speed; break;
        case MOVE_BACKWARD: cameraPosition -= cameraFrontDirection * speed; break;
        case MOVE_RIGHT:    cameraPosition += cameraRightDirection * speed; break;
        case MOVE_LEFT:     cameraPosition -= cameraRightDirection * speed; break;
        }
        cameraTarget = cameraPosition + cameraFrontDirection;
    }

    //update the camera internal parameters following a camera rotate event
    //yaw - camera rotation around the y axis
    //pitch - camera rotation around the x axis
    void Camera::rotate(float pitch, float yaw) {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

        cameraFrontDirection = glm::normalize(front);
        cameraRightDirection = glm::normalize(glm::cross(cameraFrontDirection, cameraUpDirection));
        cameraTarget = cameraPosition + cameraFrontDirection;
    }

    //place the camera directly (used by the camera tour)
    void Camera::setLookAt(glm::vec3 position, glm::vec3 target) {
        cameraPosition = position;
        cameraTarget = target;
        cameraFrontDirection = glm::normalize(target - position);
        cameraRightDirection = glm::normalize(glm::cross(cameraFrontDirection, cameraUpDirection));
    }

    //current viewing direction (normalized)
    glm::vec3 Camera::getFrontDirection() {
        return cameraFrontDirection;
    }
}