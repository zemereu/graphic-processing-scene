#include "Shadows.hpp"
#include "Lighting.hpp"   // the light's direction, floodlights
#include "Renderer.hpp"

#include <cfloat>

bool shadowsEnabled = true;
bool showShadowMap = false;
GLuint depthMapFBO = 0, depthMapTexture = 0;
gps::Shader depthMapShader;
GLint depthModelLoc, depthLightSpaceLoc;           // in depthMapShader
glm::mat4 lightSpaceMatrices[SHADOW_CASCADES];
float cascadeTexel[SHADOW_CASCADES];               // meters covered by one texel of each cascade
float cascadeDepthRange[SHADOW_CASCADES];          // meters between the near and far planes of each cascade
GLint cascadeSplitsLoc, cascadeTexelLoc, cascadeDepthRangeLoc;   // in myBasicShader
gps::Shader shadowPreviewShader;
GLuint shadowPreviewFBO[SHADOW_CASCADES] = { 0 }, shadowPreviewTexture[SHADOW_CASCADES] = { 0 };
const int SHADOW_PREVIEW_SIZE = 256;
bool depthPass = false;                  // true while drawing into a shadow map
glm::mat4 depthPassMatrix;
bool spotShadowPass = false;
GLuint spotShadowFBO = 0, spotShadowTexture = 0;
glm::mat4 spotLightSpaceMatrices[4];
GLint spotLightSpaceLoc, spotShadowMapsLoc, spotShadowNearFarLoc, numSpotShadowsLoc;   // in myBasicShader

