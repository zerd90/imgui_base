// Native Cocoa window + Metal renderer.
// Mirrors the application loop in imgui_main_win32_dx11.cpp / imgui_main_glfw_opengl3.cpp:
// saved window rect, size limits, file drop, vsync, and ImGuiApplication frame callbacks.

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <string>
#include <vector>

#include "ImGuiApplication.h"
#include "ImGuiCommonTools.h"
#include "imgui.h"
#include "imgui_impl_metal.h"
#include "imgui_impl_osx.h"

using namespace ImGui;

static MTKView            *g_view      = nil;
static id<MTLCommandQueue> g_queue     = nil;
static bool                g_exit      = false;
static bool                g_rendering = false;
static bool                g_ready     = false;

static const NSWindowStyleMask kWindowStyle =
    NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;

static NSRect TopLeftContentRectToFrame(int x, int y, int w, int h)
{
    NSScreen *screen      = NSScreen.screens.firstObject;
    NSRect    screenFrame = screen != nil ? screen.frame : NSMakeRect(0, 0, (CGFloat)w, (CGFloat)h);
    NSRect    content     = NSMakeRect(screenFrame.origin.x + x, NSMaxY(screenFrame) - y - h, w, h);
    return [NSWindow frameRectForContentRect:content styleMask:kWindowStyle];
}

static void SyncWindowRect(NSWindow *window)
{
    if (window == nil || window.miniaturized)
        return;

    NSScreen *screen      = NSScreen.screens.firstObject;
    NSRect    screenFrame = screen != nil ? screen.frame : NSMakeRect(0, 0, window.frame.size.width, window.frame.size.height);
    NSRect    content     = [window contentRectForFrameRect:window.frame];
    int       x           = (int)(content.origin.x - screenFrame.origin.x);
    int       y           = (int)(NSMaxY(screenFrame) - NSMaxY(content));
    int       w           = (int)content.size.width;
    int       h           = (int)content.size.height;
    gUserApp->windowRectChange({x, y, w, h});
}

@interface AppMTKView : MTKView <NSDraggingDestination>
@end

struct ViewPixelSize
{
    CGFloat pointsW;
    CGFloat pointsH;
    CGFloat scale;
    CGSize  pixels;
};

static CGFloat WindowScale(NSWindow *window)
{
    CGFloat scale = window.backingScaleFactor;
    if (scale <= 0)
        scale = window.screen.backingScaleFactor;
    return scale > 0 ? scale : 1;
}

// Window frame is already the post-resize size inside windowDidResize. The view bounds and
// MTKView's cached drawable can still be the previous size, and AppKit will scale that old
// texture to the new view until we replace it.
static ViewPixelSize PixelSizeForWindow(NSWindow *window)
{
    const NSRect  content = [window contentRectForFrameRect:window.frame];
    ViewPixelSize size;
    size.pointsW = std::max(content.size.width, (CGFloat)1);
    size.pointsH = std::max(content.size.height, (CGFloat)1);
    size.scale   = WindowScale(window);
    size.pixels  = CGSizeMake(size.pointsW * size.scale, size.pointsH * size.scale);
    return size;
}

static void ApplyDrawableSize(MTKView *view, CGSize points, CGFloat scale)
{
    CAMetalLayer *layer = (CAMetalLayer *)view.layer;
    layer.contentsScale = scale;
    layer.drawableSize  = CGSizeMake(std::max(points.width, (CGFloat)1) * scale, std::max(points.height, (CGFloat)1) * scale);
    view.drawableSize   = layer.drawableSize;
}

@implementation AppMTKView

- (instancetype)initWithFrame:(NSRect)frameRect device:(id<MTLDevice>)device
{
    self = [super initWithFrame:frameRect device:device];
    if (self != nil)
    {
        self.autoResizeDrawable        = NO;
        self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
        self.layerContentsPlacement    = NSViewLayerContentsPlacementTopLeft;
        [self registerForDraggedTypes:@[ NSPasteboardTypeFileURL ]];
    }
    return self;
}

- (void)setFrameSize:(NSSize)newSize
{
    [super setFrameSize:newSize];
    ApplyDrawableSize(self, newSize, WindowScale(self.window));
}

