cmake_minimum_required(VERSION 3.16)
project(Atoms LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)

if(NOT APPLE)
  message(FATAL_ERROR "This project uses Metal and is macOS-only.")
endif()

# Dependencies
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# metal-cpp (header-only)
include(FetchContent)
FetchContent_Declare(
  metal-cpp
  GIT_REPOSITORY https://github.com/bkaradzic/metal-cpp
  GIT_TAG metal-cpp_26
)
FetchContent_Populate(metal-cpp)

# Metal shaders: compiled at runtime (no build-time Metal toolchain required)

# Common Metal setup for all targets
set(METAL_FRAMEWORKS "-framework Metal -framework Foundation -framework QuartzCore -framework AppKit -framework CoreGraphics")
set(METAL_INCLUDES ${metal-cpp_SOURCE_DIR})

# metal_test - Phase 1 verification (empty window, clear color)
add_executable(metal_test
  src/metal_test.cpp
  src/metal_bridge.mm
)
target_include_directories(metal_test PRIVATE ${METAL_INCLUDES})
target_link_libraries(metal_test PRIVATE glfw ${METAL_FRAMEWORKS})

# atom - 2D Bohr model (Metal)
add_executable(atom src/atom.cpp src/metal_bridge.mm)
target_include_directories(atom PRIVATE ${METAL_INCLUDES})
target_link_libraries(atom PRIVATE glfw glm::glm ${METAL_FRAMEWORKS})

# atom_realtime - Real-time 3D particle visualization (Metal, instanced)
add_executable(atom_realtime src/atom_realtime.cpp src/metal_bridge.mm)
target_include_directories(atom_realtime PRIVATE ${METAL_INCLUDES})
target_link_libraries(atom_realtime PRIVATE glfw glm::glm ${METAL_FRAMEWORKS})

# atom_raytracer - Ray-traced 3D particle visualization (Metal)
add_executable(atom_raytracer src/atom_raytracer.cpp src/metal_bridge.mm)
target_include_directories(atom_raytracer PRIVATE ${METAL_INCLUDES})
target_link_libraries(atom_raytracer PRIVATE glfw glm::glm ${METAL_FRAMEWORKS})

# wave_atom_2d - 2D wave visualization (Metal)
add_executable(wave_atom_2d src/wave_atom_2d.cpp src/metal_bridge.mm)
target_include_directories(wave_atom_2d PRIVATE ${METAL_INCLUDES})
target_link_libraries(wave_atom_2d PRIVATE glfw glm::glm ${METAL_FRAMEWORKS})
