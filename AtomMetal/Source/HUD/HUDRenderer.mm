#import "HUDRenderer.h"
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#import <AppKit/AppKit.h>
#include <cstdio>

@implementation HUDRenderer {
    id<MTLDevice> _device;
    id<MTLTexture> _hudTexture;
    id<MTLRenderPipelineState> _pipeline;
    id<MTLBuffer> _quadBuffer;
    CGContextRef _cgContext;
    void* _bitmapData;
    size_t _width;
    size_t _height;
    int _n, _l, _m, _count, _mode;
    BOOL _animating;
}

@synthesize pipeline = _pipeline;

- (instancetype)initWithDevice:(id<MTLDevice>)device {
    self = [super init];
    if (!self) return nil;
    _device = device;
    _width = 600;
    _height = 160;

    _bitmapData = calloc(_width * _height, 4);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    _cgContext = CGBitmapContextCreate(_bitmapData, _width, _height, 8, _width * 4, cs,
                                       kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
    CGColorSpaceRelease(cs);
    if (!_cgContext) { free(_bitmapData); _bitmapData = nil; return nil; }

    MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                       width:_width
                                                                                      height:_height
                                                                                   mipmapped:NO];
    texDesc.usage = MTLTextureUsageShaderRead;
    _hudTexture = [device newTextureWithDescriptor:texDesc];

    float quadVerts[] = {
        -1.0f, 1.0f, 0, 0,
        -1.0f, 0.60f, 0, 1,
         0.0f, 1.0f, 1, 0,
        -1.0f, 0.60f, 0, 1,
         0.0f, 0.60f, 1, 1,
         0.0f, 1.0f, 1, 0
    };
    _quadBuffer = [device newBufferWithBytes:quadVerts length:sizeof(quadVerts) options:MTLResourceStorageModeShared];

    return self;
}

- (void)dealloc {
    if (_cgContext) { CGContextRelease(_cgContext); _cgContext = nil; }
    if (_bitmapData) { free(_bitmapData); _bitmapData = nil; }
}

- (void)updateWithN:(int)n L:(int)l M:(int)m particleCount:(int)count renderMode:(int)mode animating:(BOOL)anim {
    _n = n; _l = l; _m = m; _count = count; _mode = mode; _animating = anim;
    [self renderHUD];
}

- (void)renderHUD {
    if (!_cgContext || !_bitmapData || !_hudTexture) return;

    CGContextClearRect(_cgContext, CGRectMake(0, 0, (CGFloat)_width, (CGFloat)_height));
    CGContextSetRGBFillColor(_cgContext, 0.0, 0.0, 0.0, 0.55);
    CGContextFillRect(_cgContext, CGRectMake(0, 0, (CGFloat)_width, (CGFloat)_height));

    const char* sublabels = "spdfgh";
    float energy = -13.6f / (float)(_n * _n);
    char tmp[256];

    NSFont* mainFont = [NSFont monospacedSystemFontOfSize:15 weight:NSFontWeightMedium];
    NSFont* smallFont = [NSFont monospacedSystemFontOfSize:11 weight:NSFontWeightRegular];

    snprintf(tmp, sizeof(tmp), "Orbital: %d%c  n=%d l=%d m=%d  E=%.2f eV  N=%d  [%s]%s",
             _n, (_l < 6 ? sublabels[_l] : '?'), _n, _l, _m, energy, _count,
             (_mode == 0 ? "Realtime" : "Raytraced"), (_animating ? "" : " [PAUSED]"));
    NSString* line1 = [NSString stringWithUTF8String:tmp];

    snprintf(tmp, sizeof(tmp), "[W/S] n   [E/D] l   [R/F] m   [T/G] particles   [V] mode   [X] cutaway   [Space] pause");
    NSString* line2 = [NSString stringWithUTF8String:tmp];

    NSDictionary* attr1 = @{
        NSFontAttributeName: mainFont,
        NSForegroundColorAttributeName: [NSColor whiteColor]
    };
    NSDictionary* attr2 = @{
        NSFontAttributeName: smallFont,
        NSForegroundColorAttributeName: [NSColor colorWithWhite:0.7 alpha:1.0]
    };

    CGFloat y = (CGFloat)_height - 35;
    NSAttributedString* as1 = [[NSAttributedString alloc] initWithString:line1 attributes:attr1];
    CTLineRef ct1 = CTLineCreateWithAttributedString((CFAttributedStringRef)as1);
    CGContextSetTextPosition(_cgContext, 10, y);
    CTLineDraw(ct1, _cgContext);
    CFRelease(ct1);

    y -= 22;
    NSAttributedString* as2 = [[NSAttributedString alloc] initWithString:line2 attributes:attr2];
    CTLineRef ct2 = CTLineCreateWithAttributedString((CFAttributedStringRef)as2);
    CGContextSetTextPosition(_cgContext, 10, y);
    CTLineDraw(ct2, _cgContext);
    CFRelease(ct2);

    [_hudTexture replaceRegion:MTLRegionMake2D(0, 0, _width, _height)
                   mipmapLevel:0
                     withBytes:_bitmapData
                   bytesPerRow:_width * 4];
}

- (void)drawInCommandBuffer:(id<MTLCommandBuffer>)cmd renderPassDescriptor:(MTLRenderPassDescriptor*)rpd {
    if (!_pipeline || !_hudTexture || !cmd || !rpd) return;
    MTLRenderPassDescriptor* desc = [MTLRenderPassDescriptor renderPassDescriptor];
    desc.colorAttachments[0].texture = rpd.colorAttachments[0].texture;
    desc.colorAttachments[0].loadAction = MTLLoadActionLoad;
    desc.colorAttachments[0].storeAction = MTLStoreActionStore;

    id<MTLRenderCommandEncoder> enc = [cmd renderCommandEncoderWithDescriptor:desc];
    if (!enc) return;
    [enc setRenderPipelineState:_pipeline];
    [enc setVertexBuffer:_quadBuffer offset:0 atIndex:0];
    [enc setFragmentTexture:_hudTexture atIndex:0];
    [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
    [enc endEncoding];
}

- (void)setPipeline:(id<MTLRenderPipelineState>)p {
    _pipeline = p;
}

@end
