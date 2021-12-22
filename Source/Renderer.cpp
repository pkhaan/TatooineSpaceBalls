#include "Renderer.h"
#include "GeometryNode.h"
#include "Tools.h"
#include "ShaderProgram.h"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "OBJLoader.h"

#include <algorithm>
#include <array>
#include <iostream>
#include "SDL2/SDL.h"
#include <SDL2/SDL_image.h>

// #define TEXTURE_TEST

// RENDERER
Renderer::Renderer()
{
	this->m_nodes = {};
	this->m_continous_time = 0.0;
}

Renderer::~Renderer()
{
	glDeleteTextures(1, &m_fbo_texture);
	glDeleteFramebuffers(1, &m_fbo);

	glDeleteVertexArrays(1, &m_vao_fbo);
	glDeleteBuffers(1, &m_vbo_fbo_vertices);
}

bool Renderer::Init(int SCREEN_WIDTH, int SCREEN_HEIGHT)
{
	this->m_screen_width = SCREEN_WIDTH;
	this->m_screen_height = SCREEN_HEIGHT;

	bool techniques_initialization = InitShaders();

#ifdef TEXTURE_TEST
	bool meshes_initialization = InitCommonMeshes();
#else
	bool meshes_initialization = InitGeometricMeshes();
	this->BuildWorld();
#endif

	bool common_initialization = InitCommonItems();
	bool inter_buffers_initialization = InitIntermediateBuffers();

	//If there was any errors
	if (Tools::CheckGLError() != GL_NO_ERROR)
	{
		printf("Exiting with error at Renderer::Init\n");
		return false;
	}

	this->InitCamera();

	//If everything initialized
	return techniques_initialization && meshes_initialization &&
		common_initialization && inter_buffers_initialization;
}

void Renderer::BuildWorld()
{
	// GeometryNode& bunny = *this->m_nodes[OBJECS::BUNNY];
	// GeometryNode& ball = *this->m_nodes[OBJECS::BALL];
	// GeometryNode& chair = *this->m_nodes[OBJECS::CHAIR];
	// GeometryNode& floor = *this->m_nodes[OBJECS::FLOOR];

	// ball.model_matrix = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 35.f, 0.f));
	// ball.m_aabb.center = glm::vec3(ball.model_matrix * glm::vec4(ball.m_aabb.center, 1.f));

	// chair.model_matrix = glm::mat4(1.f);
	// floor.model_matrix = glm::mat4(1.f);

	// Set static background image
	SDL_Surface* background = IMG_Load("Assets/terrain/CanopusGround.png");
	unsigned char* data = background->pixels;

	GLuint texHandle;
	glGenTextures(1, &texHandle);
	glBindTexture(GL_TEXTURE_2D, texHandle);

	SDL_LockSurface(background);
	glTexImage2D(GL_TEXTURE_2D, // target
		0, // level-of-detail
		GL_RGBA32F, // internal format / number of components
		background->w, background->h, // size
		0, // border (must be zero)
		GL_BGRA, // format
		GL_UNSIGNED_BYTE, // type
		data); // data buffer
	SDL_UnlockSurface(background);
	glGenerateMipmap(GL_TEXTURE_2D); // generate mipmap if needed
	glBindTexture(GL_TEXTURE_2D, 0);

	GeometryNode& terrain = *this->m_nodes[OBJECS::TERRAIN];
	GeometryNode& craft = *this->m_nodes[OBJECS::CRAFT];

	// Scale everything down
	this->m_world_matrix = glm::scale(glm::mat4(1.f), glm::vec3(0.02, 0.02, 0.2));
}

void Renderer::InitCamera()
{
#ifdef TEXTURE_TEST
	this->m_camera_position = glm::vec3(0, -0.7, 6);
	this->m_camera_target_position = glm::vec3(0, 0, 0);
	this->m_camera_up_vector = glm::vec3(0, 1, 0);

	m_projection_matrix = glm::perspective(glm::radians(50.f), m_screen_width / (float)m_screen_height, 0.1f, 1500.f);
	m_view_matrix = glm::lookAt(this->m_camera_position, this->m_camera_target_position, glm::vec3(0, 1, 0));
#else
	this->m_camera_position = glm::vec3(2, 2, 4.5);
	this->m_camera_target_position = glm::vec3(0, 0, 0);
	this->m_camera_up_vector = glm::vec3(0, 1, 0);

	this->m_view_matrix = glm::lookAt(
		this->m_camera_position,
		this->m_camera_target_position,
		m_camera_up_vector);

	this->m_projection_matrix = glm::perspective(
		glm::radians(45.f),
		this->m_screen_width / (float)this->m_screen_height,
		0.1f, 1000.f);
#endif
}

