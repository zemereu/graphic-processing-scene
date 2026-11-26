// The scene: the models and where they stand, the moving objects, the trees and the town's props, and the
// traffic on the roads.
#pragma once

#include "App.hpp"

// ==============================================================================================================
// Scene
//
// The objects of the scene: the models and where they stand, the parts that move (the revolving restaurant of
// Genex, the Ferris wheel, Palatul Telefoanelor moved from the keyboard), the trees near the tour path and the
// street lamps / parked cars of the town.
// ==============================================================================================================

// models
extern gps::Model3D terrain;
extern gps::Model3D water;     // river and cooling pond, drawn with moving waves
extern gps::Model3D satuMare;
extern gps::Model3D genex;
extern gps::Model3D genexRestaurant;
extern gps::Model3D genexLights;
extern gps::Model3D romanitaLights, telefoaneLights, satuMareLights;   // lit windows, drawn (glowing) only at night
extern gps::Model3D panorama;
extern gps::Model3D telefoane;
extern gps::Model3D floodlight;
extern gps::Model3D floodlightLens;   // only the lenses glow at night
extern gps::Model3D lampPost;
extern gps::Model3D lampBulb;
// the town around the landmarks (tools/city_layout.py): ordinary buildings north / south of the highway, and the street
// furniture, bridges, fences (no colliders); street lamps that only glow and parked cars (models/city/props.txt)
extern gps::Model3D cityNorth, citySouth, cityProps, cityLights;
// the vehicles of the traffic and the parked ones: the Dacia 1300 (models/cars, 4 colors) and the downloaded ones
// (models/vehicles/<name>, tools/vehicles.py). Each has a separate model with its head- and taillights (they glow at
// night); the "painted" ones get a body color per vehicle (materials named "...Paint", Mesh.paint)
struct VehicleType {
    const char* model;
    const char* lights;
    float length;        // meters (the traffic keeps its distances with it)
    bool painted;
    int trafficWeight;   // how often it appears on the roads, relative to the others
    bool parks;          // may be one of the parked cars of the town
};
const int VEHICLE_TYPES = 17;
const int TROLLEYBUS = 16;   // the last type: only on the trolleybus line (never chosen at random)
extern const VehicleType VEHICLES[VEHICLE_TYPES];
extern gps::Model3D vehicleModels[VEHICLE_TYPES];
extern gps::Model3D vehicleLights[VEHICLE_TYPES];
glm::vec3 vehiclePaint(unsigned int seed);   // one of the usual car colors of the '70s-'80s (linear RGB)
const float VEHICLE_DISTANCE = 600.0f;          // meters: further away the vehicles are not drawn

// more landmarks: Motherland Calls (Volgograd), Romanita (Chisinau), Chornobyl (power plant, Duga radar,
// Pripyat apartment blocks and the Ferris wheel from the Pripyat amusement park)
extern gps::Model3D motherland, romanita, powerPlant, duga, pripyatBlocks;
// level of detail (course 09 - Models, mesh simplification): simplified copies of the heaviest models,
// drawn instead of the full ones when the camera is far away (the colliders still use the full models)
extern gps::Model3D motherlandLod, powerPlantLod, dugaLod, pripyatBlocksLod;
extern gps::Model3D ferrisSupport, ferrisWheel, ferrisGondola;
extern gps::Model3D trees;   // distant forest: crossed billboard quads, all in one model
// trees near the tour path: one full 3D model per species, drawn at every position from trees_near.txt
const int TREE_SPECIES_COUNT = 5;
extern gps::Model3D treeModels[TREE_SPECIES_COUNT];
struct NearTree {
    int species;
    glm::mat4 model;
};
extern std::vector<NearTree> nearTrees;
struct CityProp {
    int car;          // -1 = street lamp, otherwise index in VEHICLES
    glm::mat4 model;
    glm::vec3 paint;  // body color of a painted vehicle
};
extern std::vector<CityProp> cityProps_;
// procedural details of the town (tools/details.py): street furniture, vegetation, details on the roofs and walls, on
// the ground; one mesh per material and 150 m tile, culled mesh by mesh (Renderer: renderDetails)
extern gps::Model3D detailObjects, detailDecals;
extern std::vector<gps::BoundingBox> detailObjectBounds, detailDecalBounds;   // per mesh, computed once
const float DETAIL_DISTANCE = 350.0f;          // meters: small things further away are not drawn
const float DETAIL_SHADOW_DISTANCE = 160.0f;   // ... nor cast shadows
const float DECAL_DISTANCE = 140.0f;           // the flat ones on the ground

