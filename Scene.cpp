#include "Scene.hpp"
#include "Lighting.hpp"   // the street lamps and the floodlights of the layout
#include "Controls.hpp"   // the loading screen
#include "Environment.hpp"   // the wind (flags)


#include <fstream>
#include <algorithm>
#include <cfloat>
#include <random>
#include <sstream>

// ==============================================================================================================
// Scene
// ==============================================================================================================

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
const VehicleType VEHICLES[VEHICLE_TYPES] = {
    { "models/cars/car_red.obj", "models/cars/car_lights.obj", 4.35f, false, 3, true },
    { "models/cars/car_blue.obj", "models/cars/car_lights.obj", 4.35f, false, 3, true },
    { "models/cars/car_white.obj", "models/cars/car_lights.obj", 4.35f, false, 3, true },
    { "models/cars/car_yellow.obj", "models/cars/car_lights.obj", 4.35f, false, 3, true },
    { "models/vehicles/vaz2101/vaz2101.obj", "models/vehicles/vaz2101/vaz2101_lights.obj", 4.07f, true, 8, true },
    { "models/vehicles/vaz2103/vaz2103.obj", "models/vehicles/vaz2103/vaz2103_lights.obj", 4.12f, true, 6, true },
    { "models/vehicles/vaz2104/vaz2104.obj", "models/vehicles/vaz2104/vaz2104_lights.obj", 4.12f, true, 6, true },
    { "models/vehicles/vaz2105/vaz2105.obj", "models/vehicles/vaz2105/vaz2105_lights.obj", 4.13f, true, 8, true },
    { "models/vehicles/vaz2106/vaz2106.obj", "models/vehicles/vaz2106/vaz2106_lights.obj", 4.17f, true, 8, true },
    { "models/vehicles/vaz2107/vaz2107.obj", "models/vehicles/vaz2107/vaz2107_lights.obj", 4.15f, true, 8, true },
    { "models/vehicles/vaz2107_taxi/vaz2107_taxi.obj", "models/vehicles/vaz2107_taxi/vaz2107_taxi_lights.obj", 4.15f, false, 3, true },
    { "models/vehicles/moskvich2141/moskvich2141.obj", "models/vehicles/moskvich2141/moskvich2141_lights.obj", 4.35f, true, 7, true },
    { "models/vehicles/vis2345/vis2345.obj", "models/vehicles/vis2345/vis2345_lights.obj", 4.4f, true, 3, true },
    { "models/vehicles/gaz52/gaz52.obj", "models/vehicles/gaz52/gaz52_lights.obj", 6.4f, false, 3, false },
    { "models/vehicles/ural4320/ural4320.obj", "models/vehicles/ural4320/ural4320_lights.obj", 7.37f, false, 2, false },
    { "models/vehicles/laz699/laz699.obj", "models/vehicles/laz699/laz699_lights.obj", 10.2f, false, 3, false },
    // the trolleybus: the bus body, the poles on the roof drawn with it (trolleyPoles)
    { "models/vehicles/laz699/laz699.obj", "models/vehicles/laz699/laz699_lights.obj", 10.2f, false, 0, false },
};
gps::Model3D vehicleModels[VEHICLE_TYPES];
gps::Model3D vehicleLights[VEHICLE_TYPES];

// the usual car colors of the '70s-'80s (sRGB): white, beige, red, cherry, navy, sky blue, green, olive, mustard,
// orange, grey-blue, coffee, black
glm::vec3 vehiclePaint(unsigned int seed) {
    static const unsigned char colors[][3] = {
        { 235, 235, 228 }, { 214, 196, 150 }, { 160, 30, 25 }, { 100, 20, 30 }, { 30, 45, 90 }, { 110, 160, 200 },
        { 50, 110, 70 }, { 110, 115, 60 }, { 210, 160, 40 }, { 215, 100, 30 }, { 120, 140, 150 }, { 110, 70, 45 },
        { 25, 25, 28 } };
    const unsigned char* c = colors[(seed * 2654435761u >> 16) % (sizeof(colors) / sizeof(colors[0]))];
    return glm::pow(glm::vec3(c[0], c[1], c[2]) / 255.0f, glm::vec3(2.2f));
}
gps::Model3D motherland, romanita, powerPlant, duga, pripyatBlocks;
gps::Model3D motherlandLod, powerPlantLod, dugaLod, pripyatBlocksLod;
gps::Model3D ferrisSupport, ferrisWheel, ferrisGondola;
gps::Model3D trees;
gps::Model3D yardTrees;
std::vector<gps::BoundingBox> yardTreeBounds;
const char* TREE_SPECIES[] = { "fir", "pine", "noble", "dead", "dead2", "linden", "birch", "poplar" };
gps::Model3D treeModels[TREE_SPECIES_COUNT];
std::vector<NearTree> nearTrees;
gps::Model3D detailObjects, detailDecals;
std::vector<gps::BoundingBox> detailObjectBounds, detailDecalBounds;

