// ---------------------------------------------------------------------------------
// BIBLIOTECAS Y CABECERAS
// ---------------------------------------------------------------------------------
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

// ---------------------------------------------------------------------------------
// VARIABLES GLOBALES DE CÁMARA (WASD + MOUSE)
// ---------------------------------------------------------------------------------
const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;

const glm::vec3 INITIAL_CAMERA_POS = glm::vec3(0.0f, 11.0f, 22.0f);
const float INITIAL_YAW = -90.0f;
const float INITIAL_PITCH = -24.0f;

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
// FUNCIÓN MATEMÁTICA DE CURVA DE BÉZIER CÚBICA
// ---------------------------------------------------------------------------------
glm::vec3 evaluateBezier(float t, glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3)
{
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    glm::vec3 point = uuu * p0;
    point += 3.0f * uu * t * p1;
    point += 3.0f * u * tt * p2;
    point += ttt * p3;
    return point;
}

// Derivada B'(t) para obtener la dirección tangente de vuelo de la nave
glm::vec3 evaluateBezierTangent(float t, glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3)
{
    float u = 1.0f - t;
    glm::vec3 tangent = 3.0f * u * u * (p1 - p0)
        + 6.0f * u * t * (p2 - p1)
        + 3.0f * t * t * (p3 - p2);
    if (glm::length(tangent) < 0.0001f) return glm::vec3(0.0f, 0.0f, -1.0f);
    return glm::normalize(tangent);
}