// layout: the Romanian/Moldovan city center on the south bank (palace, Palatul Telefoanelor, Romanita),
// Genex as the western gate of the city on the main road, Motherland Calls on a hill above the river,
// Hotel Panorama in the mountains, and the Chornobyl exclusion zone in the forest beyond the gate
// Where they stand: models/layout.txt (read at start; tools/blender_import.py and blender_export.py edit it in
// Blender), the values below if the file is missing. The ground is flattened under each building where it stood
// first (tools/gen_world.py): a building moved far stands on the uneven ground.
struct Placement {
    glm::vec3 pos;
    float yaw;   // degrees, around the vertical axis
};
extern Placement satuMarePlace, genexPlace, panoramaPlace, motherlandPlace, romanitaPlace, powerPlantPlace,
                 dugaPlace, pripyatPlace, ferrisPlace, telefoanePlace;
glm::mat4 placed(const Placement& p);   // T * R
void loadLayout(const char* path);      // also the street lamps and the floodlights (Lighting)

// flags (tools/flags.py, models/flags/flags.txt): the cloth is bent by the wind in the vertex shader (basic.vert:
// flagCloth); each flag turns slowly round its pole to stream away from the wind
const int FLAG_KINDS = 2;
extern const char* FLAG_NAMES[FLAG_KINDS];   // ussr, mssr
extern gps::Model3D flagModels[FLAG_KINDS], flagpole;
struct Flag {
    int kind;
    std::string on;   // the model it stands on (its placement), or "world"
    glm::vec3 top;    // the top of the flag at the pole (in that model's space)
    bool pole;        // draw a pole too
    float angle;      // degrees around the pole, now
};
extern std::vector<Flag> flags;
void loadFlags(const char* path);
glm::mat4 flagMatrix(const Flag& f);
glm::mat4 flagpoleMatrix(const Flag& f);
const float FLAG_POLE_TOP = 9.6f;   // the top of a flag on a pole, from the ground (flagpole.obj)

const glm::vec3 RESTAURANT_OFFSET(-8.23f, 107.88f, 26.86f);  // from the base of Genex (axis of the restaurant)
const float FERRIS_HUB_HEIGHT = 14.63f;   // must match ferris_support.obj / ferris_wheel.obj
const float FERRIS_RADIUS = 11.2f;
const int FERRIS_GONDOLAS = 20;

// animations
extern float restaurantAngle;
extern float ferrisAngle;

// keyboard-controlled object (Palatul Telefoanelor; it starts at telefoanePlace)
extern glm::vec3 telefoanePos;
extern float telefoaneAngle;   // degrees, around its own vertical axis
extern float telefoaneScale;   // uniform scale

void initModels();
void updateAnimations();
void resetTelefoane();

// model matrices, shared by rendering and collision detection
glm::mat4 satuMareMatrix();
glm::mat4 genexMatrix();
glm::mat4 panoramaMatrix();
glm::mat4 restaurantMatrix();
glm::mat4 ferrisWheelMatrix();
glm::mat4 ferrisGondolaMatrix(int i);   // the gondolas stay upright
glm::mat4 telefoaneMatrix();

