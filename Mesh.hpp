#ifndef Mesh_hpp
#define Mesh_hpp

#if defined (__APPLE__)
    #define GL_SILENCE_DEPRECATION
    #include <OpenGL/gl3.h>
#else
    #define GLEW_STATIC
    #include <GL/glew.h>
#endif

#include <glm/glm.hpp>

#include "Shader.hpp"

#include <string>
#include <vector>


namespace gps {

    struct Vertex {

        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoords;
    };

    struct Texture {

        GLuint id;
        //ambientTexture, diffuseTexture, specularTexture
        std::string type;
        std::string path;
    };

    struct Material {

        glm::vec3 ambient;
        glm::vec3 diffuse;
        glm::vec3 specular;
    };

    struct Buffers {
        GLuint VAO;
        GLuint VBO;
        GLuint EBO;
    };

    class Mesh {

    public:
        std::vector<Vertex> vertices;
        std::vector<GLuint> indices;
        std::vector<Texture> textures;
        bool glass = false;   // material "Glass": the shader gives it reflections of the sky
        bool grass = false;   // grass ground ("GGrass..."): the 3D grass grows on it (Environment.cpp)
        int ground = 0;       // 1: a road (in the snow: cleared by the traffic, slush), 2: a sidewalk / path (trodden)
        bool paint = false;   // car body ("...Paint"): its color is given per vehicle (uniform paintColor)

	    Mesh(std::vector<Vertex> vertices, std::vector<GLuint> indices, std::vector<Texture> textures);

	    Buffers getBuffers();

	    void Draw(gps::Shader shader);

    private:
        /*  Render data  */
        Buffers buffers;

	    // Initializes all the buffer objects/arrays
	    void setupMesh();

    };

}
#endif /* Mesh_hpp */
