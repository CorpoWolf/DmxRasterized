#define NOMINMAX  // Disable Windows min/max macros
#include <winsock2.h>
#include <windows.h>

#include <iostream>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <cstdint>
#include <set>

#pragma comment(lib, "Ws2_32.lib")

#include "glad/glad.h"
#include "glfw3.h"
#include "SpoutGL/SpoutSender.h"

#include "global.hpp"
#include "shader.hpp"
#include "artnet.hpp"

uint8_t dmxData[TOTAL_DMX_CHANNELS] = {0};
float dmxDataNormalized[TOTAL_DMX_CHANNELS] = {0.0f};
GLuint dmxDataTexture;

int main(int argc, char* argv[]) {
	bool debug = false;
	int port = 6454;

	if (argc > 0) {
		for (int i = 1; i < argc; ++i) {
			std::string arg = argv[i];
				
			if (arg == "-p" || arg == "--port") {
				if (i + 1 < argc) { // Check if there's an argument after -p/--port
					try {
						port = std::stoi(argv[++i]); // Parse the next argument as an integer
						if (port < 1 || port > 65535) {
							throw std::out_of_range("Port out of valid range");
						}
					} catch (const std::exception& e) {
						std::cerr << "Invalid port number provided: " << argv[i] << std::endl;
						std::cerr << "Using default port: " << port << std::endl;
					}
				} else {
					std::cerr << "Error: Missing value for " << arg << " flag." << std::endl;
					std::cerr << "Using default port: " << port << std::endl;
				}
			} else if (arg == "-d" || arg == "--debug") {
				debug = true;
			} else {
				std::cerr << "unkown argument: " << arg << std::endl;
			}
		}
	}

	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW" << std::endl;
		return -1;
	}


	if(!debug){
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	}
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow* window = glfwCreateWindow(RENDER_WIDTH, RENDER_HEIGHT, "DMX Shader Renderer", nullptr, nullptr);
	if (!window) {
		std::cerr << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
		std::cerr << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	glViewport(0, 0, RENDER_WIDTH, RENDER_HEIGHT);

	glGenTextures(1, &dmxDataTexture);
	glBindTexture(GL_TEXTURE_1D, dmxDataTexture);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_R32F, TOTAL_DMX_CHANNELS, 0, GL_RED, GL_FLOAT, dmxDataNormalized);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	Shader shader("shaders/vertex.glsl", "shaders/frag.glsl");
	glUseProgram(shader.ID);
	glUniform2f(glGetUniformLocation(shader.ID, "resolution"), RENDER_WIDTH, RENDER_HEIGHT);
	float vertices[] = {
		-1.0f,  1.0f,  0.0f, 1.0f, // Top-left
		-1.0f, -1.0f,  0.0f, 0.0f, // Bottom-left
		 1.0f, -1.0f,  1.0f, 0.0f, // Bottom-right

		-1.0f,  1.0f,  0.0f, 1.0f, // Top-left
		 1.0f, -1.0f,  1.0f, 0.0f, // Bottom-right
		 1.0f,  1.0f,  1.0f, 1.0f  // Top-right
	};

	GLuint VAO, VBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);

	SOCKET artNetSocket = setupArtNetSocket(port);
	std::unordered_map<uint16_t, std::vector<uint8_t>> dmxDataMap;

	SpoutSender sender;
	if (!sender.CreateSender("DmxRasterizer", RENDER_WIDTH, RENDER_HEIGHT)) {
    std::cerr << "Failed to create Spout sender!" << std::endl;
    return -1;
	}

	GLuint texture, framebuffer;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RENDER_WIDTH, RENDER_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

	while (!glfwWindowShouldClose(window)) {
		receiveArtNetData(artNetSocket, dmxDataMap, dmxData);

		for (int i = 0; i < TOTAL_DMX_CHANNELS; ++i) {
			dmxDataNormalized[i] = dmxData[i] / 255.0f;
		}

		glBindTexture(GL_TEXTURE_1D, dmxDataTexture);
		glTexSubImage1D(GL_TEXTURE_1D, 0, 0, TOTAL_DMX_CHANNELS, GL_RED, GL_FLOAT, dmxDataNormalized);

		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glClear(GL_COLOR_BUFFER_BIT);
		shader.use();
		glBindVertexArray(VAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		sender.SendTexture(texture, GL_TEXTURE_2D, RENDER_WIDTH, RENDER_HEIGHT);

		glfwPollEvents();
	}

	sender.ReleaseSender();
	glDeleteTextures(1, &texture);
	glDeleteTextures(1, &dmxDataTexture);
	glDeleteFramebuffers(1, &framebuffer);
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glfwTerminate();
	closesocket(artNetSocket);
	WSACleanup();
	return 0;
}