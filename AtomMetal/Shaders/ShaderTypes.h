#ifndef ShaderTypes_h
#define ShaderTypes_h

#include <simd/simd.h>

// Vertex input for orbital point cloud
typedef struct {
    simd_float3 position;
    simd_float4 color;
} OrbitalVertex;

// Per-frame uniform buffer
typedef struct {
    simd_float4x4 modelMatrix;
    simd_float4x4 viewMatrix;
    simd_float4x4 projectionMatrix;
    simd_float4x4 invViewProj;
    simd_float3   cameraPos;
    float         particleRadius;
    float         time;
    int           quantumN;
    int           quantumL;
    int           quantumM;
} FrameUniforms;

// Point charge struct (for Coulomb field rendering)
typedef struct {
    simd_float3 position;
    float       charge;
    simd_float4 color;
    float       radius;
} PointCharge;

// Sphere data for raytracer compute kernel
typedef struct {
    simd_float4 centerRadius;
    simd_float4 color;
} Sphere;

// Buffer indices
typedef enum {
    BufferIndexVertices = 0,
    BufferIndexUniforms = 1,
    BufferIndexSpheres  = 2,
    BufferIndexCharges  = 3,
    BufferIndexSphereCount = 4,
} BufferIndex;

#endif
