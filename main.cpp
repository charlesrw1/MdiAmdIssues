#include <vector>
#include <SDL2/SDL.h>
#include <fstream>
#include "glm/glm.hpp"
#include "glad/glad.h"
#include "glm/gtc/matrix_transform.hpp"
#include "Shader.h"
#include "Profilier.h"

SDL_Window* window;
SDL_GLContext context;
int window_w = 1280;
int window_h = 720;

float verticies_square[] =
{
	0,0,0,
	0,1,0,
	1,1,0,
	1,0,0
};

struct DrawElementsIndirectCommand
{
	uint32_t count;
	uint32_t primCount;
	uint32_t firstIndex;
	int  baseVertex;
	uint32_t baseInstance;
};

std::vector<glm::mat4> matricies;

uint32_t vao;
uint32_t index_buffer;
uint32_t vertex_buffer;
uint32_t command_buffer;
uint32_t mdi_matricies_buffer;
uint32_t element_count = 6;

Shader naive_shader;
Shader mdi_shader;

bool drawing_with_mdi = false;


void debug_message_callback(GLenum source, GLenum type, GLuint id,
	GLenum severity, GLsizei length, GLchar const* message, void const* user_param)
{
	auto const src_str = [source]() {
		switch (source)
		{
		case GL_DEBUG_SOURCE_API: return "API";
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "WINDOW SYSTEM";
		case GL_DEBUG_SOURCE_SHADER_COMPILER: return "SHADER COMPILER";
		case GL_DEBUG_SOURCE_THIRD_PARTY: return "THIRD PARTY";
		case GL_DEBUG_SOURCE_APPLICATION: return "APPLICATION";
		case GL_DEBUG_SOURCE_OTHER: return "OTHER";
		default: return "";
		}
	}();

	auto const type_str = [type]() {
		switch (type)
		{
		case GL_DEBUG_TYPE_ERROR: return "ERROR";
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "DEPRECATED_BEHAVIOR";
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "UNDEFINED_BEHAVIOR";
		case GL_DEBUG_TYPE_PORTABILITY: return "PORTABILITY";
		case GL_DEBUG_TYPE_PERFORMANCE: return "PERFORMANCE";
		case GL_DEBUG_TYPE_MARKER: return "MARKER";
		case GL_DEBUG_TYPE_OTHER: return "OTHER";
		default: return "";

		}
	}();

	auto const severity_str = [severity]() {
		switch (severity) {
		case GL_DEBUG_SEVERITY_NOTIFICATION: return "NOTIFICATION";
		case GL_DEBUG_SEVERITY_LOW: return "LOW";
		case GL_DEBUG_SEVERITY_MEDIUM: return "MEDIUM";
		case GL_DEBUG_SEVERITY_HIGH: return "HIGH";
		default: return "";

		}
	}();

	printf("%s, %s, %s, %d: %s\n", src_str, type_str, severity_str, id, message);
}

