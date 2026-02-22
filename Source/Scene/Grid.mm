#import "Grid.h"

std::vector<simd_float3> generateGridVertices(int linesPerAxis, float spacing) {
    std::vector<simd_float3> verts;
    float half = (linesPerAxis / 2) * spacing;
    for (int i = 0; i <= linesPerAxis; ++i) {
        float x = -half + i * spacing;
        verts.push_back(simd_make_float3(x, 0, -half));
        verts.push_back(simd_make_float3(x, 0, half));
        verts.push_back(simd_make_float3(-half, 0, x));
        verts.push_back(simd_make_float3(half, 0, x));
    }
    return verts;
}