// animations
float restaurantAngle = 0.0f;
const float RESTAURANT_SPEED = 6.0f;
const float FERRIS_SPEED = 4.0f;         // degrees per second
float ferrisAngle = 0.0f;

// keyboard-controlled object (Palatul Telefoanelor)
Placement satuMarePlace{ { 0.0f, 0.0f, 60.0f }, 0.0f };
Placement genexPlace{ { -420.0f, 0.0f, 196.0f }, 0.0f };
Placement panoramaPlace{ { -100.0f, 70.0f, -520.0f }, 0.0f };
Placement motherlandPlace{ { 450.0f, 35.0f, -150.0f }, 0.0f };
Placement romanitaPlace{ { 200.0f, 0.0f, 280.0f }, 0.0f };
Placement powerPlantPlace{ { -780.0f, 0.0f, 70.0f }, 0.0f };
Placement dugaPlace{ { -700.0f, 0.0f, -330.0f }, 0.0f };
Placement pripyatPlace{ { -780.0f, 0.0f, 250.0f }, 0.0f };
Placement ferrisPlace{ { -640.0f, 0.0f, 320.0f }, 0.0f };
Placement telefoanePlace{ { -170.0f, 0.0f, 140.0f }, 0.0f };
glm::vec3 telefoanePos = telefoanePlace.pos;
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
        bool wreck = kind == "wreck";   // an abandoned vehicle (the zone): that very type, rusty, sunk on one side
        if (kind == "car" || wreck) in >> car;
        float x, y, z, yaw;
        in >> x >> y >> z >> yaw;
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, y - (wreck ? 0.08f : 0.0f), z));
        m = glm::rotate(m, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
        // a parked car: one of the vehicles that park, chosen from its place (always the same)
        unsigned int seed = (unsigned int)(fabs(x) * 7.0f) * 73856093u ^ (unsigned int)(fabs(z) * 7.0f) * 19349663u;
        glm::vec3 paint = vehiclePaint(seed);
        if (wreck) {
            car = std::min(std::max(car, 0), VEHICLE_TYPES - 2);
            m = glm::rotate(m, glm::radians(3.0f), glm::normalize(glm::vec3(1.0f, 0.0f, 0.4f)));
            paint = glm::mix(paint * 0.5f, glm::vec3(0.33f, 0.2f, 0.12f), 0.7f);
        } else if (car >= 0) {
            std::vector<int> parking;
            for (int i = 0; i < VEHICLE_TYPES; i++) if (VEHICLES[i].parks) parking.push_back(i);
            car = parking[(seed >> 3) % parking.size()];
        }
        cityProps_.push_back({ car, m, paint });
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
    std::vector<std::pair<const gps::Model3D*, glm::mat4>> leafy;
    while (in >> name) {
        if (name[0] == '#') { std::getline(in, name); continue; }   // comment line
        float x, y, z, yaw, scale;
        in >> x >> y >> z >> yaw >> scale;
        for (int i = 0; i < TREE_SPECIES_COUNT; i++) {
            if (name == TREE_SPECIES[i]) {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
                m = glm::rotate(m, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
                m = glm::scale(m, glm::vec3(scale));
                if (i >= FIRST_LEAFY_TREE) leafy.push_back({ &treeModels[i], m });   // merged below
                else nearTrees.push_back({ i, m });
            }
        }
    }
    // the leafy trees (hundreds, tools/landmarks.py): copies of their models merged, one mesh per 150 m tile and
    // material, culled one by one like the details
    yardTrees.BuildMerged(leafy, 150.0f);
    yardTreeBounds = yardTrees.GetMeshBounds();
    return true;
}

glm::mat4 placed(const Placement& p) {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), p.pos);
    return glm::rotate(m, glm::radians(p.yaw), glm::vec3(0.0f, 1.0f, 0.0f));
}

// one line per object: <name> x y z yaw  |  lamp x y z  |  floodlight x y z  target_x target_y target_z
// a placement of layout.txt by its name (nullptr: not one of them)
static Placement* placementNamed(const std::string& name) {
    std::pair<const char*, Placement*> places[] = {
        { "satu_mare", &satuMarePlace }, { "genex", &genexPlace }, { "panorama", &panoramaPlace },
        { "motherland", &motherlandPlace }, { "romanita", &romanitaPlace }, { "power_plant", &powerPlantPlace },
        { "duga", &dugaPlace }, { "pripyat", &pripyatPlace }, { "ferris_wheel", &ferrisPlace },
        { "telefoane", &telefoanePlace } };
    for (auto& p : places) if (name == p.first) return p.second;
    return nullptr;
}

const char* FLAG_NAMES[FLAG_KINDS] = { "ussr", "mssr" };
gps::Model3D flagModels[FLAG_KINDS], flagpole;
std::vector<Flag> flags;

