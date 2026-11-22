#include "Scene.hpp"

#include <fstream>

// models
gps::Model3D terrain;
gps::Model3D water;     // river and cooling pond, drawn with moving waves
gps::Model3D satuMare;
gps::Model3D genex;
gps::Model3D genexRestaurant;
gps::Model3D genexLights;
gps::Model3D romanitaLights, telefoaneLights, satuMareLights;   // lit windows, drawn (glowing) only at night      // lit windows of Genex, drawn (glowing) only at night
gps::Model3D panorama;
gps::Model3D telefoane;
gps::Model3D floodlight;
gps::Model3D floodlightLens;   // only the lenses glow at night
gps::Model3D lampPost;
gps::Model3D lampBulb;
gps::Model3D cityNorth, citySouth, cityProps, cityLights;
std::vector<CityProp> cityProps_;
gps::Model3D carModels[CAR_COLORS];
gps::Model3D carLights;
gps::Model3D motherland, romanita, powerPlant, duga, pripyatBlocks;
gps::Model3D motherlandLod, powerPlantLod, dugaLod, pripyatBlocksLod;
gps::Model3D ferrisSupport, ferrisWheel, ferrisGondola;
gps::Model3D trees;
const char* TREE_SPECIES[] = { "fir", "pine", "noble", "dead", "dead2" };
gps::Model3D treeModels[TREE_SPECIES_COUNT];
std::vector<NearTree> nearTrees;

// animations
float restaurantAngle = 0.0f;
const float RESTAURANT_SPEED = 6.0f;
const float FERRIS_SPEED = 4.0f;         // degrees per second
float ferrisAngle = 0.0f;

// keyboard-controlled object (Palatul Telefoanelor)
glm::vec3 telefoanePos = TELEFOANE_START_POS;
float telefoaneAngle = 0.0f;   // degrees, around its own vertical axis
float telefoaneScale = 1.0f;   // uniform scale

// street lamps and parked cars of the town: "lamp x y z yaw" or "car <color> x y z yaw"
bool loadCityProps(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Could not open " << path << std::endl;
        return false;
    }
    std::string kind;
    while (in >> kind) {
        if (kind[0] == '#') { std::getline(in, kind); continue; }   // comment line
        int car = -1;
        if (kind == "car") in >> car;
        float x, y, z, yaw;
        in >> x >> y >> z >> yaw;
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
        cityProps_.push_back({ car, glm::rotate(m, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f)) });
    }
    return true;
}

// one line per tree: species x y z yaw(degrees) scale
bool loadNearTrees(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Could not open " << path << std::endl;
        return false;
    }
    std::string name;
    while (in >> name) {
        if (name[0] == '#') { std::getline(in, name); continue; }   // comment line
        float x, y, z, yaw, scale;
        in >> x >> y >> z >> yaw >> scale;
        for (int i = 0; i < TREE_SPECIES_COUNT; i++) {
            if (name == TREE_SPECIES[i]) {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
                m = glm::rotate(m, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
                nearTrees.push_back({ i, glm::scale(m, glm::vec3(scale)) });
            }
        }
    }
    return true;
}