bool Renderer::InitShaders()
{
	std::string vertex_shader_path = "Assets/Shaders/basic_rendering.vert";
	std::string fragment_shader_path = "Assets/Shaders/basic_rendering.frag";

	this->m_geometry_rendering_program.LoadVertexShaderFromFile(vertex_shader_path.c_str());
	this->m_geometry_rendering_program.LoadFragmentShaderFromFile(fragment_shader_path.c_str());
	this->m_geometry_rendering_program.CreateProgram();
	this->m_geometry_rendering_program.LoadUniform("uniform_projection_matrix");
	this->m_geometry_rendering_program.LoadUniform("uniform_normal_matrix");
	this->m_geometry_rendering_program.LoadUniform("uniform_diffuse");
	this->m_geometry_rendering_program.LoadUniform("uniform_has_texture");
	this->m_geometry_rendering_program.LoadUniform("uniform_texture");

	vertex_shader_path = "Assets/Shaders/post_process.vert";
	fragment_shader_path = "Assets/Shaders/post_process.frag";

	this->m_post_rendering_program.LoadVertexShaderFromFile(vertex_shader_path.c_str());
	this->m_post_rendering_program.LoadFragmentShaderFromFile(fragment_shader_path.c_str());
	this->m_post_rendering_program.CreateProgram();
	this->m_post_rendering_program.LoadUniform("uniform_texture");

	return true;
}

bool Renderer::InitIntermediateBuffers()
{
	glGenTextures(1, &m_fbo_texture);
	glGenTextures(1, &m_fbo_depth_texture);
	glGenFramebuffers(1, &m_fbo);

	return ResizeBuffers(m_screen_width, m_screen_height);
}

bool Renderer::ResizeBuffers(int width, int height)
{
	m_screen_width = width;
	m_screen_height = height;

	// texture
	glBindTexture(GL_TEXTURE_2D, m_fbo_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_screen_width, m_screen_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	glBindTexture(GL_TEXTURE_2D, m_fbo_depth_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_screen_width, m_screen_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);

	// framebuffer to link to everything together
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fbo_texture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_fbo_depth_texture, 0);

	GLenum status = Tools::CheckFramebufferStatus(m_fbo);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		return false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return true;
}

bool Renderer::InitCommonItems()
{
	glGenVertexArrays(1, &m_vao_fbo);
	glBindVertexArray(m_vao_fbo);

	GLfloat fbo_vertices[] = { -1, -1, 1, -1, -1, 1, 1, 1, };

	glGenBuffers(1, &m_vbo_fbo_vertices);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_fbo_vertices);
	glBufferData(GL_ARRAY_BUFFER, sizeof(fbo_vertices), fbo_vertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

	glBindVertexArray(0);
	return true;
}

