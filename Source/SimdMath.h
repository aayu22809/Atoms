#ifndef SimdMath_h
#define SimdMath_h

#include <simd/simd.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline simd_float4x4 simd_lookAt(simd_float3 eye, simd_float3 target, simd_float3 up) {
    simd_float3 f = simd_normalize(target - eye);
    simd_float3 r = simd_normalize(simd_cross(f, up));
    simd_float3 u = simd_cross(r, f);
    return simd_matrix(
        simd_make_float4(r.x, u.x, -f.x, 0),
        simd_make_float4(r.y, u.y, -f.y, 0),
        simd_make_float4(r.z, u.z, -f.z, 0),
        simd_make_float4(-simd_dot(r, eye), -simd_dot(u, eye), simd_dot(f, eye), 1)
    );
}

static inline simd_float4x4 simd_perspective(float fovY, float aspect, float near, float far) {
    float tanHalf = tanf(fovY / 2.0f);
    float sy = 1.0f / tanHalf;
    float sx = sy / aspect;
    float a = -(far + near) / (far - near);
    float b = -(2.0f * far * near) / (far - near);
    return simd_matrix(
        simd_make_float4(sx, 0, 0, 0),
        simd_make_float4(0, sy, 0, 0),
        simd_make_float4(0, 0, a, -1),
        simd_make_float4(0, 0, b, 0)
    );
}

static inline simd_float4x4 simd_identity(void) {
    return matrix_identity_float4x4;
}

static inline simd_float4x4 simd_inverse4x4(simd_float4x4 m) {
    float* a = (float*)&m;
    float inv[16], det;
    inv[0]  =  a[5]*a[10]*a[15] - a[5]*a[11]*a[14] - a[9]*a[6]*a[15] + a[9]*a[7]*a[14] + a[13]*a[6]*a[11] - a[13]*a[7]*a[10];
    inv[4]  = -a[4]*a[10]*a[15] + a[4]*a[11]*a[14] + a[8]*a[6]*a[15] - a[8]*a[7]*a[14] - a[12]*a[6]*a[11] + a[12]*a[7]*a[10];
    inv[8]  =  a[4]*a[9]*a[15]  - a[4]*a[11]*a[13] - a[8]*a[5]*a[15] + a[8]*a[7]*a[13] + a[12]*a[5]*a[11] - a[12]*a[7]*a[9];
    inv[12] = -a[4]*a[9]*a[14]  + a[4]*a[10]*a[13] + a[8]*a[5]*a[14] - a[8]*a[6]*a[13] - a[12]*a[5]*a[10] + a[12]*a[6]*a[9];
    inv[1]  = -a[1]*a[10]*a[15] + a[1]*a[11]*a[14] + a[9]*a[2]*a[15] - a[9]*a[3]*a[14] - a[13]*a[2]*a[11] + a[13]*a[3]*a[10];
    inv[5]  =  a[0]*a[10]*a[15] - a[0]*a[11]*a[14] - a[8]*a[2]*a[15] + a[8]*a[3]*a[14] + a[12]*a[2]*a[11] - a[12]*a[3]*a[10];
    inv[9]  = -a[0]*a[9]*a[15]  + a[0]*a[11]*a[13] + a[8]*a[1]*a[15] - a[8]*a[3]*a[13] - a[12]*a[1]*a[11] + a[12]*a[3]*a[9];
    inv[13] =  a[0]*a[9]*a[14]  - a[0]*a[10]*a[13] - a[8]*a[1]*a[14] + a[8]*a[2]*a[13] + a[12]*a[1]*a[10] - a[12]*a[2]*a[9];
    inv[2]  =  a[1]*a[6]*a[15]  - a[1]*a[7]*a[14]  - a[5]*a[2]*a[15] + a[5]*a[3]*a[14] + a[13]*a[2]*a[7]  - a[13]*a[3]*a[6];
    inv[6]  = -a[0]*a[6]*a[15]  + a[0]*a[7]*a[14]  + a[4]*a[2]*a[15] - a[4]*a[3]*a[14] - a[12]*a[2]*a[7]  + a[12]*a[3]*a[6];
    inv[10] =  a[0]*a[5]*a[15]  - a[0]*a[7]*a[13]  - a[4]*a[1]*a[15] + a[4]*a[3]*a[13] + a[12]*a[1]*a[7]  - a[12]*a[3]*a[5];
    inv[14] = -a[0]*a[5]*a[14]  + a[0]*a[6]*a[13]  + a[4]*a[1]*a[14] - a[4]*a[2]*a[13] - a[12]*a[1]*a[6]  + a[12]*a[2]*a[5];
    inv[3]  = -a[1]*a[6]*a[11]  + a[1]*a[7]*a[10]  + a[5]*a[2]*a[11] - a[5]*a[3]*a[10] - a[9]*a[2]*a[7]   + a[9]*a[3]*a[6];
    inv[7]  =  a[0]*a[6]*a[11]  - a[0]*a[7]*a[10]  - a[4]*a[2]*a[11] + a[4]*a[3]*a[10] + a[8]*a[2]*a[7]   - a[8]*a[3]*a[6];
    inv[11] = -a[0]*a[5]*a[11]  + a[0]*a[7]*a[9]   + a[4]*a[1]*a[11] - a[4]*a[3]*a[9]  - a[8]*a[1]*a[7]   + a[8]*a[3]*a[5];
    inv[15] =  a[0]*a[5]*a[10]  - a[0]*a[6]*a[9]   - a[4]*a[1]*a[10] + a[4]*a[2]*a[9]  + a[8]*a[1]*a[6]   - a[8]*a[2]*a[5];
    det = a[0]*inv[0] + a[1]*inv[4] + a[2]*inv[8] + a[3]*inv[12];
    if (fabsf(det) < 1e-9f) return matrix_identity_float4x4;
    det = 1.0f / det;
    simd_float4x4 result;
    float* r = (float*)&result;
    for (int i = 0; i < 16; i++) r[i] = inv[i] * det;
    return result;
}

#endif
