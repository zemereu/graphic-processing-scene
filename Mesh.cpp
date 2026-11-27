#include "Mesh.hpp"
namespace gps {
	/* Mesh Constructor */
	Mesh::Mesh(std::vector<Vertex> vertices, std::vector<GLuint> indices, std::vector<Texture> textures) {
		this->vertices = vertices;
		this->indices = indices;
		this->textures = textures;

		this->setupMesh();
	}

	Buffers Mesh::getBuffers() {
	    return this->buffers;
	}

	/* Mesh drawing function - also applies associated textures */
	void Mesh::Draw(const gps::Shader& shader)	{
		GLuint program = shader.shaderProgram;
		glUseProgram(program);

		//set textures
		for (GLuint i = 0; i < textures.size(); i++) {
			glActiveTexture(GL_TEXTURE0 + i);
			glUniform1i(Shader::location(program, this->textures[i].type), i);
			glBindTexture(GL_TEXTURE_2D, this->textures[i].id);
		}

		// the mesh's flags: sent only when they differ from what this program already has (most meshes: all 0)
		struct Flags { int glass = -1, grass = -1, paint = -1, ground = -1; };
		static std::unordered_map<GLuint, Flags> sent;
		Flags& last = sent[program];
		if (last.glass != (int)glass) { glUniform1i(Shader::location(program, "glass"), glass); last.glass = glass; }
		if (last.grass != (int)grass) { glUniform1i(Shader::location(program, "grass"), grass); last.grass = grass; }
		if (last.paint != (int)paint) { glUniform1i(Shader::location(program, "paint"), paint); last.paint = paint; }
		if (last.ground != (int)ground) { glUniform1i(Shader::location(program, "groundUse"), ground); last.ground = ground; }
		glBindVertexArray(this->buffers.VAO);
		glDrawElements(GL_TRIANGLES, (GLsizei)this->indices.size(), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);

        for(GLuint i = 0; i < this->textures.size(); i++) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

    }

	// Initializes all the buffer objects/arrays
	void Mesh::setupMesh() {
		// Create buffers/arrays
		glGenVertexArrays(1, &this->buffers.VAO);
		glGenBuffers(1, &this->buffers.VBO);
		glGenBuffers(1, &this->buffers.EBO);

		glBindVertexArray(this->buffers.VAO);
		// Load data into vertex buffers
		glBindBuffer(GL_ARRAY_BUFFER, this->buffers.VBO);
		glBufferData(GL_ARRAY_BUFFER, this->vertices.size() * sizeof(Vertex), &this->vertices[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->buffers.EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indices.size() * sizeof(GLuint), &this->indices[0], GL_STATIC_DRAW);

		// Set the vertex attribute pointers
		// Vertex Positions
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
		// Vertex Normals
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, Normal));
		// Vertex Texture Coords
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, TexCoords));

		glBindVertexArray(0);
	}
}