// ==============================================================================================================
// Traffic
//
// Traffic on all the roads (models/city/traffic.txt, written by tools/traffic_net.py): every road is split into
// lanes (right-hand traffic) and the lanes are joined through the junctions, the roundabout, the bridge and the
// turning places by short curves. A car follows its lane, keeps its distance to the car in front and at the end of
// the lane goes on into one of the next lanes, chosen at random; it waits in front of a junction while another car
// is on a curve that crosses its own.
// ==============================================================================================================

struct TrafficLane {
    std::vector<glm::vec3> points;
    std::vector<float> distance;    // distance along the lane at each point
    float length = 0.0f;
    float speed = 10.0f;            // speed limit, m/s
    int junction = -1;              // >= 0: a curve through a junction
    std::vector<int> next;          // the lanes a car can go on into
    std::vector<int> conflicts;     // the curves of the same junction that cross this one
    int occupants = 0;              // cars on this lane now
};
struct Car {
    int lane;          // the lane the car is on
    float s;           // distance along the lane
    float speed;       // m/s
    float eagerness;   // multiplies the speed limit
    int next;          // the lane after this one (chosen when the car enters the lane)
    int model;         // index in VEHICLES
    glm::vec3 paint;   // body color (of the painted vehicles)
    float waited;      // seconds spent waiting in front of a junction
    int route = -1;    // >= 0: a trolleybus, its index in trolleyRoute (the lane it is on)
    int stop = 0;      // the next stop of the line it will stop at
    float dwell = 0.0f;   // seconds at the stop so far
};
extern bool trafficEnabled;
extern int trafficCars;                            // how many cars drive on the roads
extern std::vector<TrafficLane> trafficLanes;
extern std::vector<Car> cars;

bool loadTraffic(const char* path);

// the trolleybus line (tools/trolleybus.py, models/trolleybus/route.txt): a loop of lanes under the overhead wires;
// the trolleybuses drive in the traffic like the cars but keep to the loop and stop at its stops
struct TrolleyStop {
    int index;   // in trolleyRoute
    float s;     // along that lane: where the front of the bus stops
};
extern std::vector<int> trolleyRoute;
extern std::vector<TrolleyStop> trolleyStops;
extern gps::Model3D trolleyWires, trolleyPoles;   // the wires and their poles; the poles on the roof of the bus
const int TROLLEYBUSES = 3;
const float TROLLEY_DWELL = 12.0f;   // seconds at each stop
void loadTrolleyRoute(const char* path);

// ==============================================================================================================
// Pedestrians
//
// People walking on the sidewalks and the footpaths (tools/pedestrians.py: models/pedestrians/walks.txt): each one
// walks a way to its end, stops for a moment and walks back, on the right-hand side; the body is drawn in parts
// (body, legs, arms) and the legs and the arms swing (turned around the hip / the shoulder) with the steps.
// ==============================================================================================================

struct Walk {
    std::vector<glm::vec3> points;
    std::vector<float> distance;   // along the way, at each point
    float length = 0.0f;
};
struct Pedestrian {
    int walk;
    float s;          // along the way
    float dir;        // +1 / -1
    float speed;      // m/s
    float side;       // meters to the right of the middle of the way
    float phase;      // of the steps (radians)
    float pause;      // seconds still standing at the end of the way
    float scale;      // height
    glm::vec3 coat, trousers;   // colors (linear RGB)
};
struct PedestrianPose {
    glm::mat4 body, legs[2], arms[2];
};
extern bool pedestriansEnabled;
extern gps::Model3D pedestrianBody, pedestrianLeg, pedestrianArm;
extern std::vector<Walk> walks;
extern std::vector<Pedestrian> pedestrians;
const int PEDESTRIANS = 150;
const float PEDESTRIAN_DISTANCE = 140.0f;   // meters: not drawn further away (tiny)

void loadWalks(const char* path);
void initPedestrians();
void updatePedestrians();
PedestrianPose pedestrianPose(const Pedestrian& p);
void initTraffic();     // spreads the cars over the lanes (again, when their number changes)
void updateTraffic();   // every frame
glm::mat4 carMatrix(const Car& car);
