#include "Renderer.hpp"
#include <unordered_map>
#include "Scene.hpp"
#include "Lighting.hpp"
#include "Environment.hpp"
#include "PostProcess.hpp"

#include <algorithm>
#include <cfloat>
#include <map>

// ==============================================================================================================
// Renderer
// ==============================================================================================================

// view modes
int viewMode = VIEW_SOLID;
bool flatShading = false;

// antialiasing: the scene is drawn into a multisampled framebuffer of our own, resolved into a texture
// and copied to the screen. The window's own framebuffer stays single-sampled (MSAA on it got corrupted
// on maximize with the NVIDIA driver); our framebuffer is recreated, under our control, on every resize.
const int MSAA_SAMPLES = 4;
bool antialiasingEnabled = true;

int graphicsQuality = QUALITY_HIGH;
float drawDistanceScale = 1.0f;

// what each preset changes; shadows, fog, weather... stay as the user set them
void applyGraphicsQuality() {
    bool high = graphicsQuality == QUALITY_HIGH, low = graphicsQuality == QUALITY_LOW;
    bool msaa = !low;
    ssaoEnabled = high;                 // ambient occlusion: the most expensive pass
    lightBeamsEnabled = high;           // the beams of the lamps in the fog (ray marching)
    godRaysEnabled = !low;
    bloomEnabled = !low;
    reflectionsEnabled = !low;          // the water: the scene drawn a second time
    flowersEnabled = !low;
    grassGrid = high ? 512 : (low ? 256 : 384);    // the grass around the camera: 51 / 38 / 26 m
    tuftGrid = high ? 400 : (low ? 200 : 300);
    drawDistanceScale = high ? 1.0f : (low ? 0.5f : 0.75f);
    if (msaa != antialiasingEnabled) {
        antialiasingEnabled = msaa;
        createSceneFramebuffers(sceneWidth, sceneHeight);
    }
}
GLuint msaaFBO = 0, msaaColorRBO = 0, msaaDepthRBO = 0;   // multisampled color + depth
GLuint resolveFBO = 0, resolveTexture = 0;                // single-sampled result, read by the screen pass
GLuint resolveDepthTexture = 0;                           // its depth (post-processing)
int sceneWidth = 0, sceneHeight = 0;
gps::Shader screenShader;
GLuint screenVAO = 0;                                      // empty: the fullscreen triangle comes from gl_VertexID

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
    if (resolveDepthTexture) glDeleteTextures(1, &resolveDepthTexture);
    resolveDepthTexture = 0;
    deletePostProcessBuffers();
    deleteReflectionBuffers();
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

    // the depth is resolved too (copied from the first sample): the post-processing reads it
    glGenTextures(1, &resolveDepthTexture);
    glBindTexture(GL_TEXTURE_2D, resolveDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &resolveFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, resolveFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resolveTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, resolveDepthTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "The resolve framebuffer is incomplete" << std::endl;
    }

    createPostProcessBuffers(width, height);
    createReflectionBuffers(width, height);
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

// the time of this frame (set once at its start): the same for every object, so it is sent only once per program
float frameTime = 0.0f;

// a uniform sent only when its value differs from the last one sent to that program (most objects: the same)
static std::unordered_map<unsigned long long, glm::vec4> sentUniforms;
static bool changed(GLuint program, GLint loc, const glm::vec4& v) {
    if (loc < 0) return false;
    unsigned long long key = ((unsigned long long)program << 32) | (unsigned int)loc;
    auto found = sentUniforms.find(key);
    if (found != sentUniforms.end() && found->second == v) return false;
    sentUniforms[key] = v;
    return true;
}
static void uniform1f(GLuint program, GLint loc, float v) { if (changed(program, loc, glm::vec4(v, 0, 0, 0))) glUniform1f(loc, v); }
static void uniform1i(GLuint program, GLint loc, int v) { if (changed(program, loc, glm::vec4((float)v, 0, 0, 0))) glUniform1i(loc, v); }
static void uniform2f(GLuint program, GLint loc, const glm::vec2& v) {
    if (changed(program, loc, glm::vec4(v, 0, 0))) glUniform2fv(loc, 1, glm::value_ptr(v));
}
static void uniform3f(GLuint program, GLint loc, const glm::vec3& v) {
    if (changed(program, loc, glm::vec4(v, 0))) glUniform3fv(loc, 1, glm::value_ptr(v));
}

