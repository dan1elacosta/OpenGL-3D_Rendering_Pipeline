# Escenario 3D Interactivo con Iluminación Phong y Trayectoria de Bézier (OpenGL 3.3)

Proyecto de Computación Gráfica desarrollado en **C++** y **OpenGL 3.3 Core Profile**. La escena representa un entorno montañoso nevado estático sobrevolado por una nave espacial animada mediante una **Curva de Bézier Cúbica**, incorporando orientación tangencial dinámica, alabeo, cámara libre en primera persona e iluminación Phong diferenciada por material.

---

## Arquitectura del Proyecto

Para mantener el código limpio, modular y fácil de sustentar, la lógica gráfica se dividió en dos archivos principales:

* **`Trabajo_Modelo3D_CG.cpp`**: Contiene el punto de entrada (`main`), la configuración de ventana y cámara, la carga de modelos 3D (`.obj` y `.glb`), las funciones matemáticas de la trayectoria de Bézier y el bucle principal de renderizado con las transformaciones matriciales.
* **`ShaderLighting.h`**: Encapsula el código fuente GLSL del *Vertex Shader* y *Fragment Shader*, la compilación del programa de shaders y las funciones auxiliares para configurar la iluminación ambiental/especular de la escena.

---

## Transformaciones Matriciales en el Pipeline 3D

En cada fotograma, los vértices pasan por una cadena de transformaciones afines y proyectivas antes de llegar a pantalla:

### 1. Matriz de Modelo (`model`)
Posiciona, orienta y escala cada objeto desde su espacio local (*Local Space*) hacia el espacio global de la escena (*World Space*).
* **Para el terreno estático (`mapModel`)**: Se aplica un escalado uniforme (`mapScale`) para normalizar su dimensión máxima a `25.0` unidades y una traslación (`-mapCenter`) para centrar su base en el origen `(0, 0, 0)`.
* **Para la nave animada (`shipModel`)**: Se construye siguiendo el orden estricto de multiplicación matricial en OpenGL ($T \cdot R_y \cdot R_x \cdot R_z \cdot S \cdot T_{centro}$):
  1. **Centrado local (`-shipCenter`) y escalado (`shipScale`)**: Garantiza que la nave rote sobre su propio centro geométrico.
  2. **Alabeo / *Roll* (`R_z`)**: Rotación sinusoidal sobre el eje longitudinal local para simular inclinación aerodinámica en vuelo.
  3. **Cabeceo / *Pitch* (`R_x`)**: Inclinación vertical calculada a partir de la componente $Y$ del vector tangente.
  4. **Guiñada / *Yaw* (`R_y`)**: Orientación horizontal calculada mediante `atan2(tangent.x, tangent.z)` para que la proa apunte siempre hacia la dirección del desplazamiento.
  5. **Traslación (`bezierPos`)**: Ubica la nave en el punto exacto $B(t)$ de la curva de Bézier.

### 2. Matriz Normal (`normalMatrix`)
Calculada como la transpuesta de la inversa de la submatriz $3 \times 3$ de `model`:
$$\text{normalMatrix} = (M_{3\times 3}^{-1})^T$$
Esta matriz es indispensable cuando se aplican escalados o rotaciones dinámicas (como en la nave), ya que evita que los vectores normales se deformen y asegura que el cálculo de iluminación Phong refleje la luz correctamente en cada cara.

### 3. Matriz de Vista (`view`)
Generada mediante `glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp)`. Transforma las coordenadas del mundo al espacio de la cámara (*View Space*), permitiendo navegar libremente por la escena con teclado y ratón.

### 4. Matriz de Proyección (`projection`)
Construida con `glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 200.0f)`. Aplica proyección en perspectiva para simular profundidad visual y recortar objetos fuera del *frustum* de visión.

---

## Funciones Principales Implementadas

### Trayectoria y Orientación Matemática
* `evaluateBezier(t, P0, P1, P2, P3)`: Evalúa el polinomio de Bernstein de grado 3 para obtener la posición espacial $B(t)$ de la nave sobre cuatro puntos de control elevados sobre el valle:
  $$B(t) = (1-t)^3 P_0 + 3(1-t)^2 t P_1 + 3(1-t) t^2 P_2 + t^3 P_3, \quad t \in [0, 1]$$
