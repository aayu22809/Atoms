#import "Renderer.h"
#import "ShaderTypes.h"
#import "SimdMath.h"
#import "ParticleSystem.h"
#import "PointCharge.h"
#import "Grid.h"
#import "HUDRenderer.h"
#include <vector>

#define kMaxSpheres 300000
#include <fstream>
#include <sstream>
#include <mach-o/dyld.h>

static NSString* getShaderPath(NSString* name) {
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        NSString* exe = [NSString stringWithUTF8String:path];
        NSString* dir = [exe stringByDeletingLastPathComponent];
        return [[dir stringByAppendingPathComponent:@"../Resources/Shaders"]
                stringByAppendingPathComponent:name];
    }
    return [@"Shaders" stringByAppendingPathComponent:name];
}

static NSString* loadShaderSource(void) {
    NSMutableString* combined = [NSMutableString string];
    NSString* typesPath = getShaderPath(@"ShaderTypes.h");
    NSString* types = [NSString stringWithContentsOfFile:typesPath encoding:NSUTF8StringEncoding error:nil];
    if (types) [combined appendString:types];

    NSArray* metalFiles = @[@"OrbitalShaders.metal", @"ChargeShaders.metal", @"GridShaders.metal", @"RaytracerShaders.metal", @"HUDShaders.metal"];
    for (NSString* name in metalFiles) {
        NSString* path = getShaderPath(name);
        NSString* src = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil];
        if (src) {
            NSString* noInclude = [src stringByReplacingOccurrencesOfString:@"#include \"ShaderTypes.h\"\n" withString:@""];
            [combined appendString:noInclude];
        }
    }
    return combined;
}

@implementation Renderer {
    id<MTLDevice> _device;
    id<MTLCommandQueue> _commandQueue;
    id<MTLRenderPipelineState> _orbitalPipeline;
    id<MTLRenderPipelineState> _chargePipeline;
    id<MTLRenderPipelineState> _gridPipeline;
    id<MTLRenderPipelineState> _blitPipeline;
    id<MTLComputePipelineState> _raytracerPipeline;
    id<MTLDepthStencilState> _depthStencilState;
    id<MTLBuffer> _vertexBuffer;
    id<MTLBuffer> _uniformBuffer;
    id<MTLBuffer> _sphereBuffer;
    id<MTLBuffer> _chargeBuffer;
    id<MTLBuffer> _gridVertexBuffer;
    id<MTLBuffer> _sphereMeshBuffer;
    id<MTLBuffer> _quadVertexBuffer;
    id<MTLTexture> _raytracerTexture;
    NSUInteger _sphereVertexCount;
    NSUInteger _gridVertexCount;
    NSUInteger _particleDrawCount;
    HUDRenderer* _hud;
}

