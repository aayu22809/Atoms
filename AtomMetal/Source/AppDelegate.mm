#import "AppDelegate.h"
#import "MetalView.h"
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    NSRect frame = NSMakeRect(0, 0, 1280, 800);

    NSWindow* window = [[NSWindow alloc]
        initWithContentRect:frame
                  styleMask:NSWindowStyleMaskTitled |
                           NSWindowStyleMaskClosable |
                           NSWindowStyleMaskResizable
                    backing:NSBackingStoreBuffered
                      defer:NO];

    [window setTitle:@"Quantum Orbital Visualizer — Metal"];
    [window center];

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) {
        NSLog(@"Metal is not supported on this device");
        [NSApp terminate:nil];
        return;
    }

    MetalView* view = [[MetalView alloc] initWithFrame:frame device:device];
    [window setContentView:view];
    window.delegate = self;
    [window makeFirstResponder:view];
    [window makeKeyAndOrderFront:nil];
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
    NSWindow* window = notification.object;
    if (window.contentView) {
        [window makeFirstResponder:window.contentView];
    }
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)app {
    return YES;
}

@end
