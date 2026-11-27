#ifndef Model3D_hpp
#define Model3D_hpp

#include "Mesh.hpp"

#include "tiny_obj_loader.h"
#include "stb_image.h"

#include <iostream>
#include <string>
#include <vector>

namespace gps {

    // axis-aligned bounding box
    struct BoundingBox {
        glm::vec3 min;
        glm::vec3 max;
    };

    class Model3D {

    public:
        ~Model3D();

		void LoadModel(std::string fileName);

		void LoadModel(std::string fileName, std::string basePath);

		void Draw(const gps::Shader& shaderProgram);
		// draws only the meshes marked visible (the town's details are culled mesh by mesh)
		void DrawMeshes(const gps::Shader& shaderProgram, const std::vector<bool>& visible);

		// one box per mesh (in model coordinates), used for collision detection
		std::vector<BoundingBox> GetMeshBounds();

		// this model made of copies of other models (each with its matrix), merged: one mesh per tile (tile x tile
		// meters, by the copy's position) and per mesh of the source; the textures stay the sources'
		void BuildMerged(const std::vector<std::pair<const Model3D*, glm::mat4>>& copies, float tile);

    private:
		// Component meshes - group of objects
        std::vector<gps::Mesh> meshes;
		// Associated textures
        std::vector<gps::Texture> loadedTextures;

		// Does the parsing of the .obj file and fills in the data structure
		void ReadOBJ(std::string fileName, std::string basePath);

		// Retrieves a texture associated with the object - by its name and type
		gps::Texture LoadTexture(std::string path, std::string type);

		// Reads the pixel data from an image file and loads it into the video memory
		GLuint ReadTextureFromFile(const char* file_name);
    };
}

#endif /* Model3D_hpp */
