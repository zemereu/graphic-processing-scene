#include "Reflection.hpp"
#include "Renderer.hpp"   // renderScene, scene size
#include "Skybox.hpp"

bool reflectionsEnabled = true;
bool reflectionPass = false;
GLuint reflectionFBO = 0, reflectionTexture = 0, reflectionDepthRBO = 0;
int reflectionWidth = 0, reflectionHeight = 0;
GLint clipPlaneLoc, reflectionTextureLoc, reflectionsEnabledLoc, screenSizeLoc;   // in myBasicShader

void initReflection() {
    myBasicShader.useShaderProgram();
    clipPlaneLoc = glGetUniformLocation(myBasicShader.shaderProgram, "clipPlane");
    reflectionTextureLoc = glGetUniformLocation(myBasicShader.shaderProgram, "reflectionTexture");
    reflectionsEnabledLoc = glGetUniformLocation(myBasicShader.shaderProgram, "reflectionsEnabled");
    screenSizeLoc = glGetUniformLocation(myBasicShader.shaderProgram, "screenSize");
    glUniform4f(clipPlaneLoc, 0.0f, 0.0f, 0.0f, 1.0f);   // nothing clipped (only used in the reflection pass anyway)
}

void createReflectionBuffers(int width, int height) {
    reflectionWidth = glm::max(width / 2, 1);
    reflectionHeight = glm::max(height / 2, 1);
    glGenTextures(1, &reflectionTexture);
    glBindTexture(GL_TEXTURE_2D, reflectionTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, reflectionWidth, reflectionHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glGenRenderbuffers(1, &reflectionDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, reflectionDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, reflectionWidth, reflectionHeight);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glGenFramebuffers(1, &reflectionFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, reflectionFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, reflectionTexture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, reflectionDepthRBO);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "The reflection framebuffer is incomplete" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void deleteReflectionBuffers() {
    if (reflectionFBO) glDeleteFramebuffers(1, &reflectionFBO);
    if (reflectionTexture) glDeleteTextures(1, &reflectionTexture);
    if (reflectionDepthRBO) glDeleteRenderbuffers(1, &reflectionDepthRBO);
    reflectionFBO = reflectionTexture = reflectionDepthRBO = 0;
}

void renderReflection() {
    if (!reflectionsEnabled) return;
    // mirror across the water plane: y -> 2 * WATER_LEVEL - y (the camera goes under the water, looking up)
    glm::mat4 mirror = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, WATER_LEVEL, 0.0f))
        * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f))
        * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -WATER_LEVEL, 0.0f));
    glm::mat4 cameraView = view;
    view = cameraView * mirror;
    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    // keep only what is above the water (a little margin hides the seam at the shore)
    glUniform4f(clipPlaneLoc, 0.0f, 1.0f, 0.0f, -(WATER_LEVEL - 0.05f));
    glEnable(GL_CLIP_DISTANCE0);
    glFrontFace(GL_CW);   // a mirror flips the order of the vertices: the front faces are now clockwise

    glBindFramebuffer(GL_FRAMEBUFFER, reflectionFBO);
    glViewport(0, 0, reflectionWidth, reflectionHeight);
    reflectionPass = true;
    renderScene();
    renderSkybox();
    reflectionPass = false;

    glFrontFace(GL_CCW);
    glDisable(GL_CLIP_DISTANCE0);
    view = cameraView;
    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glViewport(0, 0, sceneWidth, sceneHeight);
}

void bindReflection() {
    myBasicShader.useShaderProgram();
    glUniform1i(reflectionsEnabledLoc, reflectionsEnabled);
    glUniform2f(screenSizeLoc, (float)sceneWidth, (float)sceneHeight);
    glActiveTexture(GL_TEXTURE0 + REFLECTION_TEXTURE_UNIT);
    glBindTexture(GL_TEXTURE_2D, reflectionTexture);
    glUniform1i(reflectionTextureLoc, REFLECTION_TEXTURE_UNIT);
    glActiveTexture(GL_TEXTURE0);
}