void loadFlags(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Could not open " << path << ": no flags" << std::endl;
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string word, name, on;
        Flag f;
        int pole = 0;
        if (!(ls >> word) || word != "flag") continue;
        if (!(ls >> name >> on >> f.top.x >> f.top.y >> f.top.z >> pole)) continue;
        f.kind = -1;
        for (int k = 0; k < FLAG_KINDS; k++) if (name == FLAG_NAMES[k]) f.kind = k;
        if (f.kind < 0) continue;
        f.on = on;
        f.pole = pole != 0;
        f.angle = -windDirection;
        flags.push_back(f);
    }
}

// where the top of the flag is, in the world
static glm::vec3 flagTop(const Flag& f) {
    Placement* p = placementNamed(f.on);
    glm::mat4 base = p ? placed(*p) : glm::mat4(1.0f);
    return glm::vec3(base * glm::vec4(f.top, 1.0f));
}

glm::mat4 flagMatrix(const Flag& f) {
    // the flag's x axis (from the pole) turned towards where the wind blows (world space)
    return glm::rotate(glm::translate(glm::mat4(1.0f), flagTop(f)), glm::radians(f.angle), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 flagpoleMatrix(const Flag& f) {
    return glm::translate(glm::mat4(1.0f), flagTop(f) - glm::vec3(0.0f, FLAG_POLE_TOP, 0.0f));
}

// the flags turn slowly round their poles, after the wind (and swing a little around it)
static void updateFlags() {
    float t = (float)glfwGetTime();
    for (size_t i = 0; i < flags.size(); i++) {
        float target = -windDirection + 12.0f * sin(t * 0.37f + 1.7f * (float)i);
        float diff = fmod(target - flags[i].angle + 540.0f, 360.0f) - 180.0f;   // the short way round
        flags[i].angle += diff * (1.0f - exp(-1.5f * deltaTime));
    }
}

void loadLayout(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Could not open " << path << ", the default layout is used" << std::endl;
        return;
    }
    std::pair<const char*, Placement*> places[] = {
        { "satu_mare", &satuMarePlace }, { "genex", &genexPlace }, { "panorama", &panoramaPlace },
        { "motherland", &motherlandPlace }, { "romanita", &romanitaPlace }, { "power_plant", &powerPlantPlace },
        { "duga", &dugaPlace }, { "pripyat", &pripyatPlace }, { "ferris_wheel", &ferrisPlace },
        { "telefoane", &telefoanePlace } };
    std::vector<glm::vec3> lamps;
    std::vector<SpotLight> spots;
    std::string name;
    while (in >> name) {
        if (name[0] == '#') { std::getline(in, name); continue; }   // comment line
        glm::vec3 p;
        in >> p.x >> p.y >> p.z;
        if (name == "lamp") {
            lamps.push_back(p);
        } else if (name == "floodlight") {
            glm::vec3 target;
            in >> target.x >> target.y >> target.z;
            spots.push_back({ p, target });
        } else {
            float yaw;
            in >> yaw;
            bool known = false;
            for (auto& place : places) {
                if (name == place.first) { *place.second = { p, yaw }; known = true; }
            }
            if (!known) std::cerr << path << ": unknown object " << name << std::endl;
        }
    }
    if (!lamps.empty()) LAMP_POSITIONS = lamps;
    if (!spots.empty()) SPOT_LIGHTS = spots;
    resetTelefoane();
}

const float LOADING_MODELS = 0.85f;   // the part of the loading screen's bar for the models

// the size of a file (for the progress bar: the time to load a model goes with its size)
static long fileSize(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);   // opened at the end: its position is the size
    return f ? (long)f.tellg() : 0;
}

