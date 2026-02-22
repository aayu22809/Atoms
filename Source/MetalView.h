#import <MetalKit/MetalKit.h>

@interface MetalView : MTKView <MTKViewDelegate>
- (instancetype)initWithFrame:(NSRect)frame device:(id<MTLDevice>)device;
@end
