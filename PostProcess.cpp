#include "PostProcess.hpp"
#include "Renderer.hpp"
#include "Lighting.hpp"
#include "Environment.hpp"
#include "Controls.hpp"

#include <random>
#include <cmath>

bool bloomEnabled = true, godRaysEnabled = true, ssaoEnabled = true, lightBeamsEnabled = true, autoExposureEnabled = true;
gps::Shader bloomShader, godRaysShader, ssaoShader, volumetricShader, exposureShader;
GLuint volumetricFBO[2] = { 0, 0 }, volumetricTexture[2] = { 0, 0 };
GLuint luminanceFBO = 0, luminanceTexture = 0;
GLuint adaptedFBO[2] = { 0, 0 }, adaptedTexture[2] = { 0, 0 };   // 1x1, ping-pong: last frame / this frame
int adaptedCurrent = 0;
bool exposureSnap = true;
float gloomFilter = 0.0f;
bool lightBeamsDrawn = false;   // this frame
GLuint bloomFBO[2] = { 0, 0 }, bloomTexture[2] = { 0, 0 };   // ping-pong for the blur
GLuint godRaysFBO = 0, godRaysTexture = 0;
GLuint ssaoFBO[2] = { 0, 0 }, ssaoTexture[2] = { 0, 0 };     // raw, blurred
std::vector<glm::vec3> ssaoKernel;