bool Renderer::InitCommonMeshes()
{
	std::vector<glm::vec3> vertices {
		glm::vec3(-1, -1, 0),
		glm::vec3(1, -1, 0),
		glm::vec3(-1, 1, 0),
		glm::vec3(1, 1, 0)
	};

	std::vector<glm::vec3> normals {
		glm::vec3(0, 1, 0),
		glm::vec3(0, 1, 0),
		glm::vec3(0, 1, 0),
		glm::vec3(0, 1, 0)
	};

	std::vector<glm::vec2> uvs {
		glm::vec2(0, 0),
		glm::vec2(1, 0),
		glm::vec2(0, 1),
		glm::vec2(1, 1)
	};

	std::string texFile = "Assets/terrain/terrain_BaseMap.png";

	GLint max_anisotropy = 1;
	glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);
	
	GeometryNode* node1 = new GeometryNode();
	node1->Init(vertices, normals, uvs, texFile);
	node1->model_matrix =
		glm::translate(glm::mat4(1.f), glm::vec3(-2, 0, -2)) *
		glm::rotate(glm::mat4(1.f), glm::radians(-80.f), glm::vec3(1, 0, 0)) *
		glm::scale(glm::mat4(1.f), glm::vec3(1, 10, 1));
	
	glBindTexture(GL_TEXTURE_2D, node1->parts[0].textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	this->m_opaque_nodes.push_back(node1);

	GeometryNode* node2 = new GeometryNode();
	node2->Init(vertices, normals, uvs, texFile);
	node2->model_matrix = glm::translate(glm::mat4(1.f), glm::vec3(-2, 2.2, -12));

	glBindTexture(GL_TEXTURE_2D, node2->parts[0].textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	this->m_opaque_nodes.push_back(node2);

	GeometryNode* node3 = new GeometryNode();
	node3->Init(vertices, normals, uvs, texFile);
	node3->model_matrix =
		glm::translate(glm::mat4(1.f), glm::vec3(0, 0, -2)) *
		glm::rotate(glm::mat4(1.f), glm::radians(-80.f), glm::vec3(1, 0, 0)) *
		glm::scale(glm::mat4(1.f), glm::vec3(1, 10, 1));
	
	glBindTexture(GL_TEXTURE_2D, node3->parts[0].textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	this->m_opaque_nodes.push_back(node3);

	GeometryNode* node4 = new GeometryNode();
	node4->Init(vertices, normals, uvs, texFile);
	node4->model_matrix = glm::translate(glm::mat4(1.f), glm::vec3(0, 2.2, -12));

	glBindTexture(GL_TEXTURE_2D, node4->parts[0].textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	this->m_opaque_nodes.push_back(node4);

	GeometryNode* node5 = new GeometryNode();
	node5->Init(vertices, normals, uvs, texFile, true);
	node5->model_matrix =
		glm::translate(glm::mat4(1.f), glm::vec3(2, 0, -2)) *
		glm::rotate(glm::mat4(1.f), glm::radians(-80.f), glm::vec3(1, 0, 0)) *
		glm::scale(glm::mat4(1.f), glm::vec3(1, 10, 1));

	glBindTexture(GL_TEXTURE_2D, node5->parts[0].textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	this->m_opaque_nodes.push_back(node5);

	GeometryNode* node6 = new GeometryNode();
	node6->Init(vertices, normals, uvs, texFile, true);
	node6->model_matrix = glm::translate(glm::mat4(1.f), glm::vec3(2, 2.2, -12));

	glBindTexture(GL_TEXTURE_2D, node6->parts[0].textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	this->m_opaque_nodes.push_back(node6);

	GeometryNode* node7 = new GeometryNode();
	node7->Init(vertices, normals, uvs, texFile, true);
	node7->model_matrix =
		glm::translate(glm::mat4(1.f), glm::vec3(4, 0, -2)) *
		glm::rotate(glm::mat4(1.f), glm::radians(-80.f), glm::vec3(1, 0, 0)) *
		glm::scale(glm::mat4(1.f), glm::vec3(1, 10, 1));

	glBindTexture(GL_TEXTURE_2D, node7->parts[0].textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropy);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	this->m_opaque_nodes.push_back(node7);

	GeometryNode* node8 = new GeometryNode();
	node8->Init(vertices, normals, uvs, texFile, true);
	node8->model_matrix = glm::translate(glm::mat4(1.f), glm::vec3(4, 2.2, -12));

	glBindTexture(GL_TEXTURE_2D, node8->parts[0].textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropy);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	this->m_opaque_nodes.push_back(node8);

	/*
	std::string grass = "Assets/test_scene/Grass.png";
	GeometryNode* node9 = new GeometryNode();
	node9->Init(vertices, normals, uvs, grass);
	node9->model_matrix =
		glm::translate(glm::mat4(1.f), glm::vec3(0, 0, 1)) *
		glm::scale(glm::mat4(1.f), glm::vec3(0.5, 0.5, 0.5));

	glBindTexture(GL_TEXTURE_2D, node9->parts[0].textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	this->m_transparent_nodes.push_back(node9);*/

	return true;
}

bool Renderer::InitGeometricMeshes()
{
	std::array<const char*, OBJECS::SIZE_ALL> assets = {
		"Assets/terrain/collision_hull.obj",
		"Assets/terrain/terrain.obj",
		"Assets/craft/craft.obj"
	};

	bool initialized = true;
	OBJLoader loader;

	for (auto & asset : assets)
	{
		if (asset != NULL)
		{
			GeometricMesh* mesh = loader.load(asset);

			if (mesh != nullptr)
			{
				GeometryNode* node = new GeometryNode();
				node->Init(mesh);
				this->m_nodes.push_back(node);
				delete mesh;
			}
			else
			{
				initialized = false;
			}
		}
	}

	return initialized;
}

void Renderer::Update(float dt)
{
#ifndef TEXTURE_TEST
	this->UpdateGeometry(dt);
#endif

	this->UpdateCamera(dt);
	m_continous_time += dt;
}

void Renderer::UpdateGeometry(float dt)
{
	GeometryNode& terrain = *this->m_nodes[OBJECS::TERRAIN];
	GeometryNode& craft = *this->m_nodes[OBJECS::CRAFT];
	// GeometryNode& chair = *this->m_nodes[OBJECS::CHAIR];
	// GeometryNode& floor = *this->m_nodes[OBJECS::FLOOR];

	// bunny.app_model_matrix =
	//	glm::translate(glm::mat4(1.f), bunny.m_aabb.center) *
	//	glm::rotate(glm::mat4(1.f), m_continous_time, glm::vec3(0.f, 1.f, 0.f)) *
	//	glm::translate(glm::mat4(1.f), -bunny.m_aabb.center) *
	//	bunny.model_matrix;

	// ball.app_model_matrix =
	//	glm::translate(glm::mat4(1.f), ball.m_aabb.center) *
	//	glm::rotate(glm::mat4(1.f), m_continous_time, glm::vec3(.5f, .5f, 0.f)) *
	//	glm::translate(glm::mat4(1.f), -ball.m_aabb.center) *
	//	ball.model_matrix;
}

void Renderer::UpdateCamera(float dt)
{
	glm::vec3 direction = glm::normalize(m_camera_target_position - m_camera_position);

	m_camera_position = m_camera_position + (m_camera_movement.x * 5.f * dt) * direction;
	m_camera_target_position = m_camera_target_position + (m_camera_movement.x * 5.f * dt) * direction;

	glm::vec3 right = glm::normalize(glm::cross(direction, m_camera_up_vector));

	m_camera_position = m_camera_position + (m_camera_movement.y * 5.f * dt) * right;
	m_camera_target_position = m_camera_target_position + (m_camera_movement.y * 5.f * dt) * right;

	float speed = glm::pi<float>() * 0.002;
	glm::mat4 rotation = glm::rotate(glm::mat4(1.f), m_camera_look_angle_destination.y * speed, right);
	rotation *= glm::rotate(glm::mat4(1.f), m_camera_look_angle_destination.x * speed, m_camera_up_vector);
	m_camera_look_angle_destination = glm::vec2(0.f);

	direction = rotation * glm::vec4(direction, 0.f);
	m_camera_target_position = m_camera_position + direction * glm::distance(m_camera_position, m_camera_target_position);

	m_view_matrix = glm::lookAt(m_camera_position, m_camera_target_position, m_camera_up_vector);
}

bool Renderer::ReloadShaders()
{
	m_geometry_rendering_program.ReloadProgram();
	m_post_rendering_program.ReloadProgram();
	return true;
}

void Renderer::Render()
{
#ifdef TEXTURE_TEST
	RenderCommonMeshes();
#else
	RenderGeometry();
#endif

	GLenum error = Tools::CheckGLError();

	if (error != GL_NO_ERROR)
	{
		printf("Reanderer:Draw GL Error\n");
		system("pause");
	}
}

void Renderer::RenderCommonMeshes()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, m_screen_width, m_screen_height);
	glClearColor(0.f, 0.8f, 1.f, 1.f);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LEQUAL);
	glClearDepth(1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	this->m_geometry_rendering_program.Bind();

	glm::mat4 proj = m_projection_matrix * m_view_matrix;

	for (auto& node : this->m_opaque_nodes)
	{
		glBindVertexArray(node->m_vao);

		glUniformMatrix4fv(m_geometry_rendering_program["uniform_projection_matrix"], 1, GL_FALSE,
			glm::value_ptr(proj * node->model_matrix));

		for (int j = 0; j < node->parts.size(); ++j)
		{
			glUniform1i(m_geometry_rendering_program["uniform_has_texture"],
				(node->parts[j].textureID > 0) ? 1 : 0);

			glActiveTexture(GL_TEXTURE0);
			glUniform1i(m_geometry_rendering_program["uniform_texture"], 0);
			glBindTexture(GL_TEXTURE_2D, node->parts[j].textureID);
			glDrawArrays(GL_TRIANGLE_STRIP, node->parts[j].start_offset, node->parts[j].count);
		}

		glBindVertexArray(0);
	}

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	for (auto& node : this->m_transparent_nodes)
	{
		glBindVertexArray(node->m_vao);

		glUniformMatrix4fv(m_geometry_rendering_program["uniform_projection_matrix"], 1, GL_FALSE,
			glm::value_ptr(proj * node->model_matrix));

		for (int j = 0; j < node->parts.size(); ++j)
		{
			glUniform1i(m_geometry_rendering_program["uniform_has_texture"],
				(node->parts[j].textureID > 0) ? 1 : 0);

			glActiveTexture(GL_TEXTURE0);
			glUniform1i(m_geometry_rendering_program["uniform_texture"], 0);
			glBindTexture(GL_TEXTURE_2D, node->parts[j].textureID);
			glDrawArrays(GL_TRIANGLE_STRIP, node->parts[j].start_offset, node->parts[j].count);
		}

		glBindVertexArray(0);
	}

	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	this->m_geometry_rendering_program.Unbind();
}

void Renderer::RenderGeometry()
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	GLenum drawbuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, drawbuffers);

	glViewport(0, 0, m_screen_width, m_screen_height);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glClearDepth(1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	this->m_geometry_rendering_program.Bind();

	glm::mat4 proj = m_projection_matrix * m_view_matrix * m_world_matrix;

	for (auto& node : this->m_nodes)
	{
		glBindVertexArray(node->m_vao);

		glUniformMatrix4fv(m_geometry_rendering_program["uniform_projection_matrix"], 1, GL_FALSE,
			glm::value_ptr(proj * node->app_model_matrix));

		glm::mat4 normal_matrix = glm::transpose(glm::inverse(m_world_matrix * node->app_model_matrix));

		glUniformMatrix4fv(m_geometry_rendering_program["uniform_normal_matrix"], 1, GL_FALSE,
			glm::value_ptr(normal_matrix));

		for (int j = 0; j < node->parts.size(); ++j)
		{
			glm::vec3 diffuse = node->parts[j].diffuseColor;

			glUniform3f(m_geometry_rendering_program["uniform_diffuse"],
				diffuse.x, diffuse.y, diffuse.z);

			glUniform1i(m_geometry_rendering_program["uniform_has_texture"],
				(node->parts[j].textureID > 0) ? 1 : 0);

			glActiveTexture(GL_TEXTURE0);
			glUniform1i(m_geometry_rendering_program["uniform_texture"], 0);
			glBindTexture(GL_TEXTURE_2D, node->parts[j].textureID);

			glDrawArrays(GL_TRIANGLES, node->parts[j].start_offset, node->parts[j].count);
		}

		glBindVertexArray(0);
	}

	this->m_geometry_rendering_program.Unbind();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0.f, 0.8f, 1.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);

	m_post_rendering_program.Bind();

	glBindVertexArray(m_vao_fbo);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_fbo_texture);
	glUniform1i(m_post_rendering_program["uniform_texture"], 0);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	m_post_rendering_program.Unbind();
}

void Renderer::CameraMoveForward(bool enable)
{
	m_camera_movement.x = (enable) ? 1 : 0;
}
void Renderer::CameraMoveBackWard(bool enable)
{
	m_camera_movement.x = (enable) ? -1 : 0;
}

void Renderer::CameraMoveLeft(bool enable)
{
	m_camera_movement.y = (enable) ? -1 : 0;
}
void Renderer::CameraMoveRight(bool enable)
{
	m_camera_movement.y = (enable) ? 1 : 0;
}

void Renderer::CameraLook(glm::vec2 lookDir)
{
	m_camera_look_angle_destination = lookDir;
}