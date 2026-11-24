#include "Renderer.hpp"
#include "Scene.hpp"
#include "Lighting.hpp"
#include "Shadows.hpp"
#include "Skybox.hpp"
#include "Rain.hpp"
#include "Traffic.hpp"

#include <algorithm>
#include <cfloat>
#include <map>

// view modes
int viewMode = VIEW_SOLID;
bool flatShading = false;

// antialiasing: the scene is drawn into a multisampled framebuffer of our own, resolved into a texture
// and copied to the screen. The window's own framebuffer stays single-sampled (MSAA on it got corrupted
// on maximize with the NVIDIA driver); our framebuffer is recreated, under our control, on every resize.
const int MSAA_SAMPLES = 4;
bool antialiasingEnabled = true;
GLuint msaaFBO = 0, msaaColorRBO = 0, msaaDepthRBO = 0;   // multisampled color + depth
GLuint resolveFBO = 0, resolveTexture = 0;                // single-sampled result, read by the screen pass
int sceneWidth = 0, sceneHeight = 0;
gps::Shader screenShader;
GLuint screenVAO = 0;                                      // empty: the fullscreen triangle comes from gl_VertexID
bool bloomEnabled = true;
gps::Shader bloomShader;
GLuint bloomFBO[2] = { 0, 0 }, bloomTexture[2] = { 0, 0 };

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

void initShaders() {
    screenShader.loadShader(
        "shaders/screen.vert",
        "shaders/screen.frag");
    bloomShader.loadShader(
        "shaders/screen.vert",
        "shaders/bloom.frag");
    myBasicShader.loadShader(
        "shaders/basic.vert",
        "shaders/basic.frag");
    rainShader.loadShader(
        "shaders/rain.vert",
        "shaders/rain.frag");
    depthMapShader.loadShader(
        "shaders/shadow.vert",
        "shaders/shadow.frag");
}

void applyViewMode() {
    GLenum mode = GL_FILL;
    if (viewMode == VIEW_WIREFRAME) mode = GL_LINE;
    if (viewMode == VIEW_POINTS) mode = GL_POINT;
    glPolygonMode(GL_FRONT_AND_BACK, mode);
}

void applyShadingMode() {
    myBasicShader.useShaderProgram();
    glUniform1i(flatShadingLoc, flatShading);
}

void deleteSceneFramebuffers() {
    if (msaaFBO) glDeleteFramebuffers(1, &msaaFBO);
    if (msaaColorRBO) glDeleteRenderbuffers(1, &msaaColorRBO);
    if (msaaDepthRBO) glDeleteRenderbuffers(1, &msaaDepthRBO);
    if (resolveFBO) glDeleteFramebuffers(1, &resolveFBO);
    if (resolveTexture) glDeleteTextures(1, &resolveTexture);
    msaaFBO = msaaColorRBO = msaaDepthRBO = resolveFBO = resolveTexture = 0;
    for (int i = 0; i < 2; i++) {
        if (bloomFBO[i]) glDeleteFramebuffers(1, &bloomFBO[i]);
        if (bloomTexture[i]) glDeleteTextures(1, &bloomTexture[i]);
        bloomFBO[i] = bloomTexture[i] = 0;
    }
}