void initModels() {
    loadLayout("models/layout.txt");
    // all the models to load, then loaded one by one with the loading screen in between
    std::vector<std::pair<gps::Model3D*, std::string>> jobs = {
        { &terrain, "models/terrain/terrain.obj" }, { &water, "models/terrain/water.obj" },
        { &satuMare, "models/satu_mare/satu_mare.obj" }, { &genex, "models/genex/genex.obj" },
        { &genexRestaurant, "models/genex/genex_restaurant.obj" }, { &genexLights, "models/genex/genex_lights.obj" },
        { &romanitaLights, "models/romanita/romanita_lights.obj" },
        { &telefoaneLights, "models/telefoane/telefoane_lights.obj" },
        { &satuMareLights, "models/satu_mare/satu_mare_lights.obj" }, { &panorama, "models/panorama/panorama.obj" },
        { &telefoane, "models/telefoane/telefoane.obj" }, { &lampPost, "models/lamp_post/lamp_post.obj" },
        { &lampBulb, "models/lamp_post/lamp_bulb.obj" }, { &floodlight, "models/floodlight/floodlight.obj" },
        { &floodlightLens, "models/floodlight/floodlight_lens.obj" },
        { &motherland, "models/motherland/motherland.obj" }, { &romanita, "models/romanita/romanita.obj" },
        { &powerPlant, "models/power_plant/power_plant.obj" }, { &duga, "models/duga/duga.obj" },
        { &pripyatBlocks, "models/pripyat/pripyat_blocks.obj" },
        { &motherlandLod, "models/motherland/motherland_lod.obj" },
        { &powerPlantLod, "models/power_plant/power_plant_lod.obj" }, { &dugaLod, "models/duga/duga_lod.obj" },
        { &pripyatBlocksLod, "models/pripyat/pripyat_blocks_lod.obj" },
        { &ferrisSupport, "models/ferris_wheel/ferris_support.obj" },
        { &ferrisWheel, "models/ferris_wheel/ferris_wheel.obj" },
        { &ferrisGondola, "models/ferris_wheel/ferris_gondola.obj" }, { &trees, "models/trees/trees.obj" },
        { &cityNorth, "models/city/city_north.obj" }, { &citySouth, "models/city/city_south.obj" },
        { &cityProps, "models/city/city_props.obj" }, { &cityLights, "models/city/city_lights.obj" },
        { &detailObjects, "models/details/objects.obj" }, { &detailDecals, "models/details/decals.obj" },
        { &flagpole, "models/flags/flagpole.obj" },
        { &trolleyWires, "models/trolleybus/wires.obj" }, { &trolleyPoles, "models/trolleybus/trolley_poles.obj" },
        { &pedestrianThigh, "models/pedestrians/thigh.obj" }, { &pedestrianShin, "models/pedestrians/shin.obj" },
        { &pedestrianArm, "models/pedestrians/arm.obj" },
    };
    for (int i = 0; i < PEDESTRIAN_KINDS; i++) {
        jobs.push_back({ &pedestrianBodies[i], std::string("models/pedestrians/body_") + PEDESTRIAN_KIND_NAMES[i] + ".obj" });
    }
    for (int i = 0; i < FLAG_KINDS; i++) {
        jobs.push_back({ &flagModels[i], std::string("models/flags/flag_") + FLAG_NAMES[i] + ".obj" });
    }
    for (int i = 0; i < VEHICLE_TYPES; i++) {
        jobs.push_back({ &vehicleModels[i], VEHICLES[i].model });
        jobs.push_back({ &vehicleLights[i], VEHICLES[i].lights });
    }
    for (int i = 0; i < TREE_SPECIES_COUNT; i++) {
        jobs.push_back({ &treeModels[i], std::string("models/trees/tree_") + TREE_SPECIES[i] + ".obj" });
    }
    double total = 0.0, done = 0.0;
    for (auto& job : jobs) total += (double)fileSize(job.second) + 1e5;   // + a little for the textures
    for (auto& job : jobs) {
        showLoading(LOADING_MODELS * (float)(done / total), job.second.c_str());
        job.first->LoadModel(job.second);
        done += (double)fileSize(job.second) + 1e5;
    }
    loadNearTrees("models/trees/trees_near.txt");
    loadCityProps("models/city/props.txt");
    loadFlags("models/flags/flags.txt");
    loadWalks("models/pedestrians/walks.txt");
    initPedestrians();
    detailObjectBounds = detailObjects.GetMeshBounds();

    detailDecalBounds = detailDecals.GetMeshBounds();
}

void updateAnimations() {
    updateFlags();
    restaurantAngle = fmod(restaurantAngle + RESTAURANT_SPEED * deltaTime, 360.0f);
    ferrisAngle = fmod(ferrisAngle + FERRIS_SPEED * deltaTime, 360.0f);
}

void resetTelefoane() {
    telefoanePos = telefoanePlace.pos;
    telefoaneAngle = telefoanePlace.yaw;
    telefoaneScale = 1.0f;
}

// model matrices, shared by rendering and collision detection
glm::mat4 satuMareMatrix() { return placed(satuMarePlace); }
glm::mat4 genexMatrix() { return placed(genexPlace); }
glm::mat4 panoramaMatrix() { return placed(panoramaPlace); }

