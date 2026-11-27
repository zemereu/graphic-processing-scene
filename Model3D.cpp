#include "Model3D.hpp"
#include <map>
#include <tuple>
#include <glm/gtc/matrix_inverse.hpp>

namespace gps {
	// anisotropic filtering (EXT_texture_filter_anisotropic, core in OpenGL 4.6): keeps the ground textures sharp
	// at grazing angles instead of blurring them with the smaller mipmaps
	static const GLenum TEXTURE_MAX_ANISOTROPY = 0x84FE;
	static const GLenum MAX_TEXTURE_MAX_ANISOTROPY = 0x84FF;

	static void setTextureFiltering() {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		GLfloat maxAnisotropy = 1.0f;
		glGetFloatv(MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
		if (glGetError() == GL_NO_ERROR && maxAnisotropy > 1.0f) {
			glTexParameterf(GL_TEXTURE_2D, TEXTURE_MAX_ANISOTROPY, glm::min(maxAnisotropy, 8.0f));
		}
	}

	// loads an image into the video memory; srgb = color texture (decoded to linear by the GPU),
	// otherwise the values are used as they are (normal maps)
	static GLuint loadTextureImage(const char* file_name, bool srgb) {
		int x, y, n;
		unsigned char* image_data = stbi_load(file_name, &x, &y, &n, 4);
		if (!image_data) {
			fprintf(stderr, "ERROR: could not load %s\n", file_name);
			return 0;
		}
		// OpenGL expects the first row at the bottom: flip vertically
		int width_in_bytes = x * 4;
		for (int row = 0; row < y / 2; row++) {
			unsigned char* top = image_data + row * width_in_bytes;
			unsigned char* bottom = image_data + (y - row - 1) * width_in_bytes;
			for (int col = 0; col < width_in_bytes; col++) {
				std::swap(top[col], bottom[col]);
			}
		}
		GLuint textureID;
		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB_ALPHA : GL_RGBA8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
		glGenerateMipmap(GL_TEXTURE_2D);
		setTextureFiltering();
		glBindTexture(GL_TEXTURE_2D, 0);
		stbi_image_free(image_data);
		return textureID;
	}

	// normal map for the meshes that don't have one: a single texel pointing straight out of the surface,
	// so the shader can always apply normal mapping without changing anything
	static Texture flatNormalTexture() {
		static Texture flat = { 0, "normalTexture", "<flat normal>" };
		if (flat.id == 0) {
			unsigned char texel[4] = { 128, 128, 255, 255 };
			glGenTextures(1, &flat.id);
			glBindTexture(GL_TEXTURE_2D, flat.id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		return flat;
	}

	void Model3D::LoadModel(std::string fileName) {
		std::string basePath = fileName.substr(0, fileName.find_last_of('/')) + "/";
		ReadOBJ(fileName, basePath);
	}

	void Model3D::LoadModel(std::string fileName, std::string basePath) { ReadOBJ(fileName, basePath); }

	// Draw each mesh from the model
	void Model3D::Draw(const gps::Shader& shaderProgram) {
		for (int i = 0; i < meshes.size(); i++)
			meshes[i].Draw(shaderProgram);
	}

	// Does the parsing of the .obj file and fills in the data structure
	void Model3D::ReadOBJ(std::string fileName, std::string basePath) {
		std::cout << "Loading : " << fileName << std::endl;
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		int materialId;

		std::string err;
		bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, fileName.c_str(), basePath.c_str(), GL_TRUE);

		if (!err.empty()) {
			// `err` may contain warning message.
			std::cerr << err << std::endl;
		}

		if (!ret) { exit(1); }

		std::cout << "# of shapes    : " << shapes.size() << std::endl;
		std::cout << "# of materials : " << materials.size() << std::endl;

		// Loop over shapes
		for (size_t s = 0; s < shapes.size(); s++) {
			std::vector<gps::Vertex> vertices;
			std::vector<GLuint> indices;
			std::vector<gps::Texture> textures;

			// Loop over faces(polygon)
			size_t index_offset = 0;
			for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
				int fv = shapes[s].mesh.num_face_vertices[f];

				//gps::Texture currentTexture = LoadTexture("index1.png", "ambientTexture");
				//textures.push_back(currentTexture);

				// Loop over vertices in the face.
				for (size_t v = 0; v < fv; v++) {
					// access to vertex
					tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

					float vx = attrib.vertices[3 * idx.vertex_index + 0];
					float vy = attrib.vertices[3 * idx.vertex_index + 1];
					float vz = attrib.vertices[3 * idx.vertex_index + 2];
					float nx = attrib.normals[3 * idx.normal_index + 0];
					float ny = attrib.normals[3 * idx.normal_index + 1];
					float nz = attrib.normals[3 * idx.normal_index + 2];
					float tx = 0.0f;
					float ty = 0.0f;

					if (idx.texcoord_index != -1) {
						tx = attrib.texcoords[2 * idx.texcoord_index + 0];
						ty = attrib.texcoords[2 * idx.texcoord_index + 1];
					}

					glm::vec3 vertexPosition(vx, vy, vz);
					glm::vec3 vertexNormal(nx, ny, nz);
					glm::vec2 vertexTexCoords(tx, ty);

					gps::Vertex currentVertex;
					currentVertex.Position = vertexPosition;
					currentVertex.Normal = vertexNormal;
					currentVertex.TexCoords = vertexTexCoords;

					vertices.push_back(currentVertex);

					indices.push_back((GLuint)(index_offset + v));
				}

				index_offset += fv;
			}

			// get material id
			// Only try to read materials if the .mtl file is present
			size_t a = shapes[s].mesh.material_ids.size();

			if (a > 0 && materials.size() > 0) {
				materialId = shapes[s].mesh.material_ids[0];
				if (materialId != -1) {
					gps::Material currentMaterial;
					currentMaterial.ambient = glm::vec3(materials[materialId].ambient[0], materials[materialId].ambient[1], materials[materialId].ambient[2]);
					currentMaterial.diffuse = glm::vec3(materials[materialId].diffuse[0], materials[materialId].diffuse[1], materials[materialId].diffuse[2]);
					currentMaterial.specular = glm::vec3(materials[materialId].specular[0], materials[materialId].specular[1], materials[materialId].specular[2]);

					//ambient texture
					std::string ambientTexturePath = materials[materialId].ambient_texname;

					if (!ambientTexturePath.empty()) {
						gps::Texture currentTexture;
						currentTexture = LoadTexture(basePath + ambientTexturePath, "ambientTexture");
						textures.push_back(currentTexture);
					}

					//diffuse texture
					std::string diffuseTexturePath = materials[materialId].diffuse_texname;

					if (!diffuseTexturePath.empty()) {
						gps::Texture currentTexture;
						currentTexture = LoadTexture(basePath + diffuseTexturePath, "diffuseTexture");
						textures.push_back(currentTexture);
					}

					//specular texture
					std::string specularTexturePath = materials[materialId].specular_texname;

					if (!specularTexturePath.empty()) {
						gps::Texture currentTexture;
						currentTexture = LoadTexture(basePath + specularTexturePath, "specularTexture");
						textures.push_back(currentTexture);
					}

					//normal map ("map_bump" or "norm" in the .mtl file)
					std::string normalTexturePath = materials[materialId].bump_texname.empty()
						? materials[materialId].normal_texname : materials[materialId].bump_texname;

					if (!normalTexturePath.empty()) {
						textures.push_back(LoadTexture(basePath + normalTexturePath, "normalTexture"));
					}
				}
			}

			bool hasNormalMap = false;
			for (const Texture& t : textures) {
				hasNormalMap = hasNormalMap || t.type == "normalTexture";
			}
			if (!hasNormalMap) {
				textures.push_back(flatNormalTexture());
			}
			gps::Mesh mesh(vertices, indices, textures);
			// windows, shop windows, windscreens: every model names its glass material "Glass" / "glass"
			if (materialId >= 0 && materialId < (int)materials.size()) {
				std::string name = materials[materialId].name;
				for (char& c : name) c = (char)tolower(c);
				mesh.glass = name.find("glass") != std::string::npos;
				mesh.grass = name.find("grass") != std::string::npos;
				mesh.paint = name.size() >= 5 && name.compare(name.size() - 5, 5, "paint") == 0;   // "...Paint"
				if (name == "gasphalt" || name == "groad" || name == "roadpaint") mesh.ground = 1;
				else if (name == "pconcrete" || name == "psquare" || name == "phex" || name == "ppatterned" ||
				         name.compare(0, 6, "pcobbl") == 0) mesh.ground = 2;
			}
			meshes.push_back(mesh);
		}
	}

	// Retrieves a texture associated with the object - by its name and type
	gps::Texture Model3D::LoadTexture(std::string path, std::string type) {
		for (int i = 0; i < loadedTextures.size(); i++) {
			if (loadedTextures[i].path == path) {
				//already loaded texture
				return loadedTextures[i];
			}
		}

		gps::Texture currentTexture;
		currentTexture.id = type == "normalTexture" ? loadTextureImage(path.c_str(), false) : ReadTextureFromFile(path.c_str());
		currentTexture.type = std::string(type);
		currentTexture.path = path;

		loadedTextures.push_back(currentTexture);

		return currentTexture;
	}

	// Reads the pixel data from an image file and loads it into the video memory
	GLuint Model3D::ReadTextureFromFile(const char* file_name) {
		return loadTextureImage(file_name, true);   // sRGB colors + alpha channel (alpha test of the foliage)
	}

	// One box per mesh (in model coordinates), used for collision detection.
	// A building made of several meshes (tower, wing, ...) gets a tighter shape than a single box.
	void Model3D::BuildMerged(const std::vector<std::pair<const Model3D*, glm::mat4>>& copies, float tile) {
		// the groups: (tile x, tile z, source model, its mesh)
		std::map<std::tuple<int, int, const Model3D*, size_t>, std::pair<std::vector<Vertex>, std::vector<GLuint>>> groups;
		for (const auto& copy : copies) {
			const glm::mat4& m = copy.second;
			glm::mat3 nm = glm::mat3(glm::inverseTranspose(m));
			int tx = (int)floor(m[3].x / tile), tz = (int)floor(m[3].z / tile);
			for (size_t k = 0; k < copy.first->meshes.size(); k++) {
				const Mesh& src = copy.first->meshes[k];
				auto& g = groups[std::make_tuple(tx, tz, copy.first, k)];
				GLuint base = (GLuint)g.first.size();
				for (Vertex v : src.vertices) {
					v.Position = glm::vec3(m * glm::vec4(v.Position, 1.0f));
					v.Normal = glm::normalize(nm * v.Normal);
					g.first.push_back(v);
				}
				for (GLuint i : src.indices) g.second.push_back(base + i);
			}
		}
		for (auto& g : groups) {
			const Mesh& src = std::get<2>(g.first)->meshes[std::get<3>(g.first)];
			if (g.second.second.empty()) continue;
			Mesh mesh(g.second.first, g.second.second, src.textures);
			mesh.glass = src.glass;
			mesh.grass = src.grass;
			mesh.ground = src.ground;
			mesh.paint = src.paint;
			meshes.push_back(mesh);
		}
	}

	std::vector<BoundingBox> Model3D::GetMeshBounds() {

		std::vector<BoundingBox> bounds;
		for (size_t i = 0; i < meshes.size(); i++) {

			if (meshes[i].vertices.empty())
				continue;
			BoundingBox box = { meshes[i].vertices[0].Position, meshes[i].vertices[0].Position };
			for (const Vertex& v : meshes[i].vertices) {
				box.min = glm::min(box.min, v.Position);
				box.max = glm::max(box.max, v.Position);
			}
			bounds.push_back(box);
		}
		return bounds;
	}

	void Model3D::DrawMeshes(const gps::Shader& shaderProgram, const std::vector<bool>& visible) {
		for (size_t i = 0; i < meshes.size() && i < visible.size(); i++) {
			if (visible[i]) meshes[i].Draw(shaderProgram);
		}
	}

	Model3D::~Model3D() {

		for (size_t i = 0; i < loadedTextures.size(); i++) {
			glDeleteTextures(1, &loadedTextures.at(i).id);
		}

		for (size_t i = 0; i < meshes.size(); i++) {
			GLuint VBO = meshes.at(i).getBuffers().VBO;
			GLuint EBO = meshes.at(i).getBuffers().EBO;
			GLuint VAO = meshes.at(i).getBuffers().VAO;
			glDeleteBuffers(1, &VBO);
			glDeleteBuffers(1, &EBO);
			glDeleteVertexArrays(1, &VAO);
		}
	}
}
