#include "Grass.hpp"
#include "Renderer.hpp"   // renderSceneObjects, scene size, applyViewMode
#include "Shadows.hpp"    // depth pass, cascades
#include "Lighting.hpp"   // sun / moon, street lamps
#include "Rain.hpp"       // wind, snow
#include "stb_image.h"

bool grassEnabled = true;
bool flowersEnabled = true;
bool grassMaskPass = false;
gps::Shader grassShader, grassMaskShader, tuftShader;
GLuint grassMapFBO = 0, grassMapTexture = 0, grassMapDepthRBO = 0;
GLuint grassVAO = 0, grassVBO = 0;
GLuint tuftVAO = 0, tuftVBO = 0, tuftTexture = 0;
const int GRASS_MAP_TEXTURE_UNIT = 10;

// world -> grass map: a top view of the whole world. x and z map to the image, the height to the depth
// (the higher, the nearer: the depth test keeps the highest surface)
glm::mat4 grassMapMatrix() {
    glm::mat4 m(0.0f);
    m[0][0] = 1.0f / WORLD_HALF_SIZE;   // x -> horizontal
    m[2][1] = 1.0f / WORLD_HALF_SIZE;   // z -> vertical
    m[1][2] = -1.0f / 300.0f;           // y from -50 to 550 m -> depth from 1 to -1
    m[3][2] = 1.0f - 50.0f / 300.0f;
    m[3][3] = 1.0f;
    return m;
}

void initGrass() {
    grassShader.loadShader("shaders/grass.vert", "shaders/grass.frag");
    grassMaskShader.loadShader("shaders/grass_map.vert", "shaders/grass_map.frag");
    tuftShader.loadShader("shaders/grass_tuft.vert", "shaders/grass_tuft.frag");

    // the grass map: red = is this grass ground, green = height of the highest surface (meters)
    glGenTextures(1, &grassMapTexture);
    glBindTexture(GL_TEXTURE_2D, grassMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, GRASS_MAP_SIZE, GRASS_MAP_SIZE, 0, GL_RG, GL_FLOAT, nullptr);
    // nearest: blending a grass texel with a building's roof would lift blades into the air at its edge
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glGenRenderbuffers(1, &grassMapDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, grassMapDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, GRASS_MAP_SIZE, GRASS_MAP_SIZE);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glGenFramebuffers(1, &grassMapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, grassMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, grassMapTexture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, grassMapDepthRBO);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "The grass map framebuffer is incomplete" << std::endl;
    }

    // draw the whole scene once from above (the objects are static enough: the grass map is made only once)
    glViewport(0, 0, GRASS_MAP_SIZE, GRASS_MAP_SIZE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_CULL_FACE);   // from above, some surfaces are seen from their back
    grassMaskShader.useShaderProgram();
    glUniformMatrix4fv(glGetUniformLocation(grassMaskShader.shaderProgram, "mapMatrix"), 1, GL_FALSE,
        glm::value_ptr(grassMapMatrix()));
    depthPass = true;
    grassMaskPass = true;
    depthPassMatrix = grassMapMatrix();
    renderSceneObjects();
    grassMaskPass = false;
    depthPass = false;
    glEnable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, sceneWidth, sceneHeight);
    applyLighting();   // restores the clear color

    // one blade: a strip narrowing to a point; x = across the blade (-1..1), y = along it (0 = root, 1 = tip)
    const float blade[] = { -1.0f, 0.0f,  1.0f, 0.0f,  -0.8f, 0.4f,  0.8f, 0.4f,  -0.5f, 0.75f,  0.5f, 0.75f,  0.0f, 1.0f };
    glGenVertexArrays(1, &grassVAO);
    glBindVertexArray(grassVAO);
    glGenBuffers(1, &grassVBO);
    glBindBuffer(GL_ARRAY_BUFFER, grassVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(blade), blade, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // one tuft: two crossed quads (x = across -1..1, y = up 0..1, z = which quad)
    float tuft[2 * 6 * 3];
    const float corners[6][2] = { {-1, 0}, {1, 0}, {1, 1}, {-1, 0}, {1, 1}, {-1, 1} };
    for (int q = 0; q < 2; q++) {
        for (int k = 0; k < 6; k++) {
            tuft[(q * 6 + k) * 3 + 0] = corners[k][0];
            tuft[(q * 6 + k) * 3 + 1] = corners[k][1];
            tuft[(q * 6 + k) * 3 + 2] = (float)q;
        }
    }
    glGenVertexArrays(1, &tuftVAO);
    glBindVertexArray(tuftVAO);
    glGenBuffers(1, &tuftVBO);
    glBindBuffer(GL_ARRAY_BUFFER, tuftVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tuft), tuft, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // the pictures of the tufts, side by side in one texture
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);   // OpenGL expects the first row at the bottom
    unsigned char* data = stbi_load("models/grass/tufts.png", &width, &height, &channels, 4);
    stbi_set_flip_vertically_on_load(false);
    if (!data) {
        std::cerr << "Could not load models/grass/tufts.png" << std::endl;
        return;
    }
    glGenTextures(1, &tuftTexture);
    glBindTexture(GL_TEXTURE_2D, tuftTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
}