glm::mat4 restaurantMatrix() {
    // rotate around its own axis first, then move it on top of the tower (T * R)
    glm::mat4 m = glm::translate(placed(genexPlace), RESTAURANT_OFFSET);
    return glm::rotate(m, glm::radians(restaurantAngle), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 ferrisWheelMatrix() {
    // the wheel turns around its horizontal axle (Z), at the height of the hub
    glm::mat4 m = glm::translate(placed(ferrisPlace), glm::vec3(0.0f, FERRIS_HUB_HEIGHT, 0.0f));
    return glm::rotate(m, glm::radians(ferrisAngle), glm::vec3(0.0f, 0.0f, 1.0f));
}

// the gondolas hang from the rim: they follow the wheel, but only by translation, so they stay upright
glm::mat4 ferrisGondolaMatrix(int i) {
    float angle = glm::radians(ferrisAngle) + glm::two_pi<float>() * i / FERRIS_GONDOLAS;
    glm::vec3 offset = glm::vec3(0.0f, FERRIS_HUB_HEIGHT, 0.0f) + FERRIS_RADIUS * glm::vec3(cos(angle), sin(angle), 0.0f);
    return glm::translate(placed(ferrisPlace), offset);
}

glm::mat4 telefoaneMatrix() {
    // T * R * S: scale and rotate around the building's own base, then move it into place
    glm::mat4 m = glm::translate(glm::mat4(1.0f), telefoanePos);
    m = glm::rotate(m, glm::radians(telefoaneAngle), glm::vec3(0.0f, 1.0f, 0.0f));
    return glm::scale(m, glm::vec3(telefoaneScale));
}

// ==============================================================================================================
// Traffic
// ==============================================================================================================

const float CAR_GAP = 2.5f;                       // meters between two cars that stand in a queue
bool trafficEnabled = true;
int trafficCars = 200;                            // how many cars drive on the roads
std::vector<TrafficLane> trafficLanes;
std::vector<Car> cars;
std::mt19937 trafficRng(7);

bool loadTraffic(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Cannot open " << path << std::endl;
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream words(line);
        std::string tag;
        words >> tag;
        if (tag == "lane") {
            int id, junction, n;
            float speed;
            words >> id >> junction >> speed >> n;
            if ((int)trafficLanes.size() <= id) trafficLanes.resize(id + 1);
            TrafficLane& lane = trafficLanes[id];
            lane.junction = junction;
            lane.speed = speed;
            for (int i = 0; i < n; i++) {
                glm::vec3 p;
                words >> p.x >> p.y >> p.z;
                lane.distance.push_back(lane.points.empty() ? 0.0f : lane.distance.back() + glm::distance(lane.points.back(), p));
                lane.points.push_back(p);
            }
            lane.length = lane.distance.back();
        } else if (tag == "next" || tag == "conf") {
            int id, k, other;
            words >> id >> k;
            std::vector<int>& list = tag == "next" ? trafficLanes[id].next : trafficLanes[id].conflicts;
            for (int i = 0; i < k; i++) {
                words >> other;
                list.push_back(other);
            }
        }
    }
    return !trafficLanes.empty();
}

std::vector<int> trolleyRoute;
std::vector<TrolleyStop> trolleyStops;
gps::Model3D trolleyWires, trolleyPoles;

void loadTrolleyRoute(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Could not open " << path << ": no trolleybus" << std::endl;
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string tag;
        ls >> tag;
        if (tag == "route") {
            int lane;
            while (ls >> lane) trolleyRoute.push_back(lane);
        } else if (tag == "stop") {
            TrolleyStop stop;
            if (ls >> stop.index >> stop.s) trolleyStops.push_back(stop);
        }
    }
    // a route whose lanes are not all in the traffic graph is not used
    for (int lane : trolleyRoute) {
        if (lane < 0 || lane >= (int)trafficLanes.size()) { trolleyRoute.clear(); trolleyStops.clear(); return; }
    }
}

int pickNextLane(int lane) {
    const std::vector<int>& next = trafficLanes[lane].next;
    std::uniform_int_distribution<int> pick(0, (int)next.size() - 1);
    return next[pick(trafficRng)];
}

