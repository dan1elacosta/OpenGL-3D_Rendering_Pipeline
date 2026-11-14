cmake_minimum_required (VERSION 3.10)
project ("Trabajo_Modelo3D_CG" C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)

# Añadir todas las variantes de rutas posibles de tus cabeceras
include_directories(
    libraries
    libraries/glad/include
    libraries/glfw-3.5.1/include
    libraries/glm
)

add_subdirectory(libraries/glfw-3.5.1)

# Buscar glad.c automáticamente en libraries
file(GLOB_RECURSE GLAD_SOURCES "libraries/glad/*.c")

add_executable (${PROJECT_NAME} Trabajo_Modelo3D_CG.cpp ${GLAD_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    OpenGL::GL
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/models 
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})