- (instancetype)initWithDevice:(id<MTLDevice>)device {
    self = [super init];
    if (!self) return nil;

    _device = device;
    _commandQueue = [device newCommandQueue];

    NSString* source = loadShaderSource();
    if (source.length == 0) {
        NSLog(@"Failed to load Metal shaders");
        return nil;
    }

    NSError* error = nil;
    MTLCompileOptions* opts = [MTLCompileOptions new];
    id<MTLLibrary> library = [device newLibraryWithSource:source options:opts error:&error];
    if (!library) {
        NSLog(@"Metal shader compile error: %@", error.localizedDescription);
        return nil;
    }

    id<MTLFunction> orbitalVert = [library newFunctionWithName:@"orbitalVertex"];
    id<MTLFunction> orbitalFrag = [library newFunctionWithName:@"orbitalFragment"];
    if (orbitalVert && orbitalFrag) {
        MTLRenderPipelineDescriptor* desc = [MTLRenderPipelineDescriptor new];
        desc.vertexFunction = orbitalVert;
        desc.fragmentFunction = orbitalFrag;
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
        _orbitalPipeline = [device newRenderPipelineStateWithDescriptor:desc error:&error];
    }

    id<MTLFunction> chargeVert = [library newFunctionWithName:@"chargeVertex"];
    id<MTLFunction> chargeFrag = [library newFunctionWithName:@"chargeFragment"];
    if (chargeVert && chargeFrag) {
        MTLRenderPipelineDescriptor* desc = [MTLRenderPipelineDescriptor new];
        desc.vertexFunction = chargeVert;
        desc.fragmentFunction = chargeFrag;
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
        _chargePipeline = [device newRenderPipelineStateWithDescriptor:desc error:&error];
    }

    id<MTLFunction> gridVert = [library newFunctionWithName:@"gridVertex"];
    id<MTLFunction> gridFrag = [library newFunctionWithName:@"gridFragment"];
    if (gridVert && gridFrag) {
        MTLRenderPipelineDescriptor* desc = [MTLRenderPipelineDescriptor new];
        desc.vertexFunction = gridVert;
        desc.fragmentFunction = gridFrag;
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
        desc.colorAttachments[0].blendingEnabled = YES;
        desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        _gridPipeline = [device newRenderPipelineStateWithDescriptor:desc error:&error];
    }

    id<MTLFunction> raytraceKernel = [library newFunctionWithName:@"raytracerKernel"];
    if (raytraceKernel) {
        _raytracerPipeline = [device newComputePipelineStateWithFunction:raytraceKernel error:&error];
    }

    id<MTLFunction> hudVert = [library newFunctionWithName:@"hudVertex"];
    id<MTLFunction> hudFrag = [library newFunctionWithName:@"hudFragment"];
    _hud = [[HUDRenderer alloc] initWithDevice:device];
    if (_hud && hudVert && hudFrag) {
        MTLRenderPipelineDescriptor* desc = [MTLRenderPipelineDescriptor new];
        desc.vertexFunction = hudVert;
        desc.fragmentFunction = hudFrag;
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        desc.colorAttachments[0].blendingEnabled = YES;
        desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        id<MTLRenderPipelineState> hudPipeline = [device newRenderPipelineStateWithDescriptor:desc error:&error];
        [_hud setPipeline:hudPipeline];
    }

    id<MTLFunction> blitVert = [library newFunctionWithName:@"blitVertex"];
    id<MTLFunction> blitFrag = [library newFunctionWithName:@"blitFragment"];
    if (blitVert && blitFrag) {
        MTLRenderPipelineDescriptor* desc = [MTLRenderPipelineDescriptor new];
        desc.vertexFunction = blitVert;
        desc.fragmentFunction = blitFrag;
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        _blitPipeline = [device newRenderPipelineStateWithDescriptor:desc error:&error];
    }

    MTLDepthStencilDescriptor* dsDesc = [MTLDepthStencilDescriptor new];
    dsDesc.depthCompareFunction = MTLCompareFunctionLess;
    dsDesc.depthWriteEnabled = YES;
    _depthStencilState = [device newDepthStencilStateWithDescriptor:dsDesc];

    std::vector<simd_float3> sphereVerts;
    float r = 1.0f;
    int stacks = 12, sectors = 16;
    for (int i = 0; i <= stacks; ++i) {
        float t1 = (float)i / stacks * (float)M_PI;
        float t2 = (float)(i + 1) / stacks * (float)M_PI;
        for (int j = 0; j < sectors; ++j) {
            float p1 = (float)j / sectors * 2.0f * (float)M_PI;
            float p2 = (float)(j + 1) / sectors * 2.0f * (float)M_PI;
            auto gp = [&](float t, float p) -> simd_float3 {
                return simd_make_float3(r * sinf(t) * cosf(p), r * cosf(t), r * sinf(t) * sinf(p));
            };
            sphereVerts.push_back(gp(t1, p1));
            sphereVerts.push_back(gp(t1, p2));
            sphereVerts.push_back(gp(t2, p1));
            sphereVerts.push_back(gp(t1, p2));
            sphereVerts.push_back(gp(t2, p2));
            sphereVerts.push_back(gp(t2, p1));
        }
    }
    _sphereVertexCount = sphereVerts.size();
    _sphereMeshBuffer = [device newBufferWithBytes:sphereVerts.data()
                                            length:sphereVerts.size() * sizeof(simd_float3)
                                           options:MTLResourceStorageModeShared];

    auto gridVerts = generateGridVertices(10, 5.0f);
    _gridVertexCount = gridVerts.size();
    _gridVertexBuffer = [device newBufferWithBytes:gridVerts.data()
                                           length:gridVerts.size() * sizeof(simd_float3)
                                          options:MTLResourceStorageModeShared];

    float quadVerts[] = {-1,-1, 1,-1, -1,1, -1,1, 1,-1, 1,1};
    _quadVertexBuffer = [device newBufferWithBytes:quadVerts length:sizeof(quadVerts) options:MTLResourceStorageModeShared];

    _vertexBuffer = [device newBufferWithLength:kMaxSpheres * sizeof(OrbitalVertex) options:MTLResourceStorageModeShared];
    _uniformBuffer = [device newBufferWithLength:sizeof(FrameUniforms) options:MTLResourceStorageModeShared];
    _sphereBuffer = [device newBufferWithLength:kMaxSpheres * sizeof(Sphere) options:MTLResourceStorageModeShared];
    _chargeBuffer = [device newBufferWithLength:16 * sizeof(PointCharge) options:MTLResourceStorageModeShared];

    return self;
}

