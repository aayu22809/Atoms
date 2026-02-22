cmake_minimum_required(VERSION 3.21)
project(AtomMetal LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD 17)

if(NOT APPLE)
  message(FATAL_ERROR "AtomMetal is macOS-only (Cocoa + Metal)")
endif()

# Source files
set(SOURCES
  Source/main.mm
  Source/AppDelegate.mm
  Source/MetalView.mm
  Source/Renderer.mm
  Source/Physics/WaveFunction.cpp
  Source/Physics/CDFSampler.cpp
  Source/Physics/ProbabilityCurrent.cpp
  Source/Scene/ParticleSystem.mm
  Source/Scene/PointCharge.mm
  Source/Scene/Grid.mm
  Source/HUD/HUDRenderer.mm
)

# Metal shaders (Xcode compiles these into default.metallib)
set(METAL_SHADERS
  Shaders/OrbitalShaders.metal
  Shaders/RaytracerShaders.metal
  Shaders/ChargeShaders.metal
  Shaders/GridShaders.metal
)

add_executable(AtomMetal MACOSX_BUNDLE ${SOURCES})

# Metal shaders: copy to bundle, compile at runtime (Metal toolchain may not be installed)
# Bundle layout: App.app/Contents/MacOS/AtomMetal, so Resources = $<TARGET_FILE_DIR>/../Resources
add_custom_command(TARGET AtomMetal POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:AtomMetal>/../Resources/Shaders
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ShaderTypes.h $<TARGET_FILE_DIR:AtomMetal>/../Resources/Shaders/
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/OrbitalShaders.metal $<TARGET_FILE_DIR:AtomMetal>/../Resources/Shaders/
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ChargeShaders.metal $<TARGET_FILE_DIR:AtomMetal>/../Resources/Shaders/
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/GridShaders.metal $<TARGET_FILE_DIR:AtomMetal>/../Resources/Shaders/
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/RaytracerShaders.metal $<TARGET_FILE_DIR:AtomMetal>/../Resources/Shaders/
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/HUDShaders.metal $<TARGET_FILE_DIR:AtomMetal>/../Resources/Shaders/
)

target_include_directories(AtomMetal PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Source
  ${CMAKE_CURRENT_SOURCE_DIR}/Source/Physics
  ${CMAKE_CURRENT_SOURCE_DIR}/Source/Scene
  ${CMAKE_CURRENT_SOURCE_DIR}/Source/HUD
  ${CMAKE_CURRENT_SOURCE_DIR}/Shaders
)

target_link_libraries(AtomMetal
  "-framework Metal"
  "-framework MetalKit"
  "-framework Cocoa"
  "-framework QuartzCore"
  "-framework Foundation"
  "-framework CoreGraphics"
)

set_target_properties(AtomMetal PROPERTIES
  MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.atommetal.app"
  XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "13.0"
)