void initPostProcess() {
    bloomShader.loadShader("shaders/screen.vert", "shaders/bloom.frag");
    godRaysShader.loadShader("shaders/screen.vert", "shaders/godrays.frag");
    ssaoShader.loadShader("shaders/screen.vert", "shaders/ssao.frag");
    volumetricShader.loadShader("shaders/screen.vert", "shaders/volumetric.frag");
    exposureShader.loadShader("shaders/screen.vert", "shaders/exposure.frag");

    // auto exposure: the adapted luminance, 1x1, one value per frame (it survives window resizes)
    for (int i = 0; i < 2; i++) {
        glGenTextures(1, &adaptedTexture[i]);
        glBindTexture(GL_TEXTURE_2D, adaptedTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 1, 1, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glGenFramebuffers(1, &adaptedFBO[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, adaptedFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, adaptedTexture[i], 0);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // SSAO sample points: random directions in the hemisphere above the surface (z = along the normal), more of them
    // close to the point (they matter more: the nearby geometry is what occludes)
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> random01(0.0f, 1.0f);
    for (int i = 0; i < SSAO_SAMPLES; i++) {
        glm::vec3 s(random01(rng) * 2.0f - 1.0f, random01(rng) * 2.0f - 1.0f, random01(rng));
        s = glm::normalize(s) * random01(rng);
        float scale = (float)i / SSAO_SAMPLES;
        s *= glm::mix(0.1f, 1.0f, scale * scale);
        ssaoKernel.push_back(s);
    }
    ssaoShader.useShaderProgram();
    glUniform3fv(glGetUniformLocation(ssaoShader.shaderProgram, "kernel"), SSAO_SAMPLES, glm::value_ptr(ssaoKernel[0]));
}

// one half-resolution HDR texture + its framebuffer
void createTarget(GLuint& fbo, GLuint& texture, int width, int height, GLenum format) {
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "A post-processing framebuffer is incomplete" << std::endl;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void createPostProcessBuffers(int width, int height) {
    int w = glm::max(width / 2, 1), h = glm::max(height / 2, 1);
    for (int i = 0; i < 2; i++) {
        createTarget(bloomFBO[i], bloomTexture[i], w, h, GL_RGBA16F);
        createTarget(ssaoFBO[i], ssaoTexture[i], w, h, GL_R8);
    }
    createTarget(godRaysFBO, godRaysTexture, w, h, GL_RGBA16F);
    for (int i = 0; i < 2; i++) createTarget(volumetricFBO[i], volumetricTexture[i], w, h, GL_RGBA16F);
    createTarget(luminanceFBO, luminanceTexture, w, h, GL_R16F);
    glBindTexture(GL_TEXTURE_2D, luminanceTexture);   // its mipmaps give the average
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void deleteTarget(GLuint& fbo, GLuint& texture) {
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (texture) glDeleteTextures(1, &texture);
    fbo = texture = 0;
}

void deletePostProcessBuffers() {
    for (int i = 0; i < 2; i++) {
        deleteTarget(bloomFBO[i], bloomTexture[i]);
        deleteTarget(ssaoFBO[i], ssaoTexture[i]);
    }
    deleteTarget(godRaysFBO, godRaysTexture);
    for (int i = 0; i < 2; i++) deleteTarget(volumetricFBO[i], volumetricTexture[i]);
    deleteTarget(luminanceFBO, luminanceTexture);
}

void resetExposure() {
    exposureSnap = true;
}

void drawFullscreen() {
    glBindVertexArray(screenVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void bindTexture(int unit, GLuint texture) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture);
}

// SSAO (screen-space ambient occlusion): for each pixel, points around it in the hemisphere of its normal (rebuilt
// from the depth buffer) are tested against the depth buffer: the more of them are hidden behind geometry, the less
// ambient light reaches the pixel. Then a blur removes the noise of the random rotations.
void renderSSAO() {
    ssaoShader.useShaderProgram();
    GLuint program = ssaoShader.shaderProgram;
    glUniform1i(glGetUniformLocation(program, "depthTexture"), 0);
    glUniform1i(glGetUniformLocation(program, "image"), 1);
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(program, "invProjection"), 1, GL_FALSE,
        glm::value_ptr(glm::inverse(projection)));
    glUniform1f(glGetUniformLocation(program, "radius"), SSAO_RADIUS);
    glUniform2f(glGetUniformLocation(program, "texelSize"), 1.0f / glm::max(sceneWidth / 2, 1),
        1.0f / glm::max(sceneHeight / 2, 1));
    bindTexture(0, resolveDepthTexture);

    glUniform1i(glGetUniformLocation(program, "mode"), 0);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO[0]);
    drawFullscreen();
    glUniform1i(glGetUniformLocation(program, "mode"), 1);
    bindTexture(1, ssaoTexture[0]);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO[1]);
    drawFullscreen();
}

// where the sun is on the screen (0..1) and how strong its rays are (0 = behind the camera, set, or far off screen)
float sunOnScreen(glm::vec2& screenPos) {
    glm::vec3 sun = sunDirection(timeOfDay);
    glm::vec4 clip = projection * glm::mat4(glm::mat3(view)) * glm::vec4(sun, 0.0f);   // a point at infinity
    if (clip.w <= 0.0f) return 0.0f;
    glm::vec2 ndc = glm::vec2(clip) / clip.w;
    screenPos = ndc * 0.5f + 0.5f;
    float offScreen = glm::max(fabs(ndc.x), fabs(ndc.y));
    return glm::smoothstep(-0.02f, 0.05f, sun.y) * (1.0f - glm::smoothstep(1.0f, 1.8f, offScreen));
}

// god rays: from each pixel, steps towards the sun on the screen, adding up the bright sky found on the way (only the
// sky: depth = 1); buildings and trees in front of the sun interrupt it, so the light comes out in shafts
void renderGodRays(float intensity, const glm::vec2& sunPos) {
    godRaysShader.useShaderProgram();
    GLuint program = godRaysShader.shaderProgram;
    glUniform1i(glGetUniformLocation(program, "sceneTexture"), 0);
    glUniform1i(glGetUniformLocation(program, "depthTexture"), 1);
    glUniform2fv(glGetUniformLocation(program, "sunPosition"), 1, glm::value_ptr(sunPos));
    glUniform1f(glGetUniformLocation(program, "intensity"), intensity);
    glUniform1f(glGetUniformLocation(program, "aspect"), (float)sceneWidth / (float)glm::max(sceneHeight, 1));
    glUniform1i(glGetUniformLocation(program, "samples"), GOD_RAY_SAMPLES);
    bindTexture(0, resolveTexture);
    bindTexture(1, resolveDepthTexture);
    glBindFramebuffer(GL_FRAMEBUFFER, godRaysFBO);
    drawFullscreen();
}

// bloom: the parts brighter than the threshold, blurred at half resolution, end up in bloomTexture[0]
void renderBloom() {
    bloomShader.useShaderProgram();
    GLuint program = bloomShader.shaderProgram;
    glUniform1i(glGetUniformLocation(program, "image"), 0);
    glUniform1f(glGetUniformLocation(program, "threshold"), BLOOM_THRESHOLD);
    GLint modeLoc = glGetUniformLocation(program, "mode");
    GLint directionLoc = glGetUniformLocation(program, "direction");

    // bright pass (the half-size target also averages 2x2 pixels)
    glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO[0]);
    glUniform1i(modeLoc, 0);
    bindTexture(0, resolveTexture);
    drawFullscreen();

    // Gaussian blur, separable: horizontal into [1], vertical back into [0], repeated (each repeat widens the glow)
    glUniform1i(modeLoc, 1);
    glm::vec2 texel(1.0f / glm::max(sceneWidth / 2, 1), 1.0f / glm::max(sceneHeight / 2, 1));
    for (int i = 0; i < BLOOM_BLUR_PASSES; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO[1]);
        glUniform2f(directionLoc, texel.x, 0.0f);
        bindTexture(0, bloomTexture[0]);
        drawFullscreen();
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO[0]);
        glUniform2f(directionLoc, 0.0f, texel.y);
        bindTexture(0, bloomTexture[1]);
        drawFullscreen();
    }
}