void setWindSway(GLuint program, gps::Model3D& object, const glm::mat4& objectModel) {
    // a flag: the vertex shader bends its cloth (flagCloth), 0 = not a flag
    bool flag = &object >= &flagModels[0] && &object < &flagModels[0] + FLAG_KINDS;
    uniform1f(program, gps::Shader::location(program, "flagWave"), flag ? windSpeed + 0.3f : 0.0f);
    if (flag) {
        uniform1f(program, gps::Shader::location(program, "flagLength"), modelBox(object).max.x);
        uniform1f(program, gps::Shader::location(program, "time"), frameTime);
    }
    bool tree = &object >= &treeModels[0] && &object < &treeModels[0] + TREE_SPECIES_COUNT;
    float height = tree ? modelBox(object).max.y : 0.0f;
    uniform1f(program, gps::Shader::location(program, "swayHeight"), height);
    if (!tree) return;
    // the wind (world space, see Rain) taken into the tree's own space (its rotation and scale)
    glm::vec3 windWorld(cos(glm::radians(windDirection)), 0.0f, sin(glm::radians(windDirection)));
    glm::vec3 windModel = glm::normalize(glm::inverse(glm::mat3(objectModel)) * windWorld);
    glUniform3fv(gps::Shader::location(program, "swayWind"), 1,
        glm::value_ptr(windModel * (windSpeed * TREE_LEAN * height)));
    // a phase from the tree's position: neighbours don't sway in step
    glm::vec3 pos = glm::vec3(objectModel[3]);
    float phase = glm::fract(sin(pos.x * 12.9898f + pos.z * 78.233f) * 43758.5453f) * glm::two_pi<float>();
    glUniform1f(gps::Shader::location(program, "swayPhase"), phase);
    uniform1f(program, gps::Shader::location(program, "time"), frameTime);
}

// meshes: if given, only those meshes of the model are drawn (the town's details)
// fade: (start, end) meters from the camera: the object dissolves (dithered) between them, instead of popping out
// at a distance limit; (0, 0) = no fade. dither < 1: only that part of its pixels is drawn (the level of detail
// crossfade: the full and the simplified model each draw the pixels the other doesn't, invert tells which)
void renderObject(gps::Model3D& object, const glm::mat4& objectModel, gps::Shader& shader,
    const glm::vec3& emissive = glm::vec3(0.0f), const std::vector<bool>* meshes = nullptr,
    const glm::vec3& paint = glm::vec3(1.0f), const glm::vec2& fade = glm::vec2(0.0f), float dither = 1.0f,
    bool invert = false) {
    // skip the objects that are not in the shadow map area / not in front of the camera
    glm::mat4 clipMatrix = depthPass ? depthPassMatrix : projection * view;
    if (!boxVisible(modelBox(object), clipMatrix * objectModel)) return;
    if (depthPass) { // shadow map (or the grass map): only the geometry matters
        gps::Shader& passShader = grassMaskPass ? grassMaskShader : depthMapShader;
        passShader.useShaderProgram();
        glUniformMatrix4fv(gps::Shader::location(passShader.shaderProgram, "model"), 1, GL_FALSE,
            glm::value_ptr(objectModel));
        if (!grassMaskPass) {
            // the shadow of a thing far from the camera fades out in the same way (a dithered shadow map is smoothed
            // by the filtering of the shadows: a lighter shadow, not a hole)
            GLuint program = passShader.shaderProgram;
            uniform2f(program, gps::Shader::location(program, "fadeRange"), spotShadowPass ? glm::vec2(0.0f) : fade);
            // the camera's position: computed again only when the view changes (not for every object)
            static glm::mat4 lastView(0.0f);
            static glm::vec3 cameraPos(0.0f);
            if (view != lastView) { lastView = view; cameraPos = glm::vec3(glm::inverse(view)[3]); }
            uniform3f(program, gps::Shader::location(program, "fadeCenter"), cameraPos);
        }
        setWindSway(passShader.shaderProgram, object, objectModel);
        if (meshes) object.DrawMeshes(passShader, *meshes); else object.Draw(passShader);
        return;
    }
    shader.useShaderProgram();
    static GLint windowLightsLoc = glGetUniformLocation(shader.shaderProgram, "windowLights");
    static GLint hourLoc = glGetUniformLocation(shader.shaderProgram, "hour");
    bool windows = &object == &cityLights || &object == &genexLights || &object == &romanitaLights ||
                   &object == &telefoaneLights || &object == &satuMareLights;
    GLuint program = shader.shaderProgram;
    uniform1i(program, windowLightsLoc, windows);
    uniform1f(program, hourLoc, timeOfDay);
    uniform3f(program, emissiveColorLoc, emissive);
    uniform3f(program, paintColorLoc, paint);
    static GLint fadeLoc = glGetUniformLocation(shader.shaderProgram, "fadeRange");
    static GLint ditherLoc = glGetUniformLocation(shader.shaderProgram, "ditherKeep");
    static GLint invertLoc = glGetUniformLocation(shader.shaderProgram, "ditherInvert");
    uniform2f(program, fadeLoc, fade);
    uniform1f(program, ditherLoc, dither);
    uniform1i(program, invertLoc, invert);
    // 1 = terrain (texture repetition hidden), 2 = water (animated waves + sky reflection), 0 = everything else
    uniform1i(program, surfaceModeLoc, &object == &terrain ? 1 : (&object == &water ? 2 : 0));
    uniform1f(program, timeLoc, frameTime);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(objectModel));
    setWindSway(shader.shaderProgram, object, objectModel);
    glm::mat3 nm = glm::mat3(glm::inverseTranspose(view * objectModel));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(nm));
    if (meshes) object.DrawMeshes(shader, *meshes); else object.Draw(shader);
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
    glm::vec3 paint;  // body color of a vehicle
    glm::vec2 fade = glm::vec2(0.0f);   // see renderObject
    float dither = 1.0f;
    bool invert = false;
};