// the uniforms the blades and the tufts share: camera, wind, snow, the grass map, the lights and the shadows
static void setGrassUniforms(GLuint program, int grid, float spacing) {
    glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);
    glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(program, "cameraPos"), 1, glm::value_ptr(cameraPos));
    glUniform1f(glGetUniformLocation(program, "time"), (float)glfwGetTime());
    glm::vec2 wind = windSpeed * glm::vec2(cos(glm::radians(windDirection)), sin(glm::radians(windDirection)));
    glUniform2fv(glGetUniformLocation(program, "wind"), 1, glm::value_ptr(wind));
    glUniform1i(glGetUniformLocation(program, "grid"), grid);
    glUniform1f(glGetUniformLocation(program, "spacing"), spacing);
    glUniform1f(glGetUniformLocation(program, "worldHalfSize"), WORLD_HALF_SIZE);
    glUniform1f(glGetUniformLocation(program, "snowCover"), snowCover);
    // light: the sun / moon, the sky (ambient, darker at night), the street lamps
    glUniform3fv(glGetUniformLocation(program, "lightDir"), 1, glm::value_ptr(glm::normalize(lightDir)));
    glUniform3fv(glGetUniformLocation(program, "lightColor"), 1, glm::value_ptr(lightColor));
    glm::vec3 ambient = glm::mix(glm::vec3(0.02f, 0.02f, 0.035f), glm::vec3(0.11f, 0.13f, 0.17f), dayFactor);
    glUniform3fv(glGetUniformLocation(program, "ambientColor"), 1, glm::value_ptr(ambient));
    std::vector<glm::vec3> lamps;
    for (const glm::vec3& base : LAMP_POSITIONS) lamps.push_back(base + glm::vec3(0.0f, LAMP_LIGHT_HEIGHT, 0.0f));
    glUniform1i(glGetUniformLocation(program, "numPointLights"), nightMode ? (GLint)lamps.size() : 0);
    glUniform3fv(glGetUniformLocation(program, "pointLightPos"), (GLsizei)lamps.size(), glm::value_ptr(lamps[0]));
    glUniform3fv(glGetUniformLocation(program, "pointLightColor"), 1, glm::value_ptr(LAMP_COLOR));
    // the shadows: the cascades are already bound by the main pass (bindShadowMaps), same uniforms
    glUniform1i(glGetUniformLocation(program, "shadowsEnabled"), shadowsEnabled);
    glUniform1i(glGetUniformLocation(program, "shadowMap"), SHADOW_MAP_TEXTURE_UNIT);
    glUniformMatrix4fv(glGetUniformLocation(program, "lightSpaceMatrices"), SHADOW_CASCADES, GL_FALSE,
        glm::value_ptr(lightSpaceMatrices[0]));
    glUniform1fv(glGetUniformLocation(program, "cascadeSplits"), SHADOW_CASCADES + 1, CASCADE_SPLITS);
    glActiveTexture(GL_TEXTURE0 + GRASS_MAP_TEXTURE_UNIT);
    glBindTexture(GL_TEXTURE_2D, grassMapTexture);
    glUniform1i(glGetUniformLocation(program, "grassMap"), GRASS_MAP_TEXTURE_UNIT);
    glActiveTexture(GL_TEXTURE0);
}

void renderGrass() {
    if (!grassEnabled || snowCover >= 0.99f) return;
    glDisable(GL_CULL_FACE);   // a blade / a tuft is seen from both sides

    grassShader.useShaderProgram();
    setGrassUniforms(grassShader.shaderProgram, GRASS_GRID, GRASS_SPACING);
    glBindVertexArray(grassVAO);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 7, GRASS_GRID * GRASS_GRID);

    if (tuftTexture != 0) {
        tuftShader.useShaderProgram();
        GLuint program = tuftShader.shaderProgram;
        setGrassUniforms(program, TUFT_GRID, TUFT_SPACING);
        glUniform1f(glGetUniformLocation(program, "bladeRadius"), 0.5f * GRASS_GRID * GRASS_SPACING);
        glUniform1i(glGetUniformLocation(program, "flowersEnabled"), flowersEnabled);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tuftTexture);
        glUniform1i(glGetUniformLocation(program, "tuftTexture"), 0);
        // the edges of the pictures: alpha to coverage (smooth with MSAA, no sorting needed), the alpha written is 1
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        glEnable(GL_SAMPLE_ALPHA_TO_ONE);
        glBindVertexArray(tuftVAO);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 12, TUFT_GRID * TUFT_GRID);
        glDisable(GL_SAMPLE_ALPHA_TO_ONE);
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
}