// light beams: ray marching through the floodlights' cones and the lamps' glow, then a blur for the step noise
void renderLightBeams() {
    volumetricShader.useShaderProgram();
    GLuint program = volumetricShader.shaderProgram;
    glUniform1i(glGetUniformLocation(program, "depthTexture"), 0);
    glUniform1i(glGetUniformLocation(program, "spotShadowMaps"), 1);
    glUniformMatrix4fv(glGetUniformLocation(program, "invProjection"), 1, GL_FALSE, glm::value_ptr(glm::inverse(projection)));
    glUniformMatrix4fv(glGetUniformLocation(program, "invView"), 1, GL_FALSE, glm::value_ptr(glm::inverse(view)));
    // clear air scatters little; fog, rain and snow much more
    float density = 0.007f + (fogEnabled ? fogDensity * 8.0f : 0.0f) + ((rainEnabled || snowEnabled) ? 0.015f : 0.0f);
    glUniform1f(glGetUniformLocation(program, "density"), density);
    glUniform1f(glGetUniformLocation(program, "maxDistance"), LIGHT_BEAM_DISTANCE);
    glUniform1i(glGetUniformLocation(program, "steps"), LIGHT_BEAM_STEPS);

    std::vector<glm::vec3> spotPositions, spotDirections;
    for (const SpotLight& spot : SPOT_LIGHTS) {
        spotPositions.push_back(spot.position);
        spotDirections.push_back(glm::normalize(spot.target - spot.position));
    }
    int spots = floodlightsOn ? (int)glm::min(SPOT_LIGHTS.size(), (size_t)4) : 0;
    glUniform1i(glGetUniformLocation(program, "numSpotLights"), spots);
    glUniform3fv(glGetUniformLocation(program, "spotLightPos"), (GLsizei)spotPositions.size(), glm::value_ptr(spotPositions[0]));
    glUniform3fv(glGetUniformLocation(program, "spotLightDir"), (GLsizei)spotDirections.size(), glm::value_ptr(spotDirections[0]));
    glUniform3fv(glGetUniformLocation(program, "spotLightColor"), 1, glm::value_ptr(SPOT_COLOR));
    glUniform1f(glGetUniformLocation(program, "spotCutOff"), cos(glm::radians(SPOT_INNER_ANGLE)));
    glUniform1f(glGetUniformLocation(program, "spotOuterCutOff"), cos(glm::radians(SPOT_OUTER_ANGLE)));
    glUniformMatrix4fv(glGetUniformLocation(program, "spotLightSpace"), 4, GL_FALSE, glm::value_ptr(spotLightSpaceMatrices[0]));
    glUniform2f(glGetUniformLocation(program, "spotShadowNearFar"), SPOT_SHADOW_NEAR, SPOT_SHADOW_FAR);
    std::vector<glm::vec3> lamps;
    for (const glm::vec3& base : LAMP_POSITIONS) lamps.push_back(base + glm::vec3(0.0f, LAMP_LIGHT_HEIGHT, 0.0f));
    glUniform1i(glGetUniformLocation(program, "numPointLights"), nightMode ? (GLint)lamps.size() : 0);
    glUniform3fv(glGetUniformLocation(program, "pointLightPos"), (GLsizei)lamps.size(), glm::value_ptr(lamps[0]));
    glUniform3fv(glGetUniformLocation(program, "pointLightColor"), 1, glm::value_ptr(LAMP_COLOR));
    bindTexture(0, resolveDepthTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, spotShadowTexture);
    glBindFramebuffer(GL_FRAMEBUFFER, volumetricFBO[0]);
    drawFullscreen();
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    // blur (the bloom's Gaussian): horizontal into [1], vertical back into [0]
    bloomShader.useShaderProgram();
    glUniform1i(glGetUniformLocation(bloomShader.shaderProgram, "image"), 0);
    glUniform1i(glGetUniformLocation(bloomShader.shaderProgram, "mode"), 1);
    GLint directionLoc = glGetUniformLocation(bloomShader.shaderProgram, "direction");
    glm::vec2 texel(1.0f / glm::max(sceneWidth / 2, 1), 1.0f / glm::max(sceneHeight / 2, 1));
    glBindFramebuffer(GL_FRAMEBUFFER, volumetricFBO[1]);
    glUniform2f(directionLoc, texel.x, 0.0f);
    bindTexture(0, volumetricTexture[0]);
    drawFullscreen();
    glBindFramebuffer(GL_FRAMEBUFFER, volumetricFBO[0]);
    glUniform2f(directionLoc, 0.0f, texel.y);
    bindTexture(0, volumetricTexture[1]);
    drawFullscreen();
}

