#if defined (__APPLE__)
#define GLFW_INCLUDE_GLCOREARB
#define GL_SILENCE_DEPRECATION
#else
#define GLEW_STATIC
#include <GL/glew.h>
#endif

#include <GLFW/glfw3.h>

#include <glm/glm.hpp> //core glm functionality
#include <glm/gtc/matrix_transform.hpp> //glm extension for generating common transformation matrices
#include <glm/gtc/matrix_inverse.hpp> //glm extension for computing inverse matrices
#include <glm/gtc/type_ptr.hpp> //glm extension for accessing the internal data structure of glm types

#include "Window.h"
#include "Shader.hpp"
#include "Camera.hpp"
#include "Model3D.hpp"

#include <iostream>
#include <cmath>

// window
gps::Window myWindow;

// matrices
glm::mat4 view;
glm::mat4 projection;

// light parameters
glm::vec3 lightDir;
glm::vec3 lightColor;

// shader uniform locations
GLint modelLoc;
GLint viewLoc;
GLint projectionLoc;
GLint normalMatrixLoc;
GLint lightDirLoc;
GLint lightColorLoc;

// camera
gps::Camera myCamera(
    glm::vec3(40.0f, 35.0f, 380.0f),
    glm::vec3(40.0f, 35.0f, 0.0f),
    glm::vec3(0.0f, 1.0f, 0.0f));

float cameraSpeed = 30.0f;
float cameraSprintFactor = 4.0f;

// time
double lastFrameTime = 0.0;
float deltaTime = 0.0f;

GLboolean pressedKeys[1024];

// mouse
bool firstMouse = true;
double lastX, lastY;
float yaw = -90.0f, pitch = 0.0f;
float mouseSensitivity = 0.1f;

// models 
gps::Model3D terrain;
gps::Model3D satuMare;
gps::Model3D genex;
gps::Model3D genexRestaurant;
gps::Model3D panorama;
gps::Model3D telefoane;

// animations
float restaurantAngle = 0.0f;
const float RESTAURANT_SPEED = 6.0f; 

// view modes
bool flatShading = false;
GLint flatShadingLoc;

// shaders
gps::Shader myBasicShader;

GLenum glCheckError_(const char* file, int line){
    GLenum errorCode;
    while ((errorCode = glGetError()) != GL_NO_ERROR) {
        std::string error;
        switch (errorCode) {
        case GL_INVALID_ENUM:
            error = "INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            error = "INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            error = "INVALID_OPERATION";
            break;
        case GL_OUT_OF_MEMORY:
            error = "OUT_OF_MEMORY";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            error = "INVALID_FRAMEBUFFER_OPERATION";
            break;
        }
        std::cout << error << " | " << file << " (" << line << ")" << std::endl;
    }
    return errorCode;
}
#define glCheckError() glCheckError_(__FILE__, __LINE__)

void windowResizeCallback(GLFWwindow* window, int width, int height) {
    fprintf(stdout, "Window resized! New width: %d , and height: %d\n", width, height);
    if (width == 0 || height == 0) return; // window minimized

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    myWindow.setWindowDimensions(WindowDimensions{ fbWidth, fbHeight });
    glViewport(0, 0, fbWidth, fbHeight);

    projection = glm::perspective(glm::radians(45.0f), (float)fbWidth / (float)fbHeight, 0.5f, 1500.0f);
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
        if (key == GLFW_KEY_1) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        if (key == GLFW_KEY_2) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        if (key == GLFW_KEY_3) glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        if (key == GLFW_KEY_4) {
            flatShading = !flatShading;
            myBasicShader.useShaderProgram();
            glUniform1i(flatShadingLoc, flatShading);
        }
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
    view = myCamera.getViewMatrix();
    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
}

void processMovement() {
    float distance = cameraSpeed * deltaTime;
    if (pressedKeys[GLFW_KEY_LEFT_SHIFT]) {
        distance *= cameraSprintFactor;
    }

    if (pressedKeys[GLFW_KEY_W]) {
        myCamera.move(gps::MOVE_FORWARD, distance);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    }

    if (pressedKeys[GLFW_KEY_S]) {
        myCamera.move(gps::MOVE_BACKWARD, distance);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    }

    if (pressedKeys[GLFW_KEY_A]) {
        myCamera.move(gps::MOVE_LEFT, distance);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    }

    if (pressedKeys[GLFW_KEY_D]) {
        myCamera.move(gps::MOVE_RIGHT, distance);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    }
}

void updateAnimations() {
    restaurantAngle = fmod(restaurantAngle + RESTAURANT_SPEED * deltaTime, 360.0f);
}

void initOpenGLWindow() {
    myWindow.Create(1280, 720, "Proiect");
}

