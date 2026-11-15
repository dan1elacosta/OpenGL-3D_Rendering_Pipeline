// ---------------------------------------------------------------------------------
// BIBLIOTECAS Y CABECERAS
// ---------------------------------------------------------------------------------
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

// ---------------------------------------------------------------------------------
// VARIABLES GLOBALES DE CÁMARA (WASD + MOUSE)
// ---------------------------------------------------------------------------------
const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;

// Posición y orientación inicial panorámica frente al escenario
const glm::vec3 INITIAL_CAMERA_POS = glm::vec3(0.0f, 11.0f, 22.0f);
const float INITIAL_YAW = -90.0f; // Mirando directo al centro del mapa (-Z)
const float INITIAL_PITCH = -24.0f; // Inclinación hacia abajo enfocando el valle

glm::vec3 cameraPos = INITIAL_CAMERA_POS;
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 cameraFront = glm::normalize(glm::vec3(
    cos(glm::radians(INITIAL_YAW)) * cos(glm::radians(INITIAL_PITCH)),
    sin(glm::radians(INITIAL_PITCH)),
    sin(glm::radians(INITIAL_YAW)) * cos(glm::radians(INITIAL_PITCH))
));

bool firstMouse = true;
float yaw = INITIAL_YAW;
float pitch = INITIAL_PITCH;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// ---------------------------------------------------------------------------------
// CALLBACKS Y CONTROLES
// ---------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // Invertido: coordenadas Y van de abajo hacia arriba
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // Evitar que la cámara dé la vuelta completa verticalmente
    if (pitch > 89.0f)  pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Tecla R: Restaurar cámara a la vista panorámica inicial
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
    {
        cameraPos = INITIAL_CAMERA_POS;
        yaw = INITIAL_YAW;
        pitch = INITIAL_PITCH;
        cameraFront = glm::normalize(glm::vec3(
            cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
            sin(glm::radians(pitch)),
            sin(glm::radians(yaw)) * cos(glm::radians(pitch))
        ));
    }

    float cameraSpeed = 7.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraUp;
}

// ---------------------------------------------------------------------------------
// SHADERS (SOPORTE PARA COLORES DE MATERIAL .MTL)
// ---------------------------------------------------------------------------------
const char* vertexShaderSource = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 FragColorVertex;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    FragColorVertex = aColor;
})";

const char* fragmentShaderSource = R"(#version 330 core
out vec4 FragColor;

in vec3 FragColorVertex;

void main()
{
    FragColor = vec4(FragColorVertex, 1.0);
})";

int main()
{
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Escenario 3D - Camara Libre WASD + Mouse", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);

    // Capturar el cursor del mouse dentro de la ventana para rotar libremente
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    glEnable(GL_DEPTH_TEST);

    // Compilación de Shaders
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // ---------------------------------------------------------------------------------
    // CARGA DEL MAPA NEVADO (.OBJ + .MTL) Y BOUNDING BOX
    // ---------------------------------------------------------------------------------
    std::vector<float> mapVertices;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    // El 7mo parámetro indica la carpeta base donde buscar "materials.mtl"
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
        "models/MAPA NEVADO/model.obj",
        "models/MAPA NEVADO/");

    if (!warn.empty()) std::cout << "Advertencia OBJ: " << warn << std::endl;
    if (!err.empty())  std::cerr << "Error OBJ: " << err << std::endl;
    if (!ret) return -1;

    std::cout << "Mapa cargado correctamente. Materiales encontrados: " << materials.size() << "\n";

    float minX = 1e10f, minY = 1e10f, minZ = 1e10f;
    float maxX = -1e10f, maxY = -1e10f, maxZ = -1e10f;

    for (const auto& shape : shapes) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shape.mesh.num_face_vertices[f]);

            // Obtener color difuso del material asociado a esta cara
            int matId = shape.mesh.material_ids[f];
            float r = 0.85f, g = 0.90f, b = 0.95f;
            if (matId >= 0 && matId < static_cast<int>(materials.size())) {
                r = materials[matId].diffuse[0];
                g = materials[matId].diffuse[1];
                b = materials[matId].diffuse[2];
            }

            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
                float x = attrib.vertices[3 * idx.vertex_index + 0];
                float y = attrib.vertices[3 * idx.vertex_index + 1];
                float z = attrib.vertices[3 * idx.vertex_index + 2];

                // Guardar Posición (X, Y, Z) y Color (R, G, B)
                mapVertices.push_back(x);
                mapVertices.push_back(y);
                mapVertices.push_back(z);
                mapVertices.push_back(r);
                mapVertices.push_back(g);
                mapVertices.push_back(b);

                if (x < minX) minX = x; if (x > maxX) maxX = x;
                if (y < minY) minY = y; if (y > maxY) maxY = y;
                if (z < minZ) minZ = z; if (z > maxZ) maxZ = z;
            }
            index_offset += fv;
        }
    }

    // Centrar en X y Z, apoyando la base (minY) en Y = 0
    glm::vec3 mapCenter = glm::vec3((minX + maxX) / 2.0f, minY, (minZ + maxZ) / 2.0f);
    float maxDim = std::max({ maxX - minX, maxY - minY, maxZ - minZ });
    float mapScale = (maxDim > 0.0001f) ? (25.0f / maxDim) : 1.0f;

    // Configuración de VAO y VBO del Mapa
    GLuint mapVAO, mapVBO;
    glGenVertexArrays(1, &mapVAO);
    glGenBuffers(1, &mapVBO);

    glBindVertexArray(mapVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mapVBO);
    glBufferData(GL_ARRAY_BUFFER, mapVertices.size() * sizeof(float), mapVertices.data(), GL_STATIC_DRAW);

    // Atributo 0: Posición (3 floats)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Atributo 1: Color del material .mtl (3 floats)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projLoc = glGetUniformLocation(shaderProgram, "projection");

    // ---------------------------------------------------------------------------------
    // BUCLE PRINCIPAL DE RENDERIZADO
    // ---------------------------------------------------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // Cielo azul invernal suave
        glClearColor(0.53f, 0.72f, 0.88f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Matriz de Modelo estática para el Mapa Nevado
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(mapScale));
        model = glm::translate(model, -mapCenter);

        // Matriz de Vista actualizada por WASD + Mouse
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 200.0f);

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        glBindVertexArray(mapVAO);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mapVertices.size() / 6));

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &mapVAO);
    glDeleteBuffers(1, &mapVBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}