// auto exposure: the average log luminance of the image (from the lowest mipmap), followed slowly by the
// adapted value (1x1 texture, last frame -> this frame)
void renderExposure() {
    exposureShader.useShaderProgram();
    GLuint program = exposureShader.shaderProgram;
    glUniform1i(glGetUniformLocation(program, "image"), 0);
    glUniform1i(glGetUniformLocation(program, "previous"), 1);
    glUniform1i(glGetUniformLocation(program, "mode"), 0);
    bindTexture(0, resolveTexture);
    glBindFramebuffer(GL_FRAMEBUFFER, luminanceFBO);
    drawFullscreen();
    glBindTexture(GL_TEXTURE_2D, luminanceTexture);
    glGenerateMipmap(GL_TEXTURE_2D);

    int w = glm::max(sceneWidth / 2, 1), h = glm::max(sceneHeight / 2, 1);
    float lowestMip = floor(log2((float)glm::max(w, h)));
    int previous = adaptedCurrent;
    adaptedCurrent = 1 - adaptedCurrent;
    glUniform1i(glGetUniformLocation(program, "mode"), 1);
    glUniform1f(glGetUniformLocation(program, "lowestMip"), lowestMip);
    glUniform1f(glGetUniformLocation(program, "deltaTime"), deltaTime);
    glUniform1i(glGetUniformLocation(program, "snap"), exposureSnap);
    bindTexture(1, adaptedTexture[previous]);
    glBindFramebuffer(GL_FRAMEBUFFER, adaptedFBO[adaptedCurrent]);
    glViewport(0, 0, 1, 1);
    drawFullscreen();
    glViewport(0, 0, w, h);
    exposureSnap = false;
}