void initShadowMap() {
    // depth textures: what the light "sees" (distance to the closest surface for each texel), one layer per cascade
    glGenTextures(1, &depthMapTexture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, depthMapTexture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, SHADOW_CASCADES, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // outside the shadow map: depth 1 = nothing in front = lit
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    // framebuffer with only a depth attachment (one layer of the array at a time), no color
    glGenFramebuffers(1, &depthMapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthMapTexture, 0, 0);
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

    // preview of the cascades for the UI: small grayscale images
    shadowPreviewShader.loadShader("shaders/screen.vert", "shaders/shadow_preview.frag");
    for (int i = 0; i < SHADOW_CASCADES; i++) {
        glGenTextures(1, &shadowPreviewTexture[i]);
        glBindTexture(GL_TEXTURE_2D, shadowPreviewTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SHADOW_PREVIEW_SIZE, SHADOW_PREVIEW_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenFramebuffers(1, &shadowPreviewFBO[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowPreviewFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, shadowPreviewTexture[i], 0);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    depthModelLoc = glGetUniformLocation(depthMapShader.shaderProgram, "model");
    depthLightSpaceLoc = glGetUniformLocation(depthMapShader.shaderProgram, "lightSpaceTrMatrix");
    lightSpaceTrMatrixLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightSpaceMatrices");
    shadowMapLoc = glGetUniformLocation(myBasicShader.shaderProgram, "shadowMap");
    shadowsEnabledLoc = glGetUniformLocation(myBasicShader.shaderProgram, "shadowsEnabled");
    cascadeSplitsLoc = glGetUniformLocation(myBasicShader.shaderProgram, "cascadeSplits");
    cascadeTexelLoc = glGetUniformLocation(myBasicShader.shaderProgram, "cascadeTexel");
    cascadeDepthRangeLoc = glGetUniformLocation(myBasicShader.shaderProgram, "cascadeDepthRange");
}

// each cascade: the slice of the camera's view between two split distances is wrapped in a sphere, and the light's
// orthographic projection covers that sphere. A sphere looks the same from every direction, so the size of the
// projection doesn't change when the camera turns; with its center snapped to whole texels the shadow edges stay
// still (no shimmering) while the camera moves.
void computeLightSpaceMatrices() {
    glm::vec3 toLight = glm::normalize(lightDir);
    glm::vec3 up = fabs(toLight.y) > 0.99f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 lightView = glm::lookAt(toLight, glm::vec3(0.0f), up);
    float aspect = (float)sceneWidth / (float)glm::max(sceneHeight, 1);

    for (int i = 0; i < SHADOW_CASCADES; i++) {
        // the 8 corners of the slice (world space): the camera's frustum between the two split distances
        glm::mat4 slice = glm::perspective(glm::radians(45.0f), aspect, CASCADE_SPLITS[i], CASCADE_SPLITS[i + 1]);
        glm::mat4 toWorld = glm::inverse(slice * view);
        glm::vec3 corners[8];
        glm::vec3 center(0.0f);
        for (int c = 0; c < 8; c++) {
            glm::vec4 p = toWorld * glm::vec4((c & 1) ? 1.0f : -1.0f, (c & 2) ? 1.0f : -1.0f, (c & 4) ? 1.0f : -1.0f, 1.0f);
            corners[c] = glm::vec3(p) / p.w;
            center += corners[c] / 8.0f;
        }
        float radius = 0.0f;
        for (int c = 0; c < 8; c++) radius = glm::max(radius, glm::distance(center, corners[c]));
        radius = ceil(radius);

        glm::vec3 c = glm::vec3(lightView * glm::vec4(center, 1.0f));
        float texel = 2.0f * radius / SHADOW_MAP_SIZE;
        c.x = floor(c.x / texel) * texel;
        c.y = floor(c.y / texel) * texel;
        // the light looks down -z: near = the side towards the light (extended for the casters outside the sphere)
        glm::mat4 projection = glm::ortho(c.x - radius, c.x + radius, c.y - radius, c.y + radius,
                                          -(c.z + radius) - SHADOW_CASTER_RANGE, -(c.z - radius));
        lightSpaceMatrices[i] = projection * lightView;
        cascadeTexel[i] = texel;
        cascadeDepthRange[i] = 2.0f * radius + SHADOW_CASTER_RANGE;
    }
}

// pass 1: the scene from the sun / moon, depth only, once per cascade
void renderShadowMap() {
    if (!shadowsEnabled) return;
    computeLightSpaceMatrices();
    depthMapShader.useShaderProgram();
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);  // wireframe/points don't apply to the shadows
    depthPass = true;
    for (int i = 0; i < SHADOW_CASCADES; i++) {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthMapTexture, 0, i);
        glClear(GL_DEPTH_BUFFER_BIT);
        glUniformMatrix4fv(depthLightSpaceLoc, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrices[i]));
        depthPassMatrix = lightSpaceMatrices[i];   // culling: only what is in this cascade
        renderSceneObjects();
    }
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

// the shadow maps (sun / moon cascades and floodlights) for the main shader
void bindShadowMaps() {
    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(lightSpaceTrMatrixLoc, SHADOW_CASCADES, GL_FALSE, glm::value_ptr(lightSpaceMatrices[0]));
    glUniform1fv(cascadeSplitsLoc, SHADOW_CASCADES + 1, CASCADE_SPLITS);
    glUniform1fv(cascadeTexelLoc, SHADOW_CASCADES, cascadeTexel);
    glUniform1fv(cascadeDepthRangeLoc, SHADOW_CASCADES, cascadeDepthRange);
    glUniform1i(shadowsEnabledLoc, shadowsEnabled);
    glActiveTexture(GL_TEXTURE0 + SHADOW_MAP_TEXTURE_UNIT);
    glBindTexture(GL_TEXTURE_2D_ARRAY, depthMapTexture);
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

// the cascades as small grayscale images for the UI (dark = close to the light)
void renderShadowPreview() {
    if (!showShadowMap || !shadowsEnabled) return;
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_DEPTH_TEST);
    shadowPreviewShader.useShaderProgram();
    glUniform1i(glGetUniformLocation(shadowPreviewShader.shaderProgram, "shadowMaps"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, depthMapTexture);
    glViewport(0, 0, SHADOW_PREVIEW_SIZE, SHADOW_PREVIEW_SIZE);
    glBindVertexArray(screenVAO);
    for (int i = 0; i < SHADOW_CASCADES; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, shadowPreviewFBO[i]);
        glUniform1i(glGetUniformLocation(shadowPreviewShader.shaderProgram, "layer"), i);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, sceneWidth, sceneHeight);
    glEnable(GL_DEPTH_TEST);
    applyViewMode();
}