// the cars are spread over the lanes (not the junctions), at least 12 m apart
void initTraffic() {
    cars.clear();
    for (TrafficLane& lane : trafficLanes) lane.occupants = 0;
    std::vector<int> roads;
    std::vector<float> weights;
    for (int i = 0; i < (int)trafficLanes.size(); i++) {
        if (trafficLanes[i].junction < 0 && trafficLanes[i].length > 20.0f) {
            roads.push_back(i);
            weights.push_back(trafficLanes[i].length);
        }
    }
    if (roads.empty()) return;
    std::discrete_distribution<int> pickLane(weights.begin(), weights.end());
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::vector<int> weights_;
    for (int i = 0; i < VEHICLE_TYPES; i++) weights_.push_back(VEHICLES[i].trafficWeight);
    std::discrete_distribution<int> pickType(weights_.begin(), weights_.end());
    for (int attempt = 0; attempt < trafficCars * 20 && (int)cars.size() < trafficCars; attempt++) {
        int lane = roads[pickLane(trafficRng)];
        int type = pickType(trafficRng);
        float length = VEHICLES[type].length;
        float s = 0.5f * length + unit(trafficRng) * (trafficLanes[lane].length - length);
        bool free = true;
        for (const Car& car : cars) {
            if (car.lane == lane && fabs(car.s - s) < 8.0f + 0.5f * (length + VEHICLES[car.model].length)) free = false;
        }
        if (!free) continue;
        float eagerness = 0.85f + 0.25f * unit(trafficRng);
        // the trucks and the buses are slower
        if (length > 6.0f) eagerness *= 0.8f;
        cars.push_back(Car{ lane, s, trafficLanes[lane].speed * eagerness, eagerness, pickNextLane(lane),
                            type, vehiclePaint((unsigned int)trafficRng()), 0.0f });
        trafficLanes[lane].occupants++;
    }
    // the trolleybuses: spread along their loop
    if (trolleyRoute.empty()) return;
    for (int b = 0; b < TROLLEYBUSES; b++) {
        int index = (int)(trolleyRoute.size() * b / TROLLEYBUSES);
        while (trafficLanes[trolleyRoute[index]].junction >= 0) index = (index + 1) % (int)trolleyRoute.size();
        int lane = trolleyRoute[index];
        Car bus{ lane, 0.5f * VEHICLES[TROLLEYBUS].length + 1.0f, 0.0f, 0.8f,
                 trolleyRoute[(index + 1) % trolleyRoute.size()], TROLLEYBUS, glm::vec3(1.0f), 0.0f };
        bus.route = index;
        // the first stop ahead of it
        bus.stop = 0;
        while (bus.stop < (int)trolleyStops.size() && trolleyStops[bus.stop].index < index) bus.stop++;
        if (bus.stop >= (int)trolleyStops.size()) bus.stop = 0;
        // no car right there
        for (int i = (int)cars.size() - 1; i >= 0; i--) {
            if (cars[i].lane == lane && fabs(cars[i].s - bus.s) < 14.0f) {
                trafficLanes[lane].occupants--;
                cars.erase(cars.begin() + i);
            }
        }
        cars.push_back(bus);
        trafficLanes[lane].occupants++;
    }
}

// a junction curve can be entered if nobody is on a curve that crosses it and there is room after it
bool junctionFree(int lane) {
    const TrafficLane& curve = trafficLanes[lane];
    for (int other : curve.conflicts) {
        if (trafficLanes[other].occupants > 0) return false;
    }
    return true;
}

void updateTraffic() {
    if (!trafficEnabled || trafficLanes.empty()) return;
    float dt = glm::min(deltaTime, 0.1f);
    // the cars of each lane, in the order they drive
    std::vector<std::vector<int>> onLane(trafficLanes.size());
    for (int i = 0; i < (int)cars.size(); i++) onLane[cars[i].lane].push_back(i);
    for (std::vector<int>& list : onLane) {
        std::sort(list.begin(), list.end(), [](int a, int b) { return cars[a].s < cars[b].s; });
    }
    // the vehicles are placed by their middle: half of their length in front of it and behind it
    auto half = [&](int i) { return 0.5f * VEHICLES[cars[i].model].length; };
    // the rear of the last car on a lane (from the lane's start); FLT_MAX if the lane is empty
    auto firstCar = [&](int lane) {
        return onLane[lane].empty() ? FLT_MAX : cars[onLane[lane].front()].s - half(onLane[lane].front());
    };
    for (int i = 0; i < (int)cars.size(); i++) {
        Car& car = cars[i];
        const TrafficLane& lane = trafficLanes[car.lane];
        float toEnd = lane.length - car.s;
        // the obstacle in front: the next car on this lane, or the last car on the next lane
        float gap = FLT_MAX;
        const std::vector<int>& here = onLane[car.lane];
        auto it = std::find(here.begin(), here.end(), i);
        if (it + 1 != here.end()) {
            gap = cars[*(it + 1)].s - half(*(it + 1)) - car.s - half(i);
        } else {
            gap = toEnd + firstCar(car.next) - half(i);
            // a junction curve: also the car after it, so nobody stops in the middle of the junction
            const TrafficLane& next = trafficLanes[car.next];
            if (next.junction >= 0 && !next.next.empty()) {
                gap = glm::min(gap, toEnd + next.length + firstCar(next.next[0]) - half(i));
                bool blocked = !junctionFree(car.next) ||
                               firstCar(next.next[0]) < 2.0f * half(i) + CAR_GAP;
                if (lane.junction < 0 && blocked && car.waited < 8.0f) gap = glm::min(gap, toEnd - half(i) + 1.0f);
            }
        }
        // a trolleybus stops at the stops of its line: the stop is an obstacle until it has waited there
        if (car.route >= 0 && !trolleyStops.empty()) {
            const TrolleyStop& stop = trolleyStops[car.stop];
            if (stop.index == car.route) {
                float toStop = stop.s - (car.s + half(i));
                if (toStop > -1.0f) {
                    gap = glm::min(gap, toStop + CAR_GAP);
                    if (toStop < 1.0f && car.speed < 0.3f) car.dwell += dt;
                    if (car.dwell > TROLLEY_DWELL) {
                        car.dwell = 0.0f;
                        car.stop = (car.stop + 1) % (int)trolleyStops.size();
                    }
                } else {
                    car.stop = (car.stop + 1) % (int)trolleyStops.size();   // passed it (it started behind it)
                }
            }
        }
        float target = glm::min(lane.speed * car.eagerness, sqrtf(glm::max(0.0f, 2.0f * 5.0f * (gap - CAR_GAP))));
        car.speed = car.speed < target ? glm::min(target, car.speed + 3.0f * dt) : glm::max(target, car.speed - 9.0f * dt);
        car.waited = (car.speed < 0.3f && toEnd - half(i) < 4.0f) ? car.waited + dt : 0.0f;
        car.s += car.speed * dt;
        // on into the next lane (possibly more than one if the lane is very short)
        while (car.s >= trafficLanes[car.lane].length) {
            car.s -= trafficLanes[car.lane].length;
            trafficLanes[car.lane].occupants--;
            car.lane = car.next;
            trafficLanes[car.lane].occupants++;
            if (car.route >= 0) {   // a trolleybus: the next lane of its loop
                car.route = (car.route + 1) % (int)trolleyRoute.size();
                car.next = trolleyRoute[(car.route + 1) % trolleyRoute.size()];
            } else {
                car.next = pickNextLane(car.lane);
            }
        }
    }
}