* `evaluateBezierTangent(t, P0, P1, P2, P3)`: Calcula la primera derivada $B'(t)$ normalizada para obtener el vector director tangente en cada instante, permitiendo alinear la orientación de la nave con la curvatura de su ruta:
  $$B'(t) = 3(1-t)^2(P_1 - P_0) + 6(1-t)t(P_2 - P_1) + 3t^2(P_3 - P_2)$$

### Carga de Geometría y Materiales
* `loadGLBModel(...)`: Parser personalizado sobre `tinygltf` y `stb_image` que lee archivos binarios `.glb`, extrae posiciones, normales y coordenadas UV (`TEXCOORD_0`), decodifica la textura incrustada en memoria (`stbi_load_from_memory`) y empaqueta los vértices en un formato intercalado (`Posición + Color RGB + Normal`).

### Control e Interacción
* `processInput(window)` y `mouse_callback(...)`: Gestionan el desplazamiento continuo en 3D (`W, A, S, D`, `Espacio`, `Shift`), el reinicio de cámara (`R`) y la rotación de vista mediante ángulos de Euler (*Yaw* y *Pitch*).

### Iluminación y Shaders (`ShaderLighting.h`)
* `createPhongShaderProgram()`: Compila y enlaza el *Vertex* y *Fragment Shader* en un único `GLuint` listo para usarse.
* `setupSceneLighting(...)`: Envía al shader la posición de la fuente de luz direccional/puntual, su color cálido y la posición actual del observador (`viewPos`).
* `setMaterialPhong(program, specularStrength, shininess)`: Ajusta dinámicamente las propiedades especulares del modelo de Phong según el objeto dibujado:
  * **Terreno nevado**: Acabado mate (`specularStrength = 0.25`, `shininess = 16.0`) para evitar brillos plásticos en la nieve y vegetación.
  * **Nave espacial**: Acabado metálico (`specularStrength = 0.95`, `shininess = 64.0`) para resaltar reflejos nítidos en el fuselaje.

---

## Modelos 3D y Elección de Formatos

Ambos modelos fueron obtenidos de la biblioteca abierta **[Poly Pizza](https://poly.pizza/)**:

| Elemento en Escena | Formato | Archivo | Enlace de Descarga | Justificación Técnica del Formato |
| :--- | :---: | :--- | :--- | :--- |
| **Paisaje Nevado (Estático)** | `.obj` + `.mtl` | `models/MAPA NEVADO/model.obj` | `[PEGAR LINK DE POLY PIZZA AQUÍ]` | Ideal para geometría estática compuesta por múltiples mallas y materiales difusos planos definidos en su archivo `.mtl` mediante `tiny_obj_loader`. |
| **Nave Espacial (Animada)** | `.glb` | `models/SPACESHIP/SPACESHIP.glb` | `[PEGAR LINK DE POLY PIZZA AQUÍ]` | Formato binario autocontenido (glTF 2.0) que empaqueta malla, normales, coordenadas UV y textura en un solo archivo compacto, facilitando su carga y transformación dinámica en tiempo real. |

---

## Controles de Ejecución

* **`W` / `A` / `S` / `D`**: Desplazar cámara adelante, izquierda, atrás y derecha.
* **`Espacio` / `Shift Izq.`**: Elevar o descender verticalmente.
* **`Ratón`**: Rotar vista libremente.
* **`R`**: Restablecer cámara a la posición panorámica inicial.
* **`ESC`**: Cerrar la aplicación.

---

## Dependencias Utilizadas

* **GLFW** (Gestión de ventana, contexto OpenGL y eventos de entrada)
* **GLAD** (Carga de punteros a funciones de OpenGL 3.3 Core)
* **GLM** (Matemática vectorial y matricial 3D)
* **tinyobjloader** (Carga de modelos `.obj` y `.mtl`)
* **tinygltf** + **stb_image** (Carga de modelos `.glb` y decodificación de texturas en memoria)