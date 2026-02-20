#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float4 color;
};

struct Uniforms {
    float4x4 ortho;
};

// Vertex buffer: packed float2 position + float4 color per vertex
vertex VertexOut vertex2d(constant float* vertices [[buffer(0)]], uint vid [[vertex_id]], constant Uniforms& u [[buffer(1)]]) {
    float2 pos = float2(vertices[vid * 6], vertices[vid * 6 + 1]);
    float4 col = float4(vertices[vid * 6 + 2], vertices[vid * 6 + 3], vertices[vid * 6 + 4], vertices[vid * 6 + 5]);
    VertexOut out;
    out.position = u.ortho * float4(pos, 0.0, 1.0);
    out.color = col;
    return out;
}

fragment float4 fragment2d(VertexOut in [[stage_in]]) {
    return in.color;
}
