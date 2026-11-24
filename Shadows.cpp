#include "Shadows.hpp"
#include "Lighting.hpp"   // floodlights
#include "Renderer.hpp"

#include <cfloat>

bool shadowsEnabled = true;
bool showShadowMap = false;
GLuint depthMapFBO = 0, depthMapTexture = 0;
gps::Shader depthMapShader;
GLint depthModelLoc, depthLightSpaceLoc;           // in depthMapShader
glm::mat4 lightSpaceTrMatrix;
bool depthPass = false;                  // true while drawing into a shadow map
glm::mat4 depthPassMatrix;
bool spotShadowPass = false;
GLuint spotShadowFBO = 0, spotShadowTexture = 0;
glm::mat4 spotLightSpaceMatrices[4];
GLint spotLightSpaceLoc, spotShadowMapsLoc, spotShadowNearFarLoc, numSpotShadowsLoc;   // in myBasicShader

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
    // the depth is only in the red channel: copy it to green/blue so the UI preview shows it in grayscale
    // (basic.frag reads .r, so the shadows are not affected)
    GLint swizzle[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
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

    // floodlights: a depth texture array, one layer per spot light; the framebuffer gets one layer at a time
    glGenTextures(1, &spotShadowTexture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, spotShadowTexture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, SPOT_SHADOW_MAP_SIZE, SPOT_SHADOW_MAP_SIZE, 4, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glGenFramebuffers(1, &spotShadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, spotShadowFBO);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, spotShadowTexture, 0, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "The floodlight shadow framebuffer is incomplete" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    spotLightSpaceLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotLightSpace");
    spotShadowMapsLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotShadowMaps");
    spotShadowNearFarLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotShadowNearFar");
    numSpotShadowsLoc = glGetUniformLocation(myBasicShader.shaderProgram, "numSpotShadows");

    depthModelLoc = glGetUniformLocation(depthMapShader.shaderProgram, "model");
    depthLightSpaceLoc = glGetUniformLocation(depthMapShader.shaderProgram, "lightSpaceTrMatrix");
    lightSpaceTrMatrixLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightSpaceTrMatrix");
    shadowMapLoc = glGetUniformLocation(myBasicShader.shaderProgram, "shadowMap");
    shadowsEnabledLoc = glGetUniformLocation(myBasicShader.shaderProgram, "shadowsEnabled");
}

// orthographic projection from the light (parallel rays), fitted around the box of the scene near the camera:
// the corners of the box are taken into the light's view and the projection is their bounding box. Like this the
// shadow map covers the same ground at any height of the sun (at sunset the light grazes the ground and a fixed
// square would stretch over kilometers).
glm::mat4 computeLightSpaceTrMatrix() {
    glm::vec3 toLight = glm::normalize(lightDir);
    glm::vec3 up = fabs(toLight.y) > 0.99f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 lightView = glm::lookAt(toLight, glm::vec3(0.0f), up);

    glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);
    float half = SHADOW_AREA / 2.0f;
    glm::vec3 lo(FLT_MAX), hi(-FLT_MAX);
    for (int i = 0; i < 8; i++) {
        glm::vec3 corner(cameraPos.x + ((i & 1) ? half : -half),
                         (i & 2) ? SHADOW_MAX_Y : SHADOW_MIN_Y,
                         cameraPos.z + ((i & 4) ? half : -half));
        glm::vec3 p = glm::vec3(lightView * glm::vec4(corner, 1.0f));
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    // snap to whole shadow-map texels: the size of the box only depends on the light's direction, so while the camera
    // moves the texels stay on the same spots of the ground and the shadow edges don't shimmer
    glm::vec2 size = glm::vec2(hi - lo);
    glm::vec2 texel = size / (float)SHADOW_MAP_SIZE;
    lo.x = floor(lo.x / texel.x) * texel.x;
    lo.y = floor(lo.y / texel.y) * texel.y;
    hi.x = lo.x + size.x;
    hi.y = lo.y + size.y;

    // the light looks down -z: near = the side towards the light (extended for the casters outside the box)
    glm::mat4 lightProjection = glm::ortho(lo.x, hi.x, lo.y, hi.y, -hi.z - SHADOW_CASTER_RANGE, -lo.z);
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
    depthPassMatrix = lightSpaceTrMatrix;
    renderSceneObjects();
    depthPass = false;
    applyViewMode();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, sceneWidth, sceneHeight);
}

// pass 1b: each floodlight sees the scene in its cone (perspective: a point light, the rays spread), depth only
void renderSpotShadowMaps() {
    if (!shadowsEnabled || !floodlightsOn) return;
    depthMapShader.useShaderProgram();
    glBindFramebuffer(GL_FRAMEBUFFER, spotShadowFBO);
    glViewport(0, 0, SPOT_SHADOW_MAP_SIZE, SPOT_SHADOW_MAP_SIZE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    depthPass = true;
    spotShadowPass = true;
    for (size_t i = 0; i < SPOT_LIGHTS.size() && i < 4; i++) {
        const SpotLight& spot = SPOT_LIGHTS[i];
        glm::vec3 dir = glm::normalize(spot.target - spot.position);
        glm::vec3 up = fabs(dir.y) > 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        // the frustum just covers the outer cone (+ a little margin)
        glm::mat4 projection = glm::perspective(glm::radians(2.0f * SPOT_OUTER_ANGLE + 4.0f), 1.0f,
                                                SPOT_SHADOW_NEAR, SPOT_SHADOW_FAR);
        spotLightSpaceMatrices[i] = projection * glm::lookAt(spot.position, spot.target, up);

        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, spotShadowTexture, 0, (GLint)i);
        glClear(GL_DEPTH_BUFFER_BIT);
        glUniformMatrix4fv(depthLightSpaceLoc, 1, GL_FALSE, glm::value_ptr(spotLightSpaceMatrices[i]));
        depthPassMatrix = spotLightSpaceMatrices[i];
        renderSceneObjects();
    }
    spotShadowPass = false;
    depthPass = false;
    applyViewMode();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, sceneWidth, sceneHeight);
}

// the shadow maps (sun / moon and floodlights) for the main shader
void bindShadowMaps() {
    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(lightSpaceTrMatrixLoc, 1, GL_FALSE, glm::value_ptr(lightSpaceTrMatrix));
    glUniform1i(shadowsEnabledLoc, shadowsEnabled);
    glActiveTexture(GL_TEXTURE0 + SHADOW_MAP_TEXTURE_UNIT);
    glBindTexture(GL_TEXTURE_2D, depthMapTexture);
    glUniform1i(shadowMapLoc, SHADOW_MAP_TEXTURE_UNIT);

    int spotShadows = (shadowsEnabled && floodlightsOn) ? (int)glm::min(SPOT_LIGHTS.size(), (size_t)4) : 0;
    glUniform1i(numSpotShadowsLoc, spotShadows);
    glUniformMatrix4fv(spotLightSpaceLoc, 4, GL_FALSE, glm::value_ptr(spotLightSpaceMatrices[0]));
    glUniform2f(spotShadowNearFarLoc, SPOT_SHADOW_NEAR, SPOT_SHADOW_FAR);
    glActiveTexture(GL_TEXTURE0 + SPOT_SHADOW_TEXTURE_UNIT);
    glBindTexture(GL_TEXTURE_2D_ARRAY, spotShadowTexture);
    glUniform1i(spotShadowMapsLoc, SPOT_SHADOW_TEXTURE_UNIT);
    glActiveTexture(GL_TEXTURE0);
}
