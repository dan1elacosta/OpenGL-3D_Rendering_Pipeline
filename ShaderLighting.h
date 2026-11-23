#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// ---------------------------------------------------------------------------------
// CÓDIGO GLSL DE LOS SHADERS (ILUMINACIÓN PHONG + MATRIZ NORMAL)
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

// ---------------------------------------------------------------------------------
// FUNCIONES AUXILIARES DE COMPILACIÓN Y CONFIGURACIÓN DE ILUMINACIÓN
// ---------------------------------------------------------------------------------
inline GLuint createPhongShaderProgram()
{
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

    return shaderProgram;
}

inline void setupSceneLighting(GLuint shaderProgram, const glm::vec3& cameraPos)
{
    glm::vec3 lightPos = glm::vec3(15.0f, 25.0f, 15.0f);
    glm::vec3 lightColor = glm::vec3(1.0f, 0.98f, 0.92f);

    glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
    glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(lightColor));
}

inline void setMaterialPhong(GLuint shaderProgram, float specularStrength, float shininess)
{
    glUniform1f(glGetUniformLocation(shaderProgram, "specularStrength"), specularStrength);
    glUniform1f(glGetUniformLocation(shaderProgram, "shininess"), shininess);
}