// position and orientation of a car on its lane (it faces +X in model space)
glm::mat4 carMatrix(const Car& car) {
    const TrafficLane& lane = trafficLanes[car.lane];
    int i = (int)(std::upper_bound(lane.distance.begin(), lane.distance.end(), car.s) - lane.distance.begin()) - 1;
    i = glm::clamp(i, 0, (int)lane.points.size() - 2);
    float t = glm::clamp((car.s - lane.distance[i]) / glm::max(lane.distance[i + 1] - lane.distance[i], 1e-4f), 0.0f, 1.0f);
    glm::vec3 position = glm::mix(lane.points[i], lane.points[i + 1], t);
    glm::vec3 forward = glm::normalize(lane.points[i + 1] - lane.points[i]);
    glm::vec3 side = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::cross(side, forward);
    glm::mat4 m(1.0f);
    m[0] = glm::vec4(forward, 0.0f);
    m[1] = glm::vec4(up, 0.0f);
    m[2] = glm::vec4(side, 0.0f);
    m[3] = glm::vec4(position, 1.0f);
    return m;
}

// ==============================================================================================================
// Pedestrians
// ==============================================================================================================

bool pedestriansEnabled = true;
const char* PEDESTRIAN_KIND_NAMES[PEDESTRIAN_KINDS] = { "man", "man_old", "man_cap", "woman", "woman_bun", "babushka" };
gps::Model3D pedestrianBodies[PEDESTRIAN_KINDS], pedestrianThigh, pedestrianShin, pedestrianArm;
std::vector<Walk> walks;
std::vector<Pedestrian> pedestrians;
static std::mt19937 pedestrianRng(11);
const float STRIDE = 1.4f;   // meters per two steps (one swing of the legs back and forth)

void loadWalks(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Could not open " << path << ": no pedestrians" << std::endl;
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string tag;
        int n = 0;
        if (!(ls >> tag >> n) || tag != "walk") continue;
        Walk w;
        for (int i = 0; i < n; i++) {
            glm::vec3 p;
            ls >> p.x >> p.y >> p.z;
            w.distance.push_back(w.points.empty() ? 0.0f : w.distance.back() + glm::distance(w.points.back(), p));
            w.points.push_back(p);
        }
        if (w.points.size() >= 2) {
            w.length = w.distance.back();
            walks.push_back(w);
        }
    }
}