float godRaysIntensity = 0.0f;   // of this frame (0 = not drawn)

// the effects only make sense on the solid image: in the wireframe / points view modes the lines and the points are
// shown as they are (only the tone mapping is kept, so the colors stay the same)
bool postProcessActive() {
    return viewMode == VIEW_SOLID;
}

void renderPostProcess() {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);   // linear values, not colors for the screen
    glViewport(0, 0, glm::max(sceneWidth / 2, 1), glm::max(sceneHeight / 2, 1));

    bool active = postProcessActive();
    if (active && ssaoEnabled) renderSSAO();
    glm::vec2 sunPos(0.5f);
    godRaysIntensity = (active && godRaysEnabled) ? sunOnScreen(sunPos) : 0.0f;
    if (godRaysIntensity > 0.0f) renderGodRays(godRaysIntensity, sunPos);
    lightBeamsDrawn = active && lightBeamsEnabled && (floodlightsOn || nightMode);
    if (lightBeamsDrawn) renderLightBeams();
    if (active && bloomEnabled) renderBloom();
    if (active && autoExposureEnabled) renderExposure();

    glActiveTexture(GL_TEXTURE0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, sceneWidth, sceneHeight);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_DEPTH_TEST);
    applyViewMode();
}

// draws the resolved scene over the whole window: darkened by the SSAO, + bloom + god rays, then tone mapped
void renderScreenQuad() {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // the wireframe/points view modes apply to the scene, not to this copy
    glDisable(GL_DEPTH_TEST);
    screenShader.useShaderProgram();
    GLuint program = screenShader.shaderProgram;
    bool active = postProcessActive();
    glUniform1i(glGetUniformLocation(program, "screenTexture"), 0);
    glUniform1i(glGetUniformLocation(program, "bloomTexture"), 1);
    glUniform1i(glGetUniformLocation(program, "godRaysTexture"), 2);
    glUniform1i(glGetUniformLocation(program, "ssaoTexture"), 3);
    bindTexture(0, resolveTexture);
    bindTexture(1, bloomTexture[0]);
    bindTexture(2, godRaysTexture);
    bindTexture(3, ssaoTexture[1]);
    bindTexture(4, adaptedTexture[adaptedCurrent]);
    bindTexture(5, volumetricTexture[0]);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(program, "adaptedTexture"), 4);
    glUniform1i(glGetUniformLocation(program, "lightBeamsTexture"), 5);
    glUniform1f(glGetUniformLocation(program, "lightBeamsStrength"), lightBeamsDrawn ? 1.0f : 0.0f);
    glUniform1i(glGetUniformLocation(program, "autoExposure"), active && autoExposureEnabled);
    glUniform1f(glGetUniformLocation(program, "exposureKey"), EXPOSURE_KEY);
    glUniform2f(glGetUniformLocation(program, "exposureLimits"), MIN_EXPOSURE, MAX_EXPOSURE);
    glUniform1f(glGetUniformLocation(program, "bloomStrength"), (active && bloomEnabled) ? BLOOM_STRENGTH : 0.0f);
    glUniform1f(glGetUniformLocation(program, "godRaysStrength"), godRaysIntensity > 0.0f ? GOD_RAYS_STRENGTH : 0.0f);
    glUniform1f(glGetUniformLocation(program, "ssaoStrength"), (active && ssaoEnabled) ? SSAO_STRENGTH : 0.0f);
    glUniform1f(glGetUniformLocation(program, "exposure"), EXPOSURE);
    glUniform1f(glGetUniformLocation(program, "fade"), screenFade);
    glUniform1f(glGetUniformLocation(program, "gloom"), gloomFilter);
    glUniform1f(glGetUniformLocation(program, "grainSeed"), (float)fmod(glfwGetTime() * 7.13, 100.0));
    drawFullscreen();
    glEnable(GL_DEPTH_TEST);
    applyViewMode();
}