- (void)uploadParticles:(const std::vector<Particle>&)particles
            cutaway:(BOOL)cutaway
         particleRadius:(float)radius {
    _particleDrawCount = 0;
    OrbitalVertex* dst = (OrbitalVertex*)_vertexBuffer.contents;
    for (const auto& p : particles) {
        if (cutaway && p.pos.x < 0 && p.pos.y > 0) continue;
        dst[_particleDrawCount].position = p.pos;
        dst[_particleDrawCount].color = p.color;
        _particleDrawCount++;
        if (_particleDrawCount >= kMaxSpheres) break;
    }
    (void)radius;
}

- (void)updateHUDWithN:(int)n L:(int)l M:(int)m count:(int)count mode:(int)mode animating:(BOOL)anim {
    [_hud updateWithN:n L:l M:m particleCount:count renderMode:mode animating:anim];
}

- (void)drawInMTKView:(MTKView*)view
           particles:(const std::vector<Particle>&)particles
             charges:(const std::vector<PointChargeData>&)charges
          uniforms:(FrameUniforms)uniforms
        cutaway:(BOOL)cutaway
     renderMode:(int)renderMode {
    id<MTLCommandBuffer> cmd = [_commandQueue commandBuffer];
    if (!cmd) return;

    memcpy(_uniformBuffer.contents, &uniforms, sizeof(FrameUniforms));

    MTLRenderPassDescriptor* rpd = view.currentRenderPassDescriptor;
    id<MTLDrawable> drawable = view.currentDrawable;
    if (!rpd || !drawable) return;

    rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
    rpd.colorAttachments[0].clearColor = MTLClearColorMake(0.04, 0.04, 0.08, 1.0);

    if (renderMode == 1) {
        MTLRenderPassDescriptor* rtRpd = view.currentRenderPassDescriptor;
        [self drawRaytraced:cmd view:view rpd:rtRpd particles:particles charges:charges uniforms:uniforms cutaway:cutaway];
    } else {
        [self drawRealtime:cmd rpd:rpd particles:particles charges:charges cutaway:cutaway];
    }

    [_hud drawInCommandBuffer:cmd renderPassDescriptor:rpd];
    [cmd presentDrawable:drawable];
    [cmd commit];
}