- (void)viewDidChangeBackingProperties
{
    [super viewDidChangeBackingProperties];
    ApplyDrawableSize(self, self.bounds.size, WindowScale(self.window));
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender
{
    return NSDragOperationCopy;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
    std::vector<std::string> files;
    NSArray<NSURL *>        *urls = [sender.draggingPasteboard readObjectsForClasses:@[ NSURL.class ]
                                                                      options:@{NSPasteboardURLReadingFileURLsOnlyKey : @YES}];
    for (NSURL *url in urls)
    {
        if (url.isFileURL && url.path != nil)
            files.emplace_back(url.path.UTF8String);
    }
    if (!files.empty())
        gUserApp->dropFile(files);
    return !files.empty();
}

@end

static NSString *ApplicationMenuName()
{
    NSDictionary        *bundleInfo = [[NSBundle mainBundle] infoDictionary];
    NSArray<NSString *> *nameKeys   = @[ @"CFBundleDisplayName", @"CFBundleName", @"CFBundleExecutable" ];
    for (NSString *key in nameKeys)
    {
        id name = bundleInfo[key];
        if ([name isKindOfClass:[NSString class]] && [name length] > 0)
            return name;
    }
    NSString *processName = [[NSProcessInfo processInfo] processName];
    return processName.length > 0 ? processName : @"Application";
}

static void InstallApplicationMenu()
{
    NSString *appName = ApplicationMenuName();
    NSMenu   *menubar = [[NSMenu alloc] init];
    NSApp.mainMenu    = menubar;

    NSMenuItem *appMenuItem = [menubar addItemWithTitle:@"" action:NULL keyEquivalent:@""];
    NSMenu     *appMenu     = [[NSMenu alloc] initWithTitle:appName];
    appMenuItem.submenu     = appMenu;

    [appMenu addItemWithTitle:[NSString stringWithFormat:@"About %@", appName]
                       action:@selector(orderFrontStandardAboutPanel:)
                keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    NSMenu *servicesMenu = [[NSMenu alloc] init];
    NSApp.servicesMenu   = servicesMenu;
    [[appMenu addItemWithTitle:@"Services" action:NULL keyEquivalent:@""] setSubmenu:servicesMenu];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:[NSString stringWithFormat:@"Hide %@", appName] action:@selector(hide:) keyEquivalent:@"h"];
    NSMenuItem *hideOthers               = [appMenu addItemWithTitle:@"Hide Others"
                                                action:@selector(hideOtherApplications:)
                                         keyEquivalent:@"h"];
    hideOthers.keyEquivalentModifierMask = NSEventModifierFlagOption | NSEventModifierFlagCommand;
    [appMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:[NSString stringWithFormat:@"Quit %@", appName] action:@selector(terminate:) keyEquivalent:@"q"];

    NSMenuItem *windowMenuItem = [menubar addItemWithTitle:@"" action:NULL keyEquivalent:@""];
    NSMenu     *windowMenu     = [[NSMenu alloc] initWithTitle:@"Window"];
    NSApp.windowsMenu          = windowMenu;
    windowMenuItem.submenu     = windowMenu;
    [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
    [windowMenu addItem:[NSMenuItem separatorItem]];
    [windowMenu addItemWithTitle:@"Bring All to Front" action:@selector(arrangeInFront:) keyEquivalent:@""];
    [windowMenu addItem:[NSMenuItem separatorItem]];
    NSMenuItem *fullScreen               = [windowMenu addItemWithTitle:@"Enter Full Screen"
                                                   action:@selector(toggleFullScreen:)
                                            keyEquivalent:@"f"];
    fullScreen.keyEquivalentModifierMask = NSEventModifierFlagControl | NSEventModifierFlagCommand;

    SEL setAppleMenuSelector = NSSelectorFromString(@"setAppleMenu:");
    if ([NSApp respondsToSelector:setAppleMenuSelector])
    {
        IMP implementation                = [NSApp methodForSelector:setAppleMenuSelector];
        void (*setAppleMenu)(id, SEL, id) = (void (*)(id, SEL, id))implementation;
        setAppleMenu(NSApp, setAppleMenuSelector, appMenu);
    }
}

static bool doGUIRender();

@interface                             AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property(nonatomic, strong) NSWindow *window;
@end

@implementation AppDelegate

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    IM_UNUSED(sender);
    return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    IM_UNUSED(sender);
    g_exit = true;
    return NSTerminateCancel;
}

- (BOOL)windowShouldClose:(NSWindow *)sender
{
    IM_UNUSED(sender);
    g_exit = true;
    return NO;
}

- (void)windowDidMove:(NSNotification *)notification
{
    SyncWindowRect(notification.object);
    if (g_ready && !g_rendering && doGUIRender())
        g_exit = true;
}

- (void)windowDidResize:(NSNotification *)notification
{
    SyncWindowRect(notification.object);
    if (g_ready && !g_rendering && doGUIRender())
        g_exit = true;
}

@end

static bool doGUIRender()
{
    static const ImVec4 clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    if (g_view == nil || g_view.window == nil)
        return g_exit;

    if (g_view.window.miniaturized)
    {
        [NSThread sleepForTimeInterval:0.01];
        return false;
    }

    const ViewPixelSize size      = PixelSizeForWindow(g_view.window);
    const NSRect        viewFrame = NSMakeRect(0, 0, size.pointsW, size.pointsH);
    if (!NSEqualRects(g_view.frame, viewFrame))
        g_view.frame = viewFrame;

    CAMetalLayer *layer      = (CAMetalLayer *)g_view.layer;
    layer.contentsScale      = size.scale;
    layer.drawableSize       = size.pixels;
    layer.displaySyncEnabled = gUserApp->VSyncEnabled();
    g_view.drawableSize      = size.pixels;

    id<CAMetalDrawable> drawable = [layer nextDrawable];
    if (drawable == nil)
    {
        [NSThread sleepForTimeInterval:0.01];
        return false;
    }

    MTLRenderPassDescriptor *renderPassDescriptor        = [MTLRenderPassDescriptor renderPassDescriptor];
    renderPassDescriptor.colorAttachments[0].texture     = drawable.texture;
    renderPassDescriptor.colorAttachments[0].loadAction  = MTLLoadActionClear;
    renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    renderPassDescriptor.colorAttachments[0].clearColor =
        MTLClearColorMake(clearColor.x * clearColor.w, clearColor.y * clearColor.w, clearColor.z * clearColor.w, clearColor.w);

    ImGui_ImplMetal_NewFrame(renderPassDescriptor);
    ImGui_ImplOSX_NewFrame(g_view);

    ImGuiIO &io                = ImGui::GetIO();
    io.DisplaySize             = ImVec2((float)size.pointsW, (float)size.pointsH);
    io.DisplayFramebufferScale = ImVec2((float)size.scale, (float)size.scale);

    gUserApp->newFramePreAction();
    ImGui::NewFrame();
    gUserApp->show();
    const bool quit = gUserApp->justClosed();
    ImGui::Render();
    gUserApp->endFramePostAction();

    id<MTLCommandBuffer>        commandBuffer = [g_queue commandBuffer];
    id<MTLRenderCommandEncoder> encoder       = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, encoder);
    [encoder endEncoding];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
    return quit;
}

