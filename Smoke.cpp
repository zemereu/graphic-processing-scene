#include "Smoke.hpp"
#include "Scene.hpp"      // POWER_PLANT_POS
#include "Lighting.hpp"   // sun / moon light, fog
#include "Rain.hpp"       // wind
#include "Renderer.hpp"   // applyViewMode

#include <random>

bool smokeEnabled = true;
gps::Shader smokeShader;
GLuint smokeVAO = 0, smokeQuadVBO = 0, smokeSeedVBO = 0;
int smokeMaxParticles = 0;

// positions in the power plant model (models/power_plant), moved to where the plant stands
const std::vector<SmokeEmitter> SMOKE_EMITTERS = {
    // the ventilation stack (60 m): a thin grey plume
    { POWER_PLANT_POS + glm::vec3(-89.0f, 60.5f, -4.0f), 1.5f, 300, 25.0f, 4.0f, 3.0f, 26.0f,
      glm::vec3(0.62f, 0.62f, 0.64f), 0.35f },
    // the cooling tower (72 m, 26 m wide at the top): broad white steam
    { POWER_PLANT_POS + glm::vec3(225.0f, 72.0f, -15.0f), 18.0f, 400, 26.0f, 2.5f, 14.0f, 42.0f,
      glm::vec3(0.95f, 0.95f, 0.97f), 0.2f },
};

void initSmoke() {
    smokeShader.loadShader("shaders/smoke.vert", "shaders/smoke.frag");

    // one quad (triangle strip), drawn once per particle (instancing); each instance has its own random seed
    const float corners[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    for (const SmokeEmitter& e : SMOKE_EMITTERS) smokeMaxParticles = glm::max(smokeMaxParticles, e.particles);
    std::mt19937 rng(11);
    std::uniform_real_distribution<float> random01(0.0f, 1.0f);
    std::vector<glm::vec4> seeds;
    for (int i = 0; i < smokeMaxParticles; i++) {
        seeds.push_back(glm::vec4(random01(rng), random01(rng), random01(rng), random01(rng)));
    }

    glGenVertexArrays(1, &smokeVAO);
    glBindVertexArray(smokeVAO);
    glGenBuffers(1, &smokeQuadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, smokeQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glGenBuffers(1, &smokeSeedVBO);
    glBindBuffer(GL_ARRAY_BUFFER, smokeSeedVBO);
    glBufferData(GL_ARRAY_BUFFER, seeds.size() * sizeof(glm::vec4), seeds.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);   // one seed per particle, not per vertex
    glBindVertexArray(0);
}

void renderSmoke() {
    if (!smokeEnabled) return;
    smokeShader.useShaderProgram();
    GLuint program = smokeShader.shaderProgram;
    glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1f(glGetUniformLocation(program, "time"), (float)glfwGetTime());
    glm::vec2 wind = windSpeed * glm::vec2(cos(glm::radians(windDirection)), sin(glm::radians(windDirection)));
    glUniform2fv(glGetUniformLocation(program, "wind"), 1, glm::value_ptr(wind));
    glUniform3fv(glGetUniformLocation(program, "lightColor"), 1, glm::value_ptr(lightColor));
    glUniform1f(glGetUniformLocation(program, "dayFactor"), dayFactor);
    glUniform1f(glGetUniformLocation(program, "fogDensity"), fogEnabled ? fogDensity : 0.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);   // hidden by the buildings in front, but they don't hide each other
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(smokeVAO);
    for (const SmokeEmitter& e : SMOKE_EMITTERS) {
        glUniform3fv(glGetUniformLocation(program, "source"), 1, glm::value_ptr(e.position));
        glUniform1f(glGetUniformLocation(program, "sourceRadius"), e.radius);
        glUniform1f(glGetUniformLocation(program, "lifetime"), e.lifetime);
        glUniform1f(glGetUniformLocation(program, "riseSpeed"), e.riseSpeed);
        glUniform1f(glGetUniformLocation(program, "startSize"), e.startSize);
        glUniform1f(glGetUniformLocation(program, "endSize"), e.endSize);
        glUniform3fv(glGetUniformLocation(program, "smokeColor"), 1, glm::value_ptr(e.color));
        glUniform1f(glGetUniformLocation(program, "opacity"), e.opacity);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, glm::min(e.particles, smokeMaxParticles));
    }
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    applyViewMode();
}
