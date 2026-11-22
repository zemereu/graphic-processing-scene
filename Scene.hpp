// The objects of the scene: the models and where they stand, the parts that move (the revolving restaurant of
// Genex, the Ferris wheel, Palatul Telefoanelor moved from the keyboard), the trees near the tour path and the
// street lamps / parked cars of the town.
#pragma once

#include "App.hpp"

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
const int CAR_COLORS = 4;
extern gps::Model3D carModels[CAR_COLORS];
extern gps::Model3D carLights;   // head- and taillights, separate so they can glow at night

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
    int car;          // -1 = street lamp, otherwise index in carModels
    glm::mat4 model;
};
extern std::vector<CityProp> cityProps_;

// layout: the Romanian/Moldovan city center on the south bank (palace, Palatul Telefoanelor, Romanita),
// Genex as the western gate of the city on the main road, Motherland Calls on a hill above the river,
// Hotel Panorama in the mountains, and the Chornobyl exclusion zone in the forest beyond the gate
// (positions must match tools/gen_world.py, which flattens the ground under each building)
const glm::vec3 GENEX_POS(-420.0f, 0.0f, 196.0f);
const glm::vec3 RESTAURANT_OFFSET(-8.23f, 107.88f, 26.86f);  // from the base of Genex (axis of the restaurant)
const glm::vec3 PANORAMA_POS(-100.0f, 70.0f, -520.0f);
const glm::vec3 MOTHERLAND_POS(450.0f, 35.0f, -150.0f);
const glm::vec3 ROMANITA_POS(200.0f, 0.0f, 280.0f);
const glm::vec3 POWER_PLANT_POS(-780.0f, 0.0f, 70.0f);
const glm::vec3 DUGA_POS(-700.0f, 0.0f, -330.0f);
const glm::vec3 PRIPYAT_POS(-780.0f, 0.0f, 250.0f);
const glm::vec3 FERRIS_POS(-640.0f, 0.0f, 320.0f);
const float FERRIS_HUB_HEIGHT = 14.63f;   // must match ferris_support.obj / ferris_wheel.obj
const float FERRIS_RADIUS = 11.2f;
const int FERRIS_GONDOLAS = 20;

// animations
extern float restaurantAngle;
extern float ferrisAngle;

// keyboard-controlled object (Palatul Telefoanelor)
const glm::vec3 TELEFOANE_START_POS(-170.0f, 0.0f, 140.0f);
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
