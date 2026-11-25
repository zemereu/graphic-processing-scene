#include "App.hpp"

// window
gps::Window myWindow;

// matrices
glm::mat4 view;
glm::mat4 projection;

// camera
gps::Camera myCamera(
    glm::vec3(40.0f, 35.0f, 380.0f),
    glm::vec3(40.0f, 35.0f, 0.0f),
    glm::vec3(0.0f, 1.0f, 0.0f));

// time
double lastFrameTime = 0.0;
float deltaTime = 0.0f;

GLboolean pressedKeys[1024];

// shaders
gps::Shader myBasicShader;

// shader uniform locations
GLint modelLoc;
GLint viewLoc;
GLint projectionLoc;
GLint normalMatrixLoc;
GLint lightDirLoc;
GLint lightColorLoc;
GLint numPointLightsLoc;
GLint pointLightPosLoc;
GLint pointLightColorLoc;
GLint emissiveColorLoc;
GLint paintColorLoc;
GLint surfaceModeLoc, timeLoc;  // terrain / water shading modes (basic.frag), time for the waves
GLint numSpotLightsLoc;
GLint spotLightPosLoc;
GLint spotLightDirLoc;
GLint spotLightColorLoc;
GLint spotCutOffLoc;
GLint spotOuterCutOffLoc;
GLint flatShadingLoc;
GLint fogDensityLoc;
GLint lightSpaceTrMatrixLoc, shadowMapLoc, shadowsEnabledLoc; // in myBasicShader

GLenum glCheckError_(const char* file, int line) {
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

void uploadView() {
    view = myCamera.getViewMatrix();
    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
}
