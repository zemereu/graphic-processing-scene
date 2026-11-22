#include "Shadows.hpp"
#include "Lighting.hpp"
#include "Renderer.hpp"

bool shadowsEnabled = true;
GLuint depthMapFBO = 0, depthMapTexture = 0;
gps::Shader depthMapShader;
GLint depthModelLoc, depthLightSpaceLoc;           // in depthMapShader
glm::mat4 lightSpaceTrMatrix;
bool depthPass = false;                  // true while drawing into the shadow map

void initShadowMap() {
    // depth texture: what the sun "sees" (distance to the closest surface for each texel)
    glGenTextures(1, &depthMapTexture);
    glBindTexture(GL_TEXTURE_2D, depthMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // outside the shadow map: depth 1 = nothing in front = lit
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glBindTexture(GL_TEXTURE_2D, 0);

    // framebuffer with only a depth attachment, no color
    glGenFramebuffers(1, &depthMapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMapTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "The shadow map framebuffer is incomplete" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    depthModelLoc = glGetUniformLocation(depthMapShader.shaderProgram, "model");
    depthLightSpaceLoc = glGetUniformLocation(depthMapShader.shaderProgram, "lightSpaceTrMatrix");
    lightSpaceTrMatrixLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightSpaceTrMatrix");
    shadowMapLoc = glGetUniformLocation(myBasicShader.shaderProgram, "shadowMap");
    shadowsEnabledLoc = glGetUniformLocation(myBasicShader.shaderProgram, "shadowsEnabled");
}

// orthographic projection from the sun (parallel rays), over an area centered on the camera.
// The center is snapped to whole shadow-map texels, so the shadows don't shimmer while the camera moves.
glm::mat4 computeLightSpaceTrMatrix() {
    glm::vec3 sunDir = glm::normalize(lightDir);
    glm::mat4 lightView = glm::lookAt(sunDir, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);
    glm::vec3 center = glm::vec3(lightView * glm::vec4(cameraPos.x, 0.0f, cameraPos.z, 1.0f));
    float texel = SHADOW_AREA / SHADOW_MAP_SIZE;
    center.x = floor(center.x / texel) * texel;
    center.y = floor(center.y / texel) * texel;

    float half = SHADOW_AREA / 2.0f;
    glm::mat4 lightProjection = glm::ortho(center.x - half, center.x + half, center.y - half, center.y + half,
                                           -center.z - SHADOW_DEPTH_RANGE, -center.z + SHADOW_DEPTH_RANGE);
    return lightProjection * lightView;
}

// pass 1: the scene from the sun, depth only
void renderShadowMap() {
    if (!shadowsEnabled) return;
    lightSpaceTrMatrix = computeLightSpaceTrMatrix();
    depthMapShader.useShaderProgram();
    glUniformMatrix4fv(depthLightSpaceLoc, 1, GL_FALSE, glm::value_ptr(lightSpaceTrMatrix));

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);  // wireframe/points don't apply to the shadows
    depthPass = true;
    renderSceneObjects();
    depthPass = false;
    applyViewMode();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, sceneWidth, sceneHeight);
}