// ---------------------------------------------------------------------------------
// CARGADOR DE MODELO GLB / GLTF (POSICIÓN + COLOR/TEXTURA + NORMAL)
// ---------------------------------------------------------------------------------
bool loadGLBModel(const std::string& filename, std::vector<float>& outVertices, glm::vec3& outCenter, float& outMaxDim)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    loader.SetImageLoader([](tinygltf::Image* image, const int, std::string*, std::string*,
        int, int, const unsigned char* bytes, int size, void*) -> bool {
            int w = 0, h = 0, comp = 0;
            unsigned char* data = stbi_load_from_memory(bytes, size, &w, &h, &comp, 4);
            if (!data) return false;
            image->width = w;
            image->height = h;
            image->component = 4;
            image->bits = 8;
            image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
            image->image.assign(data, data + (w * h * 4));
            stbi_image_free(data);
            return true;
        }, nullptr);

    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
    if (!warn.empty()) std::cout << "Advertencia GLB: " << warn << std::endl;
    if (!err.empty())  std::cerr << "Error GLB: " << err << std::endl;
    if (!ret) return false;

    float minX = 1e10f, minY = 1e10f, minZ = 1e10f;
    float maxX = -1e10f, maxY = -1e10f, maxZ = -1e10f;

    for (const auto& mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives) {
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES && primitive.mode != -1) continue;

            float baseR = 0.90f, baseG = 0.45f, baseB = 0.15f;
            const tinygltf::Image* texImage = nullptr;

            if (primitive.material >= 0 && primitive.material < static_cast<int>(model.materials.size())) {
                const auto& pbr = model.materials[primitive.material].pbrMetallicRoughness;
                if (pbr.baseColorFactor.size() >= 3) {
                    baseR = static_cast<float>(pbr.baseColorFactor[0]);
                    baseG = static_cast<float>(pbr.baseColorFactor[1]);
                    baseB = static_cast<float>(pbr.baseColorFactor[2]);
                }
                int texIdx = pbr.baseColorTexture.index;
                if (texIdx >= 0 && texIdx < static_cast<int>(model.textures.size())) {
                    int imgIdx = model.textures[texIdx].source;
                    if (imgIdx >= 0 && imgIdx < static_cast<int>(model.images.size()) && !model.images[imgIdx].image.empty()) {
                        texImage = &model.images[imgIdx];
                    }
                }
            }

            auto posIt = primitive.attributes.find("POSITION");
            if (posIt == primitive.attributes.end()) continue;
            const auto& posAccessor = model.accessors[posIt->second];
            const auto& posView = model.bufferViews[posAccessor.bufferView];
            const auto& posBuffer = model.buffers[posView.buffer];
            const float* positions = reinterpret_cast<const float*>(&posBuffer.data[posView.byteOffset + posAccessor.byteOffset]);
            size_t posStride = posAccessor.ByteStride(posView) ? (posAccessor.ByteStride(posView) / sizeof(float)) : 3;

            const float* normals = nullptr;
            size_t normStride = 3;
            auto normIt = primitive.attributes.find("NORMAL");
            if (normIt != primitive.attributes.end()) {
                const auto& normAccessor = model.accessors[normIt->second];
                const auto& normView = model.bufferViews[normAccessor.bufferView];
                const auto& normBuffer = model.buffers[normView.buffer];
                normals = reinterpret_cast<const float*>(&normBuffer.data[normView.byteOffset + normAccessor.byteOffset]);
                normStride = normAccessor.ByteStride(normView) ? (normAccessor.ByteStride(normView) / sizeof(float)) : 3;
            }

            const float* uvs = nullptr;
            size_t uvStride = 2;
            auto uvIt = primitive.attributes.find("TEXCOORD_0");
            if (uvIt != primitive.attributes.end()) {
                const auto& uvAccessor = model.accessors[uvIt->second];
                const auto& uvView = model.bufferViews[uvAccessor.bufferView];
                const auto& uvBuffer = model.buffers[uvView.buffer];
                uvs = reinterpret_cast<const float*>(&uvBuffer.data[uvView.byteOffset + uvAccessor.byteOffset]);
                uvStride = uvAccessor.ByteStride(uvView) ? (uvAccessor.ByteStride(uvView) / sizeof(float)) : 2;
            }

            std::vector<uint32_t> indices;
            if (primitive.indices >= 0) {
                const auto& idxAccessor = model.accessors[primitive.indices];
                const auto& idxView = model.bufferViews[idxAccessor.bufferView];
                const auto& idxBuffer = model.buffers[idxView.buffer];
                const unsigned char* dataPtr = &idxBuffer.data[idxView.byteOffset + idxAccessor.byteOffset];

                for (size_t i = 0; i < idxAccessor.count; ++i) {
                    if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                        indices.push_back(reinterpret_cast<const uint16_t*>(dataPtr)[i]);
                    else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                        indices.push_back(reinterpret_cast<const uint32_t*>(dataPtr)[i]);
                    else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                        indices.push_back(dataPtr[i]);
                }
            }
            else {
                for (size_t i = 0; i < posAccessor.count; ++i) indices.push_back(static_cast<uint32_t>(i));
            }

            for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                uint32_t idx[3] = { indices[i], indices[i + 1], indices[i + 2] };
                glm::vec3 vPos[3];
                for (int k = 0; k < 3; ++k) {
                    vPos[k] = glm::vec3(
                        positions[idx[k] * posStride + 0],
                        positions[idx[k] * posStride + 1],
                        positions[idx[k] * posStride + 2]
                    );
                }
                glm::vec3 faceNormal = glm::normalize(glm::cross(vPos[1] - vPos[0], vPos[2] - vPos[0]));

                for (int k = 0; k < 3; ++k) {
                    float x = vPos[k].x, y = vPos[k].y, z = vPos[k].z;
                    float nx = faceNormal.x, ny = faceNormal.y, nz = faceNormal.z;

                    if (normals) {
                        nx = normals[idx[k] * normStride + 0];
                        ny = normals[idx[k] * normStride + 1];
                        nz = normals[idx[k] * normStride + 2];
                    }

                    float r = baseR, g = baseG, b = baseB;
                    if (texImage && uvs) {
                        float u = uvs[idx[k] * uvStride + 0];
                        float v = uvs[idx[k] * uvStride + 1];
                        u = u - std::floor(u);
                        v = v - std::floor(v);
                        int px = std::clamp(static_cast<int>(u * texImage->width), 0, texImage->width - 1);
                        int py = std::clamp(static_cast<int>(v * texImage->height), 0, texImage->height - 1);
                        size_t pixelOffset = (static_cast<size_t>(py) * texImage->width + px) * 4;
                        r = (texImage->image[pixelOffset + 0] / 255.0f) * baseR;
                        g = (texImage->image[pixelOffset + 1] / 255.0f) * baseG;
                        b = (texImage->image[pixelOffset + 2] / 255.0f) * baseB;
                    }

                    outVertices.push_back(x);  outVertices.push_back(y);  outVertices.push_back(z);
                    outVertices.push_back(r);  outVertices.push_back(g);  outVertices.push_back(b);
                    outVertices.push_back(nx); outVertices.push_back(ny); outVertices.push_back(nz);

                    if (x < minX) minX = x; if (x > maxX) maxX = x;
                    if (y < minY) minY = y; if (y > maxY) maxY = y;
                    if (z < minZ) minZ = z; if (z > maxZ) maxZ = z;
                }
            }
        }
    }

    outCenter = glm::vec3((minX + maxX) / 2.0f, (minY + maxY) / 2.0f, (minZ + maxZ) / 2.0f);
    outMaxDim = std::max({ maxX - minX, maxY - minY, maxZ - minZ });
    return !outVertices.empty();
}

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
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

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
// SHADERS CON ILUMINACIÓN PHONG Y MATRIZ NORMAL
// ---------------------------------------------------------------------------------
const char* vertexShaderSource = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;
out vec3 ObjectColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = normalMatrix * aNormal;
    ObjectColor = aColor;

    gl_Position = projection * view * vec4(FragPos, 1.0);
})";