void initPedestrians() {
    pedestrians.clear();
    if (walks.empty()) return;
    // muted coats and jackets of the '80s, dark trousers (sRGB)
    static const unsigned char coats[][3] = {
        { 90, 70, 55 }, { 60, 65, 75 }, { 120, 40, 35 }, { 45, 55, 45 }, { 150, 130, 100 }, { 70, 80, 110 },
        { 160, 150, 140 }, { 35, 35, 38 }, { 110, 90, 50 }, { 130, 60, 80 }, { 80, 100, 90 }, { 180, 170, 150 } };
    static const unsigned char trousers[][3] = {
        { 30, 32, 40 }, { 45, 45, 48 }, { 60, 50, 40 }, { 25, 25, 27 }, { 70, 72, 78 }, { 40, 45, 70 } };
    // under a dress or a coat: stockings
    static const unsigned char stockings[][3] = { { 200, 160, 135 }, { 185, 150, 125 }, { 40, 36, 36 }, { 70, 60, 55 } };
    auto linear = [](const unsigned char* c) { return glm::pow(glm::vec3(c[0], c[1], c[2]) / 255.0f, glm::vec3(2.2f)); };
    std::vector<float> weights;
    for (const Walk& w : walks) weights.push_back(w.length);
    std::discrete_distribution<int> pickWalk(weights.begin(), weights.end());
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    for (int i = 0; i < PEDESTRIANS; i++) {
        Pedestrian p;
        p.walk = pickWalk(pedestrianRng);
        p.s = unit(pedestrianRng) * walks[p.walk].length;
        p.dir = unit(pedestrianRng) < 0.5f ? 1.0f : -1.0f;
        p.speed = 1.1f + 0.45f * unit(pedestrianRng);
        p.side = 0.35f + 0.45f * unit(pedestrianRng);
        p.phase = unit(pedestrianRng) * glm::two_pi<float>();
        p.pause = 0.0f;
        p.scale = 0.9f + 0.18f * unit(pedestrianRng);
        p.coat = linear(coats[pedestrianRng() % (sizeof(coats) / sizeof(coats[0]))]);
        p.kind = (int)(pedestrianRng() % PEDESTRIAN_KINDS);
        bool woman = p.kind >= 3;
        p.trousers = woman ? linear(stockings[pedestrianRng() % (sizeof(stockings) / sizeof(stockings[0]))])
                           : linear(trousers[pedestrianRng() % (sizeof(trousers) / sizeof(trousers[0]))]);
        if (woman) p.scale *= 0.95f;
        pedestrians.push_back(p);
    }
}

void updatePedestrians() {
    if (!pedestriansEnabled) return;
    float dt = glm::min(deltaTime, 0.1f);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    for (Pedestrian& p : pedestrians) {
        if (p.pause > 0.0f) {
            p.pause -= dt;
            continue;
        }
        const Walk& w = walks[p.walk];
        p.s += p.dir * p.speed * dt;
        p.phase = fmod(p.phase + p.speed * dt / STRIDE * glm::two_pi<float>(), glm::two_pi<float>());
        if (p.s > w.length || p.s < 0.0f) {   // the end of the way: a moment there, then back
            p.s = glm::clamp(p.s, 0.0f, w.length);
            p.dir = -p.dir;
            p.pause = 0.5f + 3.5f * unit(pedestrianRng);
        }
    }
}

PedestrianPose pedestrianPose(const Pedestrian& p) {
    const Walk& w = walks[p.walk];
    int i = (int)(std::upper_bound(w.distance.begin(), w.distance.end(), p.s) - w.distance.begin()) - 1;
    i = glm::clamp(i, 0, (int)w.points.size() - 2);
    float t = glm::clamp((p.s - w.distance[i]) / glm::max(w.distance[i + 1] - w.distance[i], 1e-4f), 0.0f, 1.0f);
    glm::vec3 position = glm::mix(w.points[i], w.points[i + 1], t);
    glm::vec3 along = w.points[i + 1] - w.points[i];
    along.y = 0.0f;
    glm::vec3 forward = glm::normalize(along) * p.dir;
    glm::vec3 side = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));   // the right-hand side
    position += side * p.side;
    // the steps: the legs swing, the arms the other way; standing: still
    float moving = p.pause > 0.0f ? 0.0f : 1.0f;
    float swing = glm::radians(22.0f) * sin(p.phase) * moving;
    position.y += 0.02f * fabs(cos(p.phase)) * moving;   // a little up at each step
    glm::mat4 m(1.0f);
    m[0] = glm::vec4(forward, 0.0f);
    m[1] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
    m[2] = glm::vec4(side, 0.0f);
    m[3] = glm::vec4(position, 1.0f);
    m = glm::scale(m, glm::vec3(p.scale));
    PedestrianPose pose;
    const glm::vec3 z(0.0f, 0.0f, 1.0f);
    pose.body = glm::rotate(m, glm::radians(-2.5f) * moving, z);   // leaning forward a little as one walks
    // the joints (tools/pedestrians.py): the hips, the knees 0.44 m lower, the shoulders
    for (int k = 0; k < 2; k++) {
        float sign = k == 0 ? 1.0f : -1.0f;
        float hip = sign * swing;
        // the knee bends while the leg swings forward (the foot off the ground), nearly straight while it carries
        float forward = glm::max(0.0f, sign * (float)cos(p.phase)) * moving;
        float knee = glm::radians(4.0f + 50.0f * forward * forward);
        pose.thighs[k] = glm::rotate(glm::translate(m, glm::vec3(0.0f, 0.92f, sign * 0.095f)), hip, z);
        pose.shins[k] = glm::rotate(glm::translate(pose.thighs[k], glm::vec3(0.0f, -0.44f, 0.0f)), -knee, z);
        pose.arms[k] = glm::rotate(glm::rotate(glm::translate(pose.body, glm::vec3(0.0f, 1.42f, sign * 0.2f)),
                                               -0.9f * hip, z), glm::radians(-6.0f) * sign, glm::vec3(1, 0, 0));
    }
    return pose;
}
