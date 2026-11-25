#include "Input.hpp"
#include "Scene.hpp"
#include "Lighting.hpp"
#include "Renderer.hpp"
#include "Shadows.hpp"
#include "Rain.hpp"
#include "Collision.hpp"
#include "Traffic.hpp"
#include "Tour.hpp"
#include "UI.hpp"

float cameraSpeed = 30.0f;
float cameraSprintFactor = 4.0f;

// mouse
bool firstMouse = true;
double lastX, lastY;
float yaw = -90.0f, pitch = 0.0f;
float mouseSensitivity = 0.1f;

const float OBJECT_MOVE_SPEED = 20.0f;    // meters per second
const float OBJECT_ROTATE_SPEED = 45.0f;  // degrees per second
const float OBJECT_SCALE_SPEED = 0.5f;    // +50% per second

void windowResizeCallback(GLFWwindow* window, int width, int height) {
    fprintf(stdout, "Window resized! New width: %d , and height: %d\n", width, height);
    if (width == 0 || height == 0) return; // window minimized
    firstMouse = true; // the cursor is re-centered on resize; don't treat that as mouse movement

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    myWindow.setWindowDimensions(WindowDimensions{ fbWidth, fbHeight });
    glViewport(0, 0, fbWidth, fbHeight);
    createSceneFramebuffers(fbWidth, fbHeight);

    projection = glm::perspective(glm::radians(45.0f), (float)fbWidth / (float)fbHeight, 0.5f, 3000.0f);
    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
}

void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) { glfwSetWindowShouldClose(window, GL_TRUE); }

    if (key == GLFW_KEY_M && action == GLFW_PRESS) {
        if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        else
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
    }

    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_1) { viewMode = VIEW_SOLID; applyViewMode(); }
        if (key == GLFW_KEY_2) { viewMode = VIEW_WIREFRAME; applyViewMode(); }
        if (key == GLFW_KEY_3) { viewMode = VIEW_POINTS; applyViewMode(); }
        if (key == GLFW_KEY_4) { flatShading = !flatShading; applyShadingMode(); }
        if (key == GLFW_KEY_H) showUI = !showUI;
        if (key == GLFW_KEY_N) toggleDayNight();
        if (key == GLFW_KEY_L) dayCycleRunning = !dayCycleRunning;
        if (key == GLFW_KEY_F) { fogEnabled = !fogEnabled; applyFog(); }
        if (key == GLFW_KEY_P) setRain(!rainEnabled);
        if (key == GLFW_KEY_K) setSnow(!snowEnabled);
        if (key == GLFW_KEY_C) collisionsEnabled = !collisionsEnabled;
        if (key == GLFW_KEY_V) trafficEnabled = !trafficEnabled;
        if (key == GLFW_KEY_O) shadowsEnabled = !shadowsEnabled;
        if (key == GLFW_KEY_B) {
            antialiasingEnabled = !antialiasingEnabled;
            createSceneFramebuffers(sceneWidth, sceneHeight);
        }
        if (key == GLFW_KEY_T) { if (tourActive) stopTour(); else startTour(); }
    }

    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) {
            pressedKeys[key] = true;
        }
        else if (action == GLFW_RELEASE) {
            pressedKeys[key] = false;
        }
    }
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) return;
    if (tourActive) return; // the tour controls the camera

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
        return;
    }

    float xoffset = (float)(xpos - lastX) * mouseSensitivity;
    float yoffset = (float)(lastY - ypos) * mouseSensitivity;
    lastX = xpos;
    lastY = ypos;

    yaw += xoffset;
    pitch += yoffset;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    myCamera.rotate(pitch, yaw);
    uploadView();
}

void setWindowCallbacks() {
    glfwSetWindowSizeCallback(myWindow.getWindow(), windowResizeCallback);
    glfwSetKeyCallback(myWindow.getWindow(), keyboardCallback);
    glfwSetCursorPosCallback(myWindow.getWindow(), mouseCallback);
    glfwSetInputMode(myWindow.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void processMovement() {
    if (tourActive) return; // the tour controls the camera

    updateColliders(); // the building and the restaurant may have moved since the last frame

    float distance = cameraSpeed * deltaTime;
    if (pressedKeys[GLFW_KEY_LEFT_SHIFT]) {
        distance *= cameraSprintFactor;
    }

    if (pressedKeys[GLFW_KEY_W]) {
        moveCamera(gps::MOVE_FORWARD, distance);
        uploadView();
    }

    if (pressedKeys[GLFW_KEY_S]) {
        moveCamera(gps::MOVE_BACKWARD, distance);
        uploadView();
    }

    if (pressedKeys[GLFW_KEY_A]) {
        moveCamera(gps::MOVE_LEFT, distance);
        uploadView();
    }

    if (pressedKeys[GLFW_KEY_D]) {
        moveCamera(gps::MOVE_RIGHT, distance);
        uploadView();
    }
}

// arrows: move, Q/E: rotate, Z/X: scale, R: reset
void processObjectTransform() {
    if (pressedKeys[GLFW_KEY_UP])    telefoanePos.z -= OBJECT_MOVE_SPEED * deltaTime;
    if (pressedKeys[GLFW_KEY_DOWN])  telefoanePos.z += OBJECT_MOVE_SPEED * deltaTime;
    if (pressedKeys[GLFW_KEY_LEFT])  telefoanePos.x -= OBJECT_MOVE_SPEED * deltaTime;
    if (pressedKeys[GLFW_KEY_RIGHT]) telefoanePos.x += OBJECT_MOVE_SPEED * deltaTime;

    if (pressedKeys[GLFW_KEY_Q]) telefoaneAngle += OBJECT_ROTATE_SPEED * deltaTime;
    if (pressedKeys[GLFW_KEY_E]) telefoaneAngle -= OBJECT_ROTATE_SPEED * deltaTime;

    if (pressedKeys[GLFW_KEY_X]) telefoaneScale *= 1.0f + OBJECT_SCALE_SPEED * deltaTime;
    if (pressedKeys[GLFW_KEY_Z]) telefoaneScale /= 1.0f + OBJECT_SCALE_SPEED * deltaTime;
    telefoaneScale = glm::clamp(telefoaneScale, 0.2f, 3.0f);

    if (pressedKeys[GLFW_KEY_R]) resetTelefoane();
}