- (void)drawRealtime:(id<MTLCommandBuffer>)cmd
                rpd:(MTLRenderPassDescriptor*)rpd
           particles:(const std::vector<Particle>&)particles
             charges:(const std::vector<PointChargeData>&)charges
            cutaway:(BOOL)cutaway {
    [self uploadParticles:particles cutaway:cutaway particleRadius:1.5f];

    id<MTLRenderCommandEncoder> enc = [cmd renderCommandEncoderWithDescriptor:rpd];
    [enc setDepthStencilState:_depthStencilState];

    if (_gridPipeline && _gridVertexCount > 0) {
        [enc setRenderPipelineState:_gridPipeline];
        [enc setVertexBuffer:_gridVertexBuffer offset:0 atIndex:0];
        [enc setVertexBuffer:_uniformBuffer offset:0 atIndex:1];
        [enc drawPrimitives:MTLPrimitiveTypeLine vertexStart:0 vertexCount:_gridVertexCount];
    }

    if (_orbitalPipeline && _particleDrawCount > 0) {
        [enc setRenderPipelineState:_orbitalPipeline];
        [enc setVertexBuffer:_vertexBuffer offset:0 atIndex:0];
        [enc setVertexBuffer:_uniformBuffer offset:0 atIndex:1];
        [enc setFragmentBuffer:_uniformBuffer offset:0 atIndex:1];
        [enc drawPrimitives:MTLPrimitiveTypePoint vertexStart:0 vertexCount:_particleDrawCount];
    }

    if (_chargePipeline && !charges.empty()) {
        memcpy(_chargeBuffer.contents, charges.data(), charges.size() * sizeof(PointChargeData));
        [enc setRenderPipelineState:_chargePipeline];
        [enc setVertexBuffer:_sphereMeshBuffer offset:0 atIndex:0];
        [enc setVertexBuffer:_uniformBuffer offset:0 atIndex:1];
        [enc setVertexBuffer:_chargeBuffer offset:0 atIndex:3];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:_sphereVertexCount instanceCount:charges.size()];
    }

    [enc endEncoding];
}

- (void)drawRaytraced:(id<MTLCommandBuffer>)cmd
                 view:(MTKView*)view
                  rpd:(MTLRenderPassDescriptor*)rpDesc
            particles:(const std::vector<Particle>&)particles
              charges:(const std::vector<PointChargeData>&)charges
           uniforms:(FrameUniforms)uniforms
             cutaway:(BOOL)cutaway {
    NSUInteger count = 0;
    Sphere* dst = (Sphere*)_sphereBuffer.contents;
    for (const auto& p : particles) {
        if (cutaway && p.pos.x < 0 && p.pos.y > 0) continue;
        dst[count].centerRadius = simd_make_float4(p.pos.x, p.pos.y, p.pos.z, 0.25f);
        dst[count].color = p.color;
        count++;
        if (count >= kMaxSpheres) break;
    }

    NSUInteger w = view.drawableSize.width;
    NSUInteger h = view.drawableSize.height;
    if (w < 1 || h < 1 || !_raytracerPipeline) return;

    if (!_raytracerTexture || _raytracerTexture.width != w || _raytracerTexture.height != h) {
        MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:w height:h mipmapped:NO];
        texDesc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
        _raytracerTexture = [_device newTextureWithDescriptor:texDesc];
    }

    id<MTLComputeCommandEncoder> cenc = [cmd computeCommandEncoder];
    [cenc setComputePipelineState:_raytracerPipeline];
    [cenc setTexture:_raytracerTexture atIndex:0];
    [cenc setBuffer:_sphereBuffer offset:0 atIndex:2];
    [cenc setBuffer:_uniformBuffer offset:0 atIndex:1];
    uint32_t sc = (uint32_t)count;
    [cenc setBytes:&sc length:sizeof(sc) atIndex:4];
    [cenc dispatchThreadgroups:MTLSizeMake((w+7)/8, (h+7)/8, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
    [cenc endEncoding];

    if (rpDesc && _blitPipeline) {
        id<MTLRenderCommandEncoder> renc = [cmd renderCommandEncoderWithDescriptor:rpDesc];
        [renc setRenderPipelineState:_blitPipeline];
        [renc setVertexBuffer:_quadVertexBuffer offset:0 atIndex:0];
        [renc setFragmentTexture:_raytracerTexture atIndex:0];
        [renc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
        [renc endEncoding];
    }
    (void)charges;
    (void)uniforms;
}

@end