const char* fragmentShaderSource = R"(#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec3 ObjectColor;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform float specularStrength;
uniform float shininess;

void main()
{
    float ambientStrength = 0.30;
    vec3 ambient = ambientStrength * lightColor;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * ObjectColor;
    FragColor = vec4(result, 1.0);
})";

int main()
{
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Escenario 3D - Paso 1: Carga de Nave GLB", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    glEnable(GL_DEPTH_TEST);

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
    // 1. CARGA DEL MAPA NEVADO (.OBJ + .MTL)
    // ---------------------------------------------------------------------------------
    std::vector<float> mapVertices;
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
        "models/MAPA NEVADO/model.obj",
        "models/MAPA NEVADO/");
    if (!ret) return -1;

    float minX = 1e10f, minY = 1e10f, minZ = 1e10f;
    float maxX = -1e10f, maxY = -1e10f, maxZ = -1e10f;

    for (const auto& shape : shapes) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shape.mesh.num_face_vertices[f]);
            int matId = shape.mesh.material_ids[f];
            float r = 0.85f, g = 0.90f, b = 0.95f;
            if (matId >= 0 && matId < static_cast<int>(materials.size())) {
                r = materials[matId].diffuse[0];
                g = materials[matId].diffuse[1];
                b = materials[matId].diffuse[2];
            }

            glm::vec3 triPos[3];
            for (size_t v = 0; v < 3 && v < fv; v++) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
                triPos[v] = glm::vec3(
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2]
                );
            }
            glm::vec3 faceNormal = glm::normalize(glm::cross(triPos[1] - triPos[0], triPos[2] - triPos[0]));

            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
                float x = attrib.vertices[3 * idx.vertex_index + 0];
                float y = attrib.vertices[3 * idx.vertex_index + 1];
                float z = attrib.vertices[3 * idx.vertex_index + 2];

                float nx = faceNormal.x, ny = faceNormal.y, nz = faceNormal.z;
                if (idx.normal_index >= 0 && !attrib.normals.empty()) {
                    nx = attrib.normals[3 * idx.normal_index + 0];
                    ny = attrib.normals[3 * idx.normal_index + 1];
                    nz = attrib.normals[3 * idx.normal_index + 2];
                }

                mapVertices.push_back(x);  mapVertices.push_back(y);  mapVertices.push_back(z);
                mapVertices.push_back(r);  mapVertices.push_back(g);  mapVertices.push_back(b);
                mapVertices.push_back(nx); mapVertices.push_back(ny); mapVertices.push_back(nz);

                if (x < minX) minX = x; if (x > maxX) maxX = x;
                if (y < minY) minY = y; if (y > maxY) maxY = y;
                if (z < minZ) minZ = z; if (z > maxZ) maxZ = z;
            }
            index_offset += fv;
        }
    }

    glm::vec3 mapCenter = glm::vec3((minX + maxX) / 2.0f, minY, (minZ + maxZ) / 2.0f);
    float mapMaxDim = std::max({ maxX - minX, maxY - minY, maxZ - minZ });
    float mapScale = (mapMaxDim > 0.0001f) ? (25.0f / mapMaxDim) : 1.0f;

    GLuint mapVAO, mapVBO;
    glGenVertexArrays(1, &mapVAO);
    glGenBuffers(1, &mapVBO);
    glBindVertexArray(mapVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mapVBO);
    glBufferData(GL_ARRAY_BUFFER, mapVertices.size() * sizeof(float), mapVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // ---------------------------------------------------------------------------------
    // 2. CARGA DE LA NAVE ESPACIAL (.GLB)
    // ---------------------------------------------------------------------------------
    std::vector<float> shipVertices;
    glm::vec3 shipCenter;
    float shipMaxDim = 1.0f;

    if (!loadGLBModel("models/SPACESHIP/SPACESHIP.glb", shipVertices, shipCenter, shipMaxDim)) {
        std::cerr << "No se pudo cargar models/SPACESHIP/SPACESHIP.glb" << std::endl;
        return -1;
    }
    float shipScale = (shipMaxDim > 0.0001f) ? (2.5f / shipMaxDim) : 1.0f;
    std::cout << "Nave GLB cargada con exito (" << shipVertices.size() / 9 << " vertices)." << std::endl;

    GLuint shipVAO, shipVBO;
    glGenVertexArrays(1, &shipVAO);
    glGenBuffers(1, &shipVBO);
    glBindVertexArray(shipVAO);
    glBindBuffer(GL_ARRAY_BUFFER, shipVBO);
    glBufferData(GL_ARRAY_BUFFER, shipVertices.size() * sizeof(float), shipVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // Uniforms
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
    GLint normalMatrixLoc = glGetUniformLocation(shaderProgram, "normalMatrix");
    GLint specStrengthLoc = glGetUniformLocation(shaderProgram, "specularStrength");
    GLint shininessLoc = glGetUniformLocation(shaderProgram, "shininess");
    GLint lightPosLoc = glGetUniformLocation(shaderProgram, "lightPos");
    GLint viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
    GLint lightColorLoc = glGetUniformLocation(shaderProgram, "lightColor");

    glm::vec3 lightPos = glm::vec3(15.0f, 25.0f, 15.0f);
    glm::vec3 lightColor = glm::vec3(1.0f, 0.98f, 0.92f);

    // Puntos de la Curva de Bezier cúbica para la trayectoria de la nave
    glm::vec3 P0 = glm::vec3(-10.0f, 10.0f, 7.0f);
    glm::vec3 P1 = glm::vec3(-6.0f, 16.5f, -9.0f);
    glm::vec3 P2 = glm::vec3(8.0f, 14.5f, -8.0f);
    glm::vec3 P3 = glm::vec3(10.0f, 9.0f, 7.0f);

    // ---------------------------------------------------------------------------------
    // BUCLE PRINCIPAL DE RENDERIZADO
    // ---------------------------------------------------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.53f, 0.72f, 0.88f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 200.0f);

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(lightPosLoc, 1, glm::value_ptr(lightPos));
        glUniform3fv(viewPosLoc, 1, glm::value_ptr(cameraPos));
        glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));

        // --- A. RENDERIZAR MAPA NEVADO (ESTÁTICO) ---
        glm::mat4 mapModel = glm::mat4(1.0f);
        mapModel = glm::scale(mapModel, glm::vec3(mapScale));
        mapModel = glm::translate(mapModel, -mapCenter);
        glm::mat3 mapNormalMatrix = glm::transpose(glm::inverse(glm::mat3(mapModel)));

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(mapModel));
        glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(mapNormalMatrix));
        glUniform1f(specStrengthLoc, 0.25f);
        glUniform1f(shininessLoc, 16.0f);

        glBindVertexArray(mapVAO);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mapVertices.size() / 9));

        // --- B. RENDERIZAR NAVE ESPACIAL (.GLB) CON CURVA DE BÉZIER + ORIENTACIÓN DINÁMICA ---
        float t = 0.5f * (1.0f - cos(currentFrame * 0.45f));
        float dirSign = (sin(currentFrame * 0.45f) >= 0.0f) ? 1.0f : -1.0f;

        glm::vec3 bezierPos = evaluateBezier(t, P0, P1, P2, P3);
        glm::vec3 tangent = evaluateBezierTangent(t, P0, P1, P2, P3) * dirSign;

        // Ángulos de dirección (Yaw/Pitch) y alabeo (Roll)
        float headingYaw = atan2(tangent.x, tangent.z);
        float pitchAngle = -asin(glm::clamp(tangent.y, -0.99f, 0.99f));
        float rollAngle = sin(currentFrame * 2.0f) * glm::radians(30.0f);

        glm::mat4 shipModel = glm::mat4(1.0f);
        shipModel = glm::translate(shipModel, bezierPos);
        shipModel = glm::rotate(shipModel, headingYaw, glm::vec3(0.0f, 1.0f, 0.0f));
        shipModel = glm::rotate(shipModel, pitchAngle, glm::vec3(1.0f, 0.0f, 0.0f));
        shipModel = glm::rotate(shipModel, rollAngle, glm::vec3(0.0f, 0.0f, 1.0f));
        shipModel = glm::scale(shipModel, glm::vec3(shipScale));
        shipModel = glm::translate(shipModel, -shipCenter);

        glm::mat3 shipNormalMatrix = glm::transpose(glm::inverse(glm::mat3(shipModel)));

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(shipModel));
        glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(shipNormalMatrix));
        glUniform1f(specStrengthLoc, 0.95f);
        glUniform1f(shininessLoc, 64.0f);

        glBindVertexArray(shipVAO);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(shipVertices.size() / 9));

        // Líneas que refrescan la ventana en cada fotograma y procesan los eventos de entrada (teclado, ratón, etc.)
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &mapVAO);
    glDeleteBuffers(1, &mapVBO);
    glDeleteVertexArrays(1, &shipVAO);
    glDeleteBuffers(1, &shipVBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}