// (re)creates the framebuffers at the window size; samples = 0 means no antialiasing
void createSceneFramebuffers(int width, int height) {
    deleteSceneFramebuffers();
    sceneWidth = width;
    sceneHeight = height;

    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    int samples = antialiasingEnabled ? glm::min(MSAA_SAMPLES, (int)maxSamples) : 0;

    // RGBA16F: the scene stays in linear colors (no banding in the dark night scenes);
    // the conversion to sRGB happens once, when the result is copied to the screen
    glGenRenderbuffers(1, &msaaColorRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaColorRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA16F, width, height);
    glGenRenderbuffers(1, &msaaDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaDepthRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &msaaFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaaColorRBO);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaaDepthRBO);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "The multisampled framebuffer is incomplete" << std::endl;
    }

    glGenTextures(1, &resolveTexture);
    glBindTexture(GL_TEXTURE_2D, resolveTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &resolveFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, resolveFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resolveTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "The resolve framebuffer is incomplete" << std::endl;
    }

    // bloom: two half-resolution HDR textures (the blur reads one and writes the other)
    for (int i = 0; i < 2; i++) {
        glGenTextures(1, &bloomTexture[i]);
        glBindTexture(GL_TEXTURE_2D, bloomTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, glm::max(width / 2, 1), glm::max(height / 2, 1), 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1, &bloomFBO[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloomTexture[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "The bloom framebuffer is incomplete" << std::endl;
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// draws a model with its own model matrix; normalMatrix is computed per object
// frustum culling: the box of the whole model (model space), computed once per model
std::map<const gps::Model3D*, gps::BoundingBox> modelBoxes;

const gps::BoundingBox& modelBox(gps::Model3D& object) {
    auto it = modelBoxes.find(&object);
    if (it != modelBoxes.end()) return it->second;
    gps::BoundingBox box{ glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX) };
    for (const gps::BoundingBox& b : object.GetMeshBounds()) {
        box.min = glm::min(box.min, b.min);
        box.max = glm::max(box.max, b.max);
    }
    return modelBoxes[&object] = box;
}

// Cohen-Sutherland region code (course 05 - Clipping), in 3D: 6 bits, one for each plane of the view volume.
// In clip space a point is inside when -w <= x, y, z <= w.
enum : unsigned { OUT_LEFT = 1, OUT_RIGHT = 2, OUT_BOTTOM = 4, OUT_TOP = 8, OUT_NEAR = 16, OUT_FAR = 32 };

unsigned outCode(const glm::vec4& c) {
    unsigned code = 0;
    if (c.x < -c.w) code |= OUT_LEFT;
    if (c.x > c.w)  code |= OUT_RIGHT;
    if (c.y < -c.w) code |= OUT_BOTTOM;
    if (c.y > c.w)  code |= OUT_TOP;
    if (c.z < -c.w) code |= OUT_NEAR;
    if (c.z > c.w)  code |= OUT_FAR;
    return code;
}

// frustum culling with "trivial reject": if all 8 corners of the box have a bit in common (AND != 0),
// the whole box is beyond the same plane, so the object is surely not visible and is not drawn at all
bool boxVisible(const gps::BoundingBox& box, const glm::mat4& clipMatrix) {
    unsigned common = OUT_LEFT | OUT_RIGHT | OUT_BOTTOM | OUT_TOP | OUT_NEAR | OUT_FAR;
    for (int i = 0; i < 8; i++) {
        glm::vec3 p((i & 1) ? box.max.x : box.min.x, (i & 2) ? box.max.y : box.min.y, (i & 4) ? box.max.z : box.min.z);
        common &= outCode(clipMatrix * glm::vec4(p, 1.0f));
        if (common == 0) return true;   // no common plane: (possibly) visible
    }
    return false;
}

// distance from the camera to the nearest point of the object's box (world space)
float boxDistance(const gps::BoundingBox& box, const glm::mat4& objectModel, const glm::vec3& cameraPos) {
    glm::vec3 lo(FLT_MAX), hi(-FLT_MAX);
    for (int i = 0; i < 8; i++) {
        glm::vec3 p((i & 1) ? box.max.x : box.min.x, (i & 2) ? box.max.y : box.min.y, (i & 4) ? box.max.z : box.min.z);
        glm::vec3 w = glm::vec3(objectModel * glm::vec4(p, 1.0f));
        lo = glm::min(lo, w);
        hi = glm::max(hi, w);
    }
    return glm::distance(cameraPos, glm::clamp(cameraPos, lo, hi));
}

// wind in the trees (windSway() in basic.vert / shadow.vert): only the near trees sway
const float TREE_LEAN = 0.004f;   // how far the top of a tree leans: this fraction of its height per m/s of wind

void setWindSway(GLuint program, gps::Model3D& object, const glm::mat4& objectModel) {
    bool tree = &object >= &treeModels[0] && &object < &treeModels[0] + TREE_SPECIES_COUNT;
    float height = tree ? modelBox(object).max.y : 0.0f;
    glUniform1f(glGetUniformLocation(program, "swayHeight"), height);
    if (!tree) return;
    // the wind (world space, see Rain) taken into the tree's own space (its rotation and scale)
    glm::vec3 windWorld(cos(glm::radians(windDirection)), 0.0f, sin(glm::radians(windDirection)));
    glm::vec3 windModel = glm::normalize(glm::inverse(glm::mat3(objectModel)) * windWorld);
    glUniform3fv(glGetUniformLocation(program, "swayWind"), 1,
        glm::value_ptr(windModel * (windSpeed * TREE_LEAN * height)));
    // a phase from the tree's position: neighbours don't sway in step
    glm::vec3 pos = glm::vec3(objectModel[3]);
    float phase = glm::fract(sin(pos.x * 12.9898f + pos.z * 78.233f) * 43758.5453f) * glm::two_pi<float>();
    glUniform1f(glGetUniformLocation(program, "swayPhase"), phase);
    glUniform1f(glGetUniformLocation(program, "time"), (float)glfwGetTime());
}

void renderObject(gps::Model3D& object, const glm::mat4& objectModel, gps::Shader shader,
    const glm::vec3& emissive = glm::vec3(0.0f)) {
    // skip the objects that are not in the shadow map area / not in front of the camera
    glm::mat4 clipMatrix = depthPass ? depthPassMatrix : projection * view;
    if (!boxVisible(modelBox(object), clipMatrix * objectModel)) return;
    if (depthPass) { // shadow map: only the geometry matters
        depthMapShader.useShaderProgram();
        glUniformMatrix4fv(depthModelLoc, 1, GL_FALSE, glm::value_ptr(objectModel));
        setWindSway(depthMapShader.shaderProgram, object, objectModel);
        object.Draw(depthMapShader);
        return;
    }
    shader.useShaderProgram();
    glUniform3fv(emissiveColorLoc, 1, glm::value_ptr(emissive));
    // 1 = terrain (texture repetition hidden), 2 = water (animated waves + sky reflection), 0 = everything else
    glUniform1i(surfaceModeLoc, &object == &terrain ? 1 : (&object == &water ? 2 : 0));
    glUniform1f(timeLoc, (float)glfwGetTime());
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(objectModel));
    setWindSway(shader.shaderProgram, object, objectModel);
    glm::mat3 nm = glm::mat3(glm::inverseTranspose(view * objectModel));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(nm));
    object.Draw(shader);
}

void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // the shadow maps from the depth passes, read by the lighting shader
    bindShadowMaps();
    // the sky of the current time of day: fog color and water reflections
    bindSkyTexture();

    renderSceneObjects();
}