void setWindowCallbacks() {
    glfwSetWindowSizeCallback(myWindow.getWindow(), windowResizeCallback);
    glfwSetKeyCallback(myWindow.getWindow(), keyboardCallback);
    glfwSetCursorPosCallback(myWindow.getWindow(), mouseCallback);
    glfwSetInputMode(myWindow.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void initOpenGLState() {
    glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
    glViewport(0, 0, myWindow.getWindowDimensions().width, myWindow.getWindowDimensions().height);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_DEPTH_TEST); // enable depth-testing
    glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"
    glEnable(GL_CULL_FACE); // cull face
    glCullFace(GL_BACK); // cull back face
    glFrontFace(GL_CCW); // GL_CCW for counter clock-wise
    glPointSize(2.0f); // vertex size in point (polygonal) mode
}

void initModels() {
    terrain.LoadModel("models/brutalism/terrain.obj");
    satuMare.LoadModel("models/brutalism/satu_mare.obj");
    genex.LoadModel("models/brutalism/genex.obj");
    genexRestaurant.LoadModel("models/brutalism/genex_restaurant.obj");
    panorama.LoadModel("models/brutalism/panorama.obj");
    telefoane.LoadModel("models/brutalism/telefoane.obj");
}

void initShaders() {
    myBasicShader.loadShader(
        "shaders/basic.vert",
        "shaders/basic.frag");
}

void initUniforms() {
    myBasicShader.useShaderProgram();

    modelLoc = glGetUniformLocation(myBasicShader.shaderProgram, "model");

    // get view matrix for current camera
    view = myCamera.getViewMatrix();
    viewLoc = glGetUniformLocation(myBasicShader.shaderProgram, "view");
    // send view matrix to shader
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    normalMatrixLoc = glGetUniformLocation(myBasicShader.shaderProgram, "normalMatrix");

    // create projection matrix
    projection = glm::perspective(glm::radians(45.0f),
        (float)myWindow.getWindowDimensions().width / (float)myWindow.getWindowDimensions().height,
        0.5f, 1500.0f);
    projectionLoc = glGetUniformLocation(myBasicShader.shaderProgram, "projection");
    // send projection matrix to shader
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

    //set the light direction (direction towards the light)
    lightDir = glm::vec3(0.0f, 1.0f, 1.0f);
    lightDirLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightDir");
    // send light dir to shader
    glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDir));

    //set light color
    lightColor = glm::vec3(1.0f, 1.0f, 1.0f); //white light
    lightColorLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightColor");
    // send light color to shader
    glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));

    flatShadingLoc = glGetUniformLocation(myBasicShader.shaderProgram, "flatShading");
    glUniform1i(flatShadingLoc, flatShading);
}

// draws a model with its own model matrix; normalMatrix is computed per object
void renderObject(gps::Model3D& object, const glm::mat4& objectModel, gps::Shader shader) {
    shader.useShaderProgram();
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(objectModel));
    glm::mat3 nm = glm::mat3(glm::inverseTranspose(view * objectModel));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(nm));
    object.Draw(shader);
}

void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // brutalist scene (positions in meters)
    glm::mat4 identity(1.0f);
    renderObject(terrain, identity, myBasicShader);
    renderObject(satuMare, glm::translate(identity, glm::vec3(0.0f, 0.0f, 60.0f)), myBasicShader);
    renderObject(genex, glm::translate(identity, glm::vec3(260.0f, 0.0f, 196.0f)), myBasicShader);
    // restaurant: rotate around its own axis first, then move it on top of the tower (T * R)
    glm::mat4 restaurantModel = glm::translate(identity, glm::vec3(260.0f, 108.0f, 229.0f));
    restaurantModel = glm::rotate(restaurantModel, glm::radians(restaurantAngle), glm::vec3(0.0f, 1.0f, 0.0f));
    renderObject(genexRestaurant, restaurantModel, myBasicShader);
    renderObject(panorama, glm::translate(identity, glm::vec3(-60.0f, 40.0f, -260.0f)), myBasicShader);
    renderObject(telefoane, glm::translate(identity, glm::vec3(-170.0f, 0.0f, 140.0f)), myBasicShader);
}

void cleanup() {
    myWindow.Delete();
    //cleanup code for your own data
}

int main(int argc, const char* argv[]) {

    try {
        initOpenGLWindow();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    initOpenGLState();
    initModels();
    initShaders();
    initUniforms();
    setWindowCallbacks();

    glCheckError();
    // application loop
    lastFrameTime = glfwGetTime();
    while (!glfwWindowShouldClose(myWindow.getWindow())) {
        double now = glfwGetTime();
        deltaTime = (float)(now - lastFrameTime);
        lastFrameTime = now;

        processMovement();
        updateAnimations();
        renderScene();

        glfwPollEvents();
        glfwSwapBuffers(myWindow.getWindow());

        glCheckError();
    }

    cleanup();

    return EXIT_SUCCESS;
}