int main(int argc, char **argv)
{
    @autoreleasepool
    {
        IM_ASSERT(gUserApp != nullptr);

        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        gUserApp->preset();

        ImGuiIO &io    = ImGui::GetIO();
        io.IniFilename = gUserApp->getConfigPath();
        ImGui::LoadIniSettingsFromDisk(io.IniFilename);
        gUserApp->initSettingsWindow();
        gUserApp->loadResources();
        startFontPixPreload();

        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device == nil)
        {
            fprintf(stderr, "Metal is not supported on this Mac.\n");
            return 1;
        }
        g_queue = [device newCommandQueue];

        const auto initial = gUserApp->getWindowInitialRect();
        NSRect     frame   = TopLeftContentRectToFrame(initial.x, initial.y, initial.w, initial.h);

        AppDelegate *delegate = [[AppDelegate alloc] init];
        [NSApp setDelegate:delegate];
        delegate.window                    = [[NSWindow alloc] initWithContentRect:frame
                                                      styleMask:kWindowStyle
                                                        backing:NSBackingStoreBuffered
                                                          defer:NO];
        delegate.window.delegate           = delegate;
        delegate.window.title              = [NSString stringWithUTF8String:gUserApp->getAppName().c_str()];
        delegate.window.releasedWhenClosed = NO;

        ImVec2 minSize, maxSize;
        gUserApp->getWindowSizeLimit(minSize, maxSize);
        if (minSize.x > 0 && minSize.y > 0)
            delegate.window.contentMinSize = NSMakeSize(minSize.x, minSize.y);
        if (maxSize.x > 0 && maxSize.y > 0)
            delegate.window.contentMaxSize = NSMakeSize(maxSize.x, maxSize.y);

        g_view                       = [[AppMTKView alloc] initWithFrame:delegate.window.contentView.bounds device:device];
        g_view.autoresizingMask      = NSViewWidthSizable | NSViewHeightSizable;
        g_view.paused                = YES;
        g_view.enableSetNeedsDisplay = NO;
        g_view.framebufferOnly       = YES;
        g_view.colorPixelFormat      = MTLPixelFormatBGRA8Unorm;
        delegate.window.contentView  = g_view;

        gUserApp->setWindowHandle((__bridge void *)delegate.window);

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.ConfigViewportsNoTaskBarIcon = false;

        ImGuiStyle &style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding              = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        [delegate.window makeKeyAndOrderFront:nil];
        InstallApplicationMenu();
        [NSApp activateIgnoringOtherApps:YES];
        [NSApp finishLaunching];

        if (!ImGui_ImplMetal_Init(device))
            return 1;
        if (!ImGui_ImplOSX_Init(g_view))
            return 1;

        std::vector<std::string> args;
        args.reserve((size_t)argc);
        for (int i = 0; i < argc; i++)
            args.emplace_back(argv[i]);
        gUserApp->transferCmdArgs(args);

        g_ready = true;
        while (!g_exit)
        {
            @autoreleasepool
            {
                while (NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                           untilDate:[NSDate distantPast]
                                                              inMode:NSDefaultRunLoopMode
                                                             dequeue:YES])
                {
                    [NSApp sendEvent:event];
                }
                if (g_exit)
                    break;

                g_rendering     = true;
                const bool quit = doGUIRender();
                g_rendering     = false;
                if (quit)
                    break;
            }
        }

        g_ready = false;
        gUserApp->exit();

        ImGui_ImplMetal_Shutdown();
        ImGui_ImplOSX_Shutdown();
        ImGui::DestroyContext();

        [delegate.window orderOut:nil];
        g_view  = nil;
        g_queue = nil;
    }
    return 0;
}