// all the objects of the scene; used by both passes (the shadow map and the normal rendering)
// one object to draw in this pass
struct DrawItem {
    gps::Model3D* object;
    glm::mat4 model;
    glm::vec3 emissive;
    float distance;   // from the camera, to the nearest point of the object
};

void renderSceneObjects() {
    std::vector<DrawItem> drawList;
    // brutalist scene (positions in meters)
    glm::mat4 identity(1.0f);
    glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);
    // collects the objects; far from the camera the heavy models are replaced by their simplified copies
    // (only in the camera pass: the shadow map always uses the full models, so the shadows keep all their detail)
    auto draw = [&](gps::Model3D& object, const glm::mat4& m, const glm::vec3& emissive = glm::vec3(0.0f)) {
        drawList.push_back({ &object, m, emissive, boxDistance(modelBox(object), m, cameraPos) });
    };
    auto lod = [&](gps::Model3D& full, gps::Model3D& simple, const glm::mat4& m) -> gps::Model3D& {
        return !depthPass && boxDistance(modelBox(full), m, cameraPos) > LOD_DISTANCE ? simple : full;
    };
    draw(terrain, identity);
    if (!depthPass) {
        draw(water, identity);   // flat, under the terrain: it doesn't cast shadows
    }
    draw(trees, identity);   // no colliders: the camera can pass through the foliage
    for (const NearTree& tree : nearTrees) {
        draw(treeModels[tree.species], tree.model);
    }
    draw(satuMare, satuMareMatrix());
    draw(genex, genexMatrix());
    draw(genexRestaurant, restaurantMatrix());
    if (windowsLit) {
        draw(genexLights, genexMatrix(), glm::vec3(1.0f));
        draw(romanitaLights, glm::translate(identity, ROMANITA_POS), glm::vec3(1.0f));
        draw(telefoaneLights, telefoaneMatrix(), glm::vec3(1.0f));
        draw(satuMareLights, satuMareMatrix(), glm::vec3(1.0f));
        draw(cityLights, identity, glm::vec3(1.0f));
    }
    draw(panorama, panoramaMatrix());
    draw(telefoane, telefoaneMatrix());
    draw(lod(motherland, motherlandLod, glm::translate(identity, MOTHERLAND_POS)), glm::translate(identity, MOTHERLAND_POS));
    draw(romanita, glm::translate(identity, ROMANITA_POS));
    draw(lod(powerPlant, powerPlantLod, glm::translate(identity, POWER_PLANT_POS)), glm::translate(identity, POWER_PLANT_POS));
    draw(lod(duga, dugaLod, glm::translate(identity, DUGA_POS)), glm::translate(identity, DUGA_POS));
    draw(lod(pripyatBlocks, pripyatBlocksLod, glm::translate(identity, PRIPYAT_POS)), glm::translate(identity, PRIPYAT_POS));
    draw(ferrisSupport, glm::translate(identity, FERRIS_POS));
    draw(cityNorth, identity);
    draw(citySouth, identity);
    draw(cityProps, identity);
    for (const CityProp& prop : cityProps_) {
        if (prop.car < 0) {
            draw(lampPost, prop.model);
            draw(lampBulb, prop.model, nightMode ? LAMP_GLOW : glm::vec3(0.0f));
        } else {
            draw(carModels[prop.car], prop.model);
        }
    }
    draw(ferrisWheel, ferrisWheelMatrix());
    for (int i = 0; i < FERRIS_GONDOLAS; i++) {
        draw(ferrisGondola, ferrisGondolaMatrix(i));
    }

    // street lamps; the bulbs glow at night
    glm::vec3 bulbGlow = nightMode ? LAMP_GLOW : glm::vec3(0.0f);
    for (const glm::vec3& base : LAMP_POSITIONS) {
        glm::mat4 lampModel = glm::translate(identity, base);
        draw(lampPost, lampModel);
        draw(lampBulb, lampModel, bulbGlow);
    }
    glm::vec3 lensGlow = floodlightsOn ? LAMP_GLOW : glm::vec3(0.0f);
    for (const SpotLight& spot : SPOT_LIGHTS) {
        if (spotShadowPass) break;   // the light is inside the floodlight: its own housing would hide it
        draw(floodlight, glm::translate(identity, spot.position));
        draw(floodlightLens, glm::translate(identity, spot.position), lensGlow);
    }

    // traffic
    glm::vec3 carLightGlow = nightMode ? glm::vec3(1.0f) : glm::vec3(0.0f);
    if (trafficEnabled) {
        for (const Car& car : cars) {
            glm::mat4 carModel = carMatrix(car);
            draw(carModels[car.model], carModel);
            draw(carLights, carModel, carLightGlow);
        }
    }
    // z-buffer (course 07 - Fragment processing): drawn from near to far, the hidden fragments of the objects behind
    // fail the depth test before the (expensive) lighting shader runs for them; the order doesn't matter for the depth map
    if (!depthPass) {
        std::sort(drawList.begin(), drawList.end(),
            [](const DrawItem& a, const DrawItem& b) { return a.distance < b.distance; });
    }
    for (const DrawItem& item : drawList) {
        renderObject(*item.object, item.model, myBasicShader, item.emissive);
    }
}

