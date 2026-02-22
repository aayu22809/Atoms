#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

@interface HUDRenderer : NSObject
- (instancetype)initWithDevice:(id<MTLDevice>)device;
- (void)updateWithN:(int)n L:(int)l M:(int)m particleCount:(int)count renderMode:(int)mode animating:(BOOL)anim;
- (void)drawInCommandBuffer:(id<MTLCommandBuffer>)cmd renderPassDescriptor:(MTLRenderPassDescriptor*)rpd;
- (void)setPipeline:(id<MTLRenderPipelineState>)pipeline;
@property (nonatomic, readonly) id<MTLRenderPipelineState> pipeline;
@end