void init()
{
	if (SDL_Init(SDL_INIT_EVERYTHING)) {
		printf(__FUNCTION__": %s\n", SDL_GetError());
		exit(-1);
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	window = SDL_CreateWindow("MdiBroken", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		window_w, window_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!window) {
		printf(__FUNCTION__": %s\n", SDL_GetError());
		exit(-1);
	}

	context = SDL_GL_CreateContext(window);
	printf("OpenGL loaded\n");
	gladLoadGLLoader(SDL_GL_GetProcAddress);
	printf("Vendor: %s\n", glGetString(GL_VENDOR));
	printf("Renderer: %s\n", glGetString(GL_RENDERER));
	printf("Version: %s\n\n", glGetString(GL_VERSION));

	SDL_GL_SetSwapInterval(0);

	Shader::compile(&naive_shader, "SimpleMeshV.txt", "SimpleF.txt", "NAIVE");
	Shader::compile(&mdi_shader, "SimpleMeshV.txt", "SimpleF.txt", "MDI");

	std::vector<float> verticies;
	std::vector<uint32_t> elements;

	for (int i = 0; i < 1000; i++) {

		int start = verticies.size();
		for (int j = 0; j < 12; j++) {
			verticies.push_back(verticies_square[j]);
		}
		elements.push_back(start + 0);
		elements.push_back(start + 1);
		elements.push_back(start + 2);
		elements.push_back(start + 0);
		elements.push_back(start + 2);
		elements.push_back(start + 3);
	}
	element_count = elements.size();

	for (int y = 0; y < 20; y++) {
		for (int x = 0; x < 20; x++) {
			matricies.push_back(
				glm::scale(
					glm::translate(glm::mat4(1.f), glm::vec3(x, y, 0.f)), 
					glm::vec3(0.9f)
				)
			);
		}
	}

	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(debug_message_callback, nullptr);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

	// monkey model .bin dump
	// ~60k triangles
	std::ifstream infile("monkey_model.bin", std::ios::binary);
	size_t vert_size = 0;
	std::vector<char> verts;
	infile.read((char*)&vert_size, sizeof(size_t));
	verts.resize(vert_size);
	infile.read((char*)verts.data(), vert_size);
	std::vector<char> indicies;
	size_t index_size = 0;
	infile.read((char*)&index_size, sizeof(size_t));
	indicies.resize(index_size);
	infile.read((char*)indicies.data(), index_size);
	infile.close();

	element_count = index_size / 4;

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, verts.size(), verts.data(), GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT,
		false, 12, (void*)0);

	glGenBuffers(1, &index_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,indicies.size(), indicies.data(), GL_STATIC_DRAW);

	glBindVertexArray(0);


	glCreateBuffers(1, &mdi_matricies_buffer);
	glNamedBufferStorage(mdi_matricies_buffer, sizeof(glm::mat4) * matricies.size(), matricies.data(), GL_DYNAMIC_STORAGE_BIT);

	std::vector<DrawElementsIndirectCommand> commands;
	for (int i = 0; i < matricies.size(); i++) {
		DrawElementsIndirectCommand cmd;
		cmd.baseInstance = 0;
		cmd.baseVertex = 0;
		cmd.count = element_count;
		cmd.firstIndex = 0;
		cmd.primCount = 1;
		commands.push_back(cmd);
	}
	glCreateBuffers(1, &command_buffer);
	glNamedBufferStorage(command_buffer, sizeof(DrawElementsIndirectCommand) * matricies.size(), commands.data(), GL_DYNAMIC_STORAGE_BIT);

}


void draw()
{
	GPUSCOPESTART("draw");

	glClearColor(0.1, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 projection = glm::ortho(0.f, 50.f, 50.f, 0.f);
	glBindVertexArray(vao);

	if (drawing_with_mdi) {
		mdi_shader.use();
		mdi_shader.set_mat4("viewproj", projection);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, mdi_matricies_buffer);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, command_buffer);

		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			GL_UNSIGNED_INT,
			(void*)0,
			matricies.size(),
			sizeof(DrawElementsIndirectCommand)
		);
	}
	else {
		naive_shader.use();
		naive_shader.set_mat4("viewproj", projection);
		for (int i = 0; i < matricies.size(); i++) {
			naive_shader.set_int("integer", i);
			naive_shader.set_mat4("model", matricies[i]);

			glDrawElementsBaseVertex(GL_TRIANGLES, element_count, GL_UNSIGNED_INT, (void*)0, 0);
		}
	}
}




int main(int argc, char** argv)
{
	init();
	std::string title_str;
	while (1)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				return 1;
				break;
			case SDL_KEYDOWN:
				if(event.key.keysym.scancode == SDL_SCANCODE_1)
					drawing_with_mdi = !drawing_with_mdi;
				break;
			}
		}

		double cpu_time, gpu_time;
		if (Profiler::get_time_for_name("draw", cpu_time, gpu_time)) {
			title_str.clear();
			title_str += "Press 1 to toggle mode : ";
			title_str += (drawing_with_mdi) ? "MDI" : "NAIVE";
			title_str += " : ";
			title_str += "Cpu ";
			title_str += std::to_string(cpu_time);
			title_str += " : ";
			title_str += "Gpu ";
			title_str += std::to_string(gpu_time);


			SDL_SetWindowTitle(window, title_str.c_str());
		}

		draw();
		SDL_GL_SwapWindow(window);

		Profiler::end_frame_tick();
	}

	return 0;
}