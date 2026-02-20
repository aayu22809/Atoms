#import "MetalView.h"
#import "Renderer.h"
#import "SimdMath.h"
#import "ParticleSystem.h"
#import "PointCharge.h"
#import "CDFSampler.h"
#include <random>
#include <simd/simd.h>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

@interface MetalView ()
@property (nonatomic, strong) Renderer* renderer;
@property (nonatomic) std::vector<Particle> particles;
@property (nonatomic) std::vector<PointChargeData> charges;
@property (nonatomic) std::unordered_map<uint64_t, CDFCache> cdfCache;
@property (nonatomic) std::mt19937 rng;
@property (nonatomic) int n, l, m, particleCount;
@property (nonatomic) int colorMode;
@property (nonatomic) int renderMode;
@property (nonatomic) BOOL cutaway;
@property (nonatomic) BOOL animating;
@property (nonatomic) float azimuth, elevation, radius;
@property (nonatomic) float particleRadius;
@property (nonatomic) BOOL dragging;
@property (nonatomic) NSPoint lastMouse;
@end

@implementation MetalView

- (instancetype)initWithFrame:(NSRect)frame device:(id<MTLDevice>)device {
    self = [super initWithFrame:frame device:device];
    if (self) {
        self.delegate = self;
        self.clearColor = MTLClearColorMake(0.04, 0.04, 0.08, 1.0);
        self.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
        self.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
        self.preferredFramesPerSecond = 60;

        _renderer = [[Renderer alloc] initWithDevice:device];
        _rng = std::mt19937(std::random_device{}());
        _n = 2; _l = 1; _m = 0;
        _particleCount = 250000;
        _colorMode = 0;
        _renderMode = 0;
        _cutaway = NO;
        _animating = YES;
        _azimuth = 0; _elevation = (float)(M_PI / 2.0); _radius = 50.0f;
        _particleRadius = 1.5f;
        _dragging = NO;

        PointChargeData nucleus;
        nucleus.position = simd_make_float3(0, 0, 0);
        nucleus.charge = 1.0f;
        nucleus.color = simd_make_float4(1, 0.2f, 0.2f, 1);
        nucleus.radius = 2.0f;
        _charges.push_back(nucleus);

        [self regenerateParticles];
    }
    return self;
}

- (void)regenerateParticles {
    generateParticles(_n, _l, _m, _particleCount, _particles, _cdfCache, _rng, _colorMode);
    _particleRadius = (float)_n / 3.0f;
}

- (simd_float3)cameraPosition {
    float e = _elevation;
    float a = _azimuth;
    float r = _radius;
    return simd_make_float3(r * sinf(e) * cosf(a), r * cosf(e), r * sinf(e) * sinf(a));
}

- (simd_float4x4)viewMatrix {
    return simd_lookAt([self cameraPosition], simd_make_float3(0, 0, 0), simd_make_float3(0, 1, 0));
}

- (simd_float4x4)projectionMatrix {
    float aspect = (float)self.drawableSize.width / (float)self.drawableSize.height;
    if (aspect < 0.01f) aspect = 1.0f;
    return simd_perspective((float)(M_PI / 4.0), aspect, 0.1f, 2000.0f);
}

- (void)drawInMTKView:(MTKView*)view {
    if (!_renderer) return;

    if (_animating) {
        updateParticles(_particles, _m, 0.5f);
    }

    FrameUniforms u;
    u.modelMatrix = simd_identity();
    u.viewMatrix = [self viewMatrix];
    u.projectionMatrix = [self projectionMatrix];
    simd_float4x4 vp = simd_mul(u.projectionMatrix, u.viewMatrix);
    u.invViewProj = simd_inverse4x4(vp);
    u.cameraPos = [self cameraPosition];
    u.particleRadius = _particleRadius;
    u.time = 0;
    u.quantumN = _n;
    u.quantumL = _l;
    u.quantumM = _m;

    [_renderer updateHUDWithN:_n L:_l M:_m count:_particleCount mode:_renderMode animating:_animating];
    [_renderer drawInMTKView:view
                   particles:_particles
                     charges:_charges
                  uniforms:u
                cutaway:_cutaway
             renderMode:_renderMode];
}

- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size {
    (void)view;
    (void)size;
}

- (void)mouseDown:(NSEvent*)event {
    _dragging = YES;
    _lastMouse = [self convertPoint:event.locationInWindow fromView:nil];
}

- (void)mouseUp:(NSEvent*)event {
    (void)event;
    _dragging = NO;
}

- (void)mouseDragged:(NSEvent*)event {
    if (!_dragging) return;
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    float dx = (float)(p.x - _lastMouse.x) * 0.01f;
    float dy = (float)(p.y - _lastMouse.y) * 0.01f;
    _lastMouse = p;
    _azimuth += dx;
    _elevation -= dy;
    _elevation = fmax(0.01f, fmin((float)(M_PI - 0.01), _elevation));
}

- (void)scrollWheel:(NSEvent*)event {
    _radius -= (float)event.scrollingDeltaY * 2.0f;
    _radius = fmax(1.0f, _radius);
}

- (void)keyDown:(NSEvent*)event {
    BOOL changed = NO;
    switch (event.keyCode) {
        case 13: _n = std::min(_n + 1, 6); changed = YES; break;
        case 1:  _n = std::max(_n - 1, 1); changed = YES; break;
        case 14: _l = std::min(_l + 1, _n - 1); changed = YES; break;
        case 2:  _l = std::max(_l - 1, 0); changed = YES; break;
        case 15: _m = std::min(_m + 1, _l); changed = YES; break;
        case 3:  _m = std::max(_m - 1, -_l); changed = YES; break;
        case 17: _particleCount = std::min(_particleCount + 100000, 1000000); changed = YES; break;
        case 5:  _particleCount = std::max(_particleCount - 100000, 10000); changed = YES; break;
        case 8:  _colorMode = (_colorMode + 1) % 2; changed = YES; break;
        case 9:  _renderMode = (_renderMode + 1) % 2; break;
        case 7:  _cutaway = !_cutaway; break;
        case 49: _animating = !_animating; break;
        default: break;
    }
    _l = std::clamp(_l, 0, _n - 1);
    _m = std::clamp(_m, -_l, _l);
    if (changed) [self regenerateParticles];
    float energy = -13.6f / (float)(_n * _n);
    NSLog(@"n=%d l=%d m=%d N=%d E=%.2feV", _n, _l, _m, _particleCount, energy);
}

- (BOOL)acceptsFirstResponder { return YES; }

@end