void initModels() {
    terrain.LoadModel("models/terrain/terrain.obj");
    water.LoadModel("models/terrain/water.obj");
    satuMare.LoadModel("models/satu_mare/satu_mare.obj");
    genex.LoadModel("models/genex/genex.obj");
    genexRestaurant.LoadModel("models/genex/genex_restaurant.obj");
    genexLights.LoadModel("models/genex/genex_lights.obj");
    romanitaLights.LoadModel("models/romanita/romanita_lights.obj");
    telefoaneLights.LoadModel("models/telefoane/telefoane_lights.obj");
    satuMareLights.LoadModel("models/satu_mare/satu_mare_lights.obj");
    panorama.LoadModel("models/panorama/panorama.obj");
    telefoane.LoadModel("models/telefoane/telefoane.obj");
    lampPost.LoadModel("models/lamp_post/lamp_post.obj");
    lampBulb.LoadModel("models/lamp_post/lamp_bulb.obj");
    floodlight.LoadModel("models/floodlight/floodlight.obj");
    floodlightLens.LoadModel("models/floodlight/floodlight_lens.obj");
    const char* carFiles[CAR_COLORS] = {
        "models/cars/car_red.obj", "models/cars/car_blue.obj",
        "models/cars/car_white.obj", "models/cars/car_yellow.obj" };
    for (int i = 0; i < CAR_COLORS; i++) {
        carModels[i].LoadModel(carFiles[i]);
    }
    carLights.LoadModel("models/cars/car_lights.obj");
    motherland.LoadModel("models/motherland/motherland.obj");
    romanita.LoadModel("models/romanita/romanita.obj");
    powerPlant.LoadModel("models/power_plant/power_plant.obj");
    duga.LoadModel("models/duga/duga.obj");
    pripyatBlocks.LoadModel("models/pripyat/pripyat_blocks.obj");
    motherlandLod.LoadModel("models/motherland/motherland_lod.obj");
    powerPlantLod.LoadModel("models/power_plant/power_plant_lod.obj");
    dugaLod.LoadModel("models/duga/duga_lod.obj");
    pripyatBlocksLod.LoadModel("models/pripyat/pripyat_blocks_lod.obj");
    ferrisSupport.LoadModel("models/ferris_wheel/ferris_support.obj");
    ferrisWheel.LoadModel("models/ferris_wheel/ferris_wheel.obj");
    ferrisGondola.LoadModel("models/ferris_wheel/ferris_gondola.obj");
    trees.LoadModel("models/trees/trees.obj");
    for (int i = 0; i < TREE_SPECIES_COUNT; i++) {
        treeModels[i].LoadModel(std::string("models/trees/tree_") + TREE_SPECIES[i] + ".obj");
    }
    loadNearTrees("models/trees/trees_near.txt");
    cityNorth.LoadModel("models/city/city_north.obj");
    citySouth.LoadModel("models/city/city_south.obj");
    cityProps.LoadModel("models/city/city_props.obj");
    cityLights.LoadModel("models/city/city_lights.obj");
    loadCityProps("models/city/props.txt");
}

void updateAnimations() {
    restaurantAngle = fmod(restaurantAngle + RESTAURANT_SPEED * deltaTime, 360.0f);
    ferrisAngle = fmod(ferrisAngle + FERRIS_SPEED * deltaTime, 360.0f);
}

void resetTelefoane() {
    telefoanePos = TELEFOANE_START_POS;
    telefoaneAngle = 0.0f;
    telefoaneScale = 1.0f;
}

// model matrices, shared by rendering and collision detection
glm::mat4 satuMareMatrix() { return glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 60.0f)); }
glm::mat4 genexMatrix() { return glm::translate(glm::mat4(1.0f), GENEX_POS); }
glm::mat4 panoramaMatrix() { return glm::translate(glm::mat4(1.0f), PANORAMA_POS); }

glm::mat4 restaurantMatrix() {
    // rotate around its own axis first, then move it on top of the tower (T * R)
    glm::mat4 m = glm::translate(glm::mat4(1.0f), GENEX_POS + RESTAURANT_OFFSET);
    return glm::rotate(m, glm::radians(restaurantAngle), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 ferrisWheelMatrix() {
    // the wheel turns around its horizontal axle (Z), at the height of the hub
    glm::mat4 m = glm::translate(glm::mat4(1.0f), FERRIS_POS + glm::vec3(0.0f, FERRIS_HUB_HEIGHT, 0.0f));
    return glm::rotate(m, glm::radians(ferrisAngle), glm::vec3(0.0f, 0.0f, 1.0f));
}

// the gondolas hang from the rim: they follow the wheel, but only by translation, so they stay upright
glm::mat4 ferrisGondolaMatrix(int i) {
    float angle = glm::radians(ferrisAngle) + glm::two_pi<float>() * i / FERRIS_GONDOLAS;
    glm::vec3 hub = FERRIS_POS + glm::vec3(0.0f, FERRIS_HUB_HEIGHT, 0.0f);
    return glm::translate(glm::mat4(1.0f), hub + FERRIS_RADIUS * glm::vec3(cos(angle), sin(angle), 0.0f));
}

glm::mat4 telefoaneMatrix() {
    // T * R * S: scale and rotate around the building's own base, then move it into place
    glm::mat4 m = glm::translate(glm::mat4(1.0f), telefoanePos);
    m = glm::rotate(m, glm::radians(telefoaneAngle), glm::vec3(0.0f, 1.0f, 0.0f));
    return glm::scale(m, glm::vec3(telefoaneScale));
}