// the town's details: thousands of small things, so each mesh (one material in one tile) is culled on its own,
// by distance (beyond maxDistance they are too small to matter) and by the view volume of this pass
void renderDetails(gps::Model3D& model, const std::vector<gps::BoundingBox>& bounds, float maxDistance,
    bool fadeOut = true) {
    glm::mat4 identity(1.0f);
    glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);
    glm::mat4 clipMatrix = depthPass ? depthPassMatrix : projection * view;
    static std::vector<bool> visible;   // (kept between frames: no allocation each frame)
    visible.assign(bounds.size(), false);
    bool any = false;
    for (size_t i = 0; i < bounds.size(); i++) {
        visible[i] = boxDistance(bounds[i], identity, cameraPos) <= maxDistance && boxVisible(bounds[i], clipMatrix);
        any = any || visible[i];
    }
    // they dissolve over the last 20% of their distance instead of popping
    glm::vec2 fade = fadeOut ? glm::vec2(0.8f * maxDistance, maxDistance) : glm::vec2(0.0f);
    if (any) renderObject(model, identity, myBasicShader, glm::vec3(0.0f), &visible, glm::vec3(1.0f), fade);
}

void renderSceneObjects() {
    static std::vector<DrawItem> drawList;   // (kept between frames: no allocation each frame)
    drawList.clear();
    // brutalist scene (positions in meters)
    glm::mat4 identity(1.0f);
    glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);
    // collects the objects; far from the camera the heavy models are replaced by their simplified copies
    // (only in the camera pass: the shadow map always uses the full models, so the shadows keep all their detail)
    auto draw = [&](gps::Model3D& object, const glm::mat4& m, const glm::vec3& emissive = glm::vec3(0.0f),
                    const glm::vec3& paint = glm::vec3(1.0f)) {
        drawList.push_back({ &object, m, emissive, boxDistance(modelBox(object), m, cameraPos), paint });
    };
    // the same, dissolving between fade.x and fade.y meters from the camera
    auto drawFaded = [&](gps::Model3D& object, const glm::mat4& m, const glm::vec2& fade,
                         const glm::vec3& emissive, const glm::vec3& paint) {
        DrawItem item{ &object, m, emissive, boxDistance(modelBox(object), m, cameraPos), paint };
        item.fade = fade;
        drawList.push_back(item);
    };
    // the vehicles: hundreds of them, so not drawn far away (tiny), and their shadows only near the camera
    auto drawVehicle = [&](int type, const glm::mat4& m, const glm::vec3& paint, bool lit) {
        float distance = boxDistance(modelBox(vehicleModels[type]), m, cameraPos);
        float range = (depthPass ? DETAIL_SHADOW_DISTANCE : VEHICLE_DISTANCE) * drawDistanceScale;
        if (distance > range) return;
        DrawItem body{ &vehicleModels[type], m, glm::vec3(0.0f), distance,
                       VEHICLES[type].painted ? paint : glm::vec3(1.0f) };
        body.fade = glm::vec2(0.8f * range, range);
        drawList.push_back(body);
        if (!depthPass) {
            drawList.push_back({ &vehicleLights[type], m, lit ? glm::vec3(1.0f) : glm::vec3(0.0f), distance,
                                 glm::vec3(1.0f) });
        }
    };
    // the level of detail: near, the full model; far, the simplified one; in between (a band around LOD_DISTANCE)
    // both, dithered into each other (each draws the pixels the other leaves out), so the switch doesn't pop
    auto drawLod = [&](gps::Model3D& full, gps::Model3D& simple, const glm::mat4& m) {
        if (depthPass) { draw(full, m); return; }
        // the reflection is small and wobbly: the simplified models are enough there
        if (reflectionPass) { draw(simple, m); return; }
        float d = boxDistance(modelBox(full), m, cameraPos);
        float L = LOD_DISTANCE * drawDistanceScale, band = 0.15f * L;
        if (d < L - band) { draw(full, m); return; }
        if (d > L + band) { draw(simple, m); return; }
        float t = (d - (L - band)) / (2.0f * band);
        DrawItem a{ &full, m, glm::vec3(0.0f), d, glm::vec3(1.0f) };
        a.dither = 1.0f - t;
        DrawItem b{ &simple, m, glm::vec3(0.0f), d, glm::vec3(1.0f) };
        b.dither = t;
        b.invert = true;
        drawList.push_back(a);
        drawList.push_back(b);
    };
    draw(terrain, identity);
    if (!depthPass && !reflectionPass) {
        draw(water, identity);   // flat, under the terrain: it doesn't cast shadows (nor is it in its own reflection)
    }
    draw(trees, identity);   // no colliders: the camera can pass through the foliage
    // the 3D trees: dissolving far away (their shadows nearer); the far forest is the billboards of `trees`
    float treeRange = (depthPass ? 350.0f : 700.0f) * drawDistanceScale;
    // the leafy trees of the town: one model, its tiles culled one by one
    // (not in the grass map: the grass grows under them)
    if (!grassMaskPass) renderDetails(yardTrees, yardTreeBounds, reflectionPass ? 300.0f : treeRange, !reflectionPass);
    for (const NearTree& tree : nearTrees) {
        float distance = glm::distance(cameraPos, glm::vec3(tree.model[3]));
        if (distance > treeRange) continue;
        drawFaded(treeModels[tree.species], tree.model, glm::vec2(0.8f * treeRange, treeRange), glm::vec3(0.0f),
                  glm::vec3(1.0f));
    }
    draw(satuMare, satuMareMatrix());
    draw(genex, genexMatrix());
    draw(genexRestaurant, restaurantMatrix());
    if (windowsLit) {
        draw(genexLights, genexMatrix(), glm::vec3(1.0f));
        draw(romanitaLights, placed(romanitaPlace), glm::vec3(1.0f));
        draw(telefoaneLights, telefoaneMatrix(), glm::vec3(1.0f));
        draw(satuMareLights, satuMareMatrix(), glm::vec3(1.0f));
        draw(cityLights, identity, glm::vec3(1.0f));
    }
    draw(panorama, panoramaMatrix());
    draw(telefoane, telefoaneMatrix());
    drawLod(motherland, motherlandLod, placed(motherlandPlace));
    draw(romanita, placed(romanitaPlace));
    drawLod(powerPlant, powerPlantLod, placed(powerPlantPlace));
    drawLod(duga, dugaLod, placed(dugaPlace));
    drawLod(pripyatBlocks, pripyatBlocksLod, placed(pripyatPlace));
    draw(ferrisSupport, placed(ferrisPlace));
    for (const Flag& f : flags) {
        draw(flagModels[f.kind], flagMatrix(f));
        if (f.pole) draw(flagpole, flagpoleMatrix(f));
    }
    draw(cityNorth, identity);
    draw(citySouth, identity);
    draw(cityProps, identity);
    for (const CityProp& prop : cityProps_) {
        if (prop.car < 0) {
            draw(lampPost, prop.model);
            draw(lampBulb, prop.model, nightMode ? LAMP_GLOW : glm::vec3(0.0f));
        } else {
            drawVehicle(prop.car, prop.model, prop.paint, false);
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
    if (trafficEnabled) {
        for (const Car& car : cars) {
            glm::mat4 carModel = carMatrix(car);
            drawVehicle(car.model, carModel, car.paint, nightMode);
            if (car.model == TROLLEYBUS) draw(trolleyPoles, carModel);
        }
    }
    draw(trolleyWires, identity);
    // the pedestrians: in parts, only near the camera (their shadows nearer still)
    if (pedestriansEnabled) {
        float range = (depthPass ? 60.0f : PEDESTRIAN_DISTANCE) * drawDistanceScale;
        for (const Pedestrian& p : pedestrians) {
            const Walk& w = walks[p.walk];
            if (glm::distance(cameraPos, w.points[0]) > range + w.length) continue;   // the whole way is far
            PedestrianPose pose = pedestrianPose(p);
            if (glm::distance(cameraPos, glm::vec3(pose.body[3])) > range) continue;
            glm::vec2 fade(0.75f * range, range);
            drawFaded(pedestrianBodies[p.kind], pose.body, fade, glm::vec3(0.0f), p.coat);
            for (int k = 0; k < 2; k++) {
                drawFaded(pedestrianThigh, pose.thighs[k], fade, glm::vec3(0.0f), p.trousers);
                drawFaded(pedestrianShin, pose.shins[k], fade, glm::vec3(0.0f), p.trousers);
                drawFaded(pedestrianArm, pose.arms[k], fade, glm::vec3(0.0f), p.coat);
            }
        }
    }
    // z-buffer (course 07 - Fragment processing): drawn from near to far, the hidden fragments of the objects behind
    // fail the depth test before the (expensive) lighting shader runs for them; the order doesn't matter for the depth map
    if (!depthPass) {
        std::sort(drawList.begin(), drawList.end(),
            [](const DrawItem& a, const DrawItem& b) { return a.distance < b.distance; });
    }
    for (const DrawItem& item : drawList) {
        renderObject(*item.object, item.model, myBasicShader, item.emissive, nullptr, item.paint, item.fade,
                     item.dither, item.invert);
    }

    // the town's details (the grass map needs all of them: no grass under a hedge or a bench)
    float range = grassMaskPass ? FLT_MAX
                : depthPass ? DETAIL_SHADOW_DISTANCE * drawDistanceScale
                : reflectionPass ? 150.0f : DETAIL_DISTANCE * drawDistanceScale;
    renderDetails(detailObjects, detailObjectBounds, range, !grassMaskPass && !reflectionPass);
    if (!depthPass && !reflectionPass) {   // flat on the ground: no shadows, too small for the reflection
        renderDetails(detailDecals, detailDecalBounds, DECAL_DISTANCE * drawDistanceScale);
    }
}

// one frame, pass after pass
void renderFrame() {
    frameTime = (float)glfwGetTime();
    // 0. shadow maps: the scene from the sun / moon (and from the floodlights at night)
    renderShadowMap();
    renderSpotShadowMaps();
    renderShadowPreview();   // only while the UI shows it
    // 0b. the scene mirrored in the water
    renderReflection();
    // 1. the scene, the sky, the birds, the smoke and the rain into the multisampled framebuffer
    bindReflection();
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO);
    renderScene();
    renderGrass();
    renderSkybox();   // after the objects: only fills the pixels they left empty
    renderBirds();
    renderSmoke();    // blended: after everything opaque
    renderRain();
    // 2. resolve: average the samples of each pixel into the texture (and copy the depth)
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFBO);
    glBlitFramebuffer(0, 0, sceneWidth, sceneHeight, 0, 0, sceneWidth, sceneHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitFramebuffer(0, 0, sceneWidth, sceneHeight, 0, 0, sceneWidth, sceneHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    // 3. post-processing: ambient occlusion, god rays, bloom
    renderPostProcess();
    // 4. the image to the window (tone mapped); the UI is drawn on top after this
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    renderScreenQuad();
}

// ==============================================================================================================
// Reflection
// ==============================================================================================================

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