// bloom: the parts brighter than the threshold, blurred at half resolution, end up in bloomTexture[0]
void renderBloom() {
    if (!bloomEnabled) return;
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);   // linear HDR values, not colors for the screen
    glViewport(0, 0, glm::max(sceneWidth / 2, 1), glm::max(sceneHeight / 2, 1));
    bloomShader.useShaderProgram();
    glUniform1i(glGetUniformLocation(bloomShader.shaderProgram, "image"), 0);
    glUniform1f(glGetUniformLocation(bloomShader.shaderProgram, "threshold"), BLOOM_THRESHOLD);
    GLint modeLoc = glGetUniformLocation(bloomShader.shaderProgram, "mode");
    GLint directionLoc = glGetUniformLocation(bloomShader.shaderProgram, "direction");
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(screenVAO);

    // bright pass (the half-size target also averages 2x2 pixels)
    glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO[0]);
    glUniform1i(modeLoc, 0);
    glBindTexture(GL_TEXTURE_2D, resolveTexture);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Gaussian blur, separable: horizontal into [1], vertical back into [0], repeated (each repeat widens the glow)
    glUniform1i(modeLoc, 1);
    glm::vec2 texel(1.0f / glm::max(sceneWidth / 2, 1), 1.0f / glm::max(sceneHeight / 2, 1));
    for (int i = 0; i < BLOOM_BLUR_PASSES; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO[1]);
        glUniform2f(directionLoc, texel.x, 0.0f);
        glBindTexture(GL_TEXTURE_2D, bloomTexture[0]);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO[0]);
        glUniform2f(directionLoc, 0.0f, texel.y);
        glBindTexture(GL_TEXTURE_2D, bloomTexture[1]);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, sceneWidth, sceneHeight);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_DEPTH_TEST);
    applyViewMode();
}

// draws the resolved scene over the whole window: + bloom, then tone mapping (HDR -> screen)
void renderScreenQuad() {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // the wireframe/points view modes apply to the scene, not to this copy
    glDisable(GL_DEPTH_TEST);
    screenShader.useShaderProgram();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, resolveTexture);
    glUniform1i(glGetUniformLocation(screenShader.shaderProgram, "screenTexture"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomTexture[0]);
    glUniform1i(glGetUniformLocation(screenShader.shaderProgram, "bloomTexture"), 1);
    glActiveTexture(GL_TEXTURE0);
    glUniform1f(glGetUniformLocation(screenShader.shaderProgram, "bloomStrength"), bloomEnabled ? BLOOM_STRENGTH : 0.0f);
    glUniform1f(glGetUniformLocation(screenShader.shaderProgram, "exposure"), EXPOSURE);
    glBindVertexArray(screenVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    applyViewMode();
}
