cmake_minimum_required(VERSION 3.16)
project(Atoms LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# Dependencies
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# macOS: silence OpenGL deprecation warnings (OpenGL deprecated since 10.14 but still works)
if(APPLE)
  add_compile_definitions(GL_SILENCE_DEPRECATION)
endif()

# Common libraries for all executables
set(ATOM_LIBS OpenGL::GL GLEW::GLEW glfw glm::glm)

# atom - 2D Bohr model
add_executable(atom src/atom.cpp)
target_link_libraries(atom PRIVATE ${ATOM_LIBS})

# atom_raytracer - Ray-traced 3D visualization
add_executable(atom_raytracer src/atom_raytracer.cpp)
target_link_libraries(atom_raytracer PRIVATE ${ATOM_LIBS})

# atom_realtime - Real-time 3D particle visualization (uses GLU)
add_executable(atom_realtime src/atom_realtime.cpp)
target_link_libraries(atom_realtime PRIVATE ${ATOM_LIBS} OpenGL::GLU)

# wave_atom_2d - 2D wave visualization
add_executable(wave_atom_2d src/wave_atom_2d.cpp)
target_link_libraries(wave_atom_2d PRIVATE ${ATOM_LIBS})
