#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import "ShaderTypes.h"
#import "ProbabilityCurrent.h"
#import "PointCharge.h"
#include <vector>

@interface Renderer : NSObject
- (instancetype)initWithDevice:(id<MTLDevice>)device;
- (void)updateHUDWithN:(int)n L:(int)l M:(int)m count:(int)count mode:(int)mode animating:(BOOL)anim;
- (void)drawInMTKView:(MTKView*)view
           particles:(const std::vector<Particle>&)particles
             charges:(const std::vector<PointChargeData>&)charges
          uniforms:(FrameUniforms)uniforms
        cutaway:(BOOL)cutaway
     renderMode:(int)renderMode;
@end
