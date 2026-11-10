// Minimal macOS CLAP host: opens the plugin GUI in a Cocoa window and takes a screenshot.
// usage: mac_gui_host <EmberEQ.clap bundle> <out.png>
#define GL_SILENCE_DEPRECATION
#import <Cocoa/Cocoa.h>
#include <OpenGL/gl.h>
#include "clap/clap.h"
#include <dlfcn.h>

// CGWindowListCreateImage is marked unavailable in newer SDKs but still exists at runtime
typedef CGImageRef (*CreateImageFn)(CGRect, uint32_t, uint32_t, uint32_t);
static void captureWindow(NSWindow* win, const char* path)
{
    void* cg = dlopen("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics", RTLD_LAZY);
    auto fn = cg ? (CreateImageFn)dlsym(cg, "CGWindowListCreateImage") : nullptr;
    if (!fn) { printf("capture: no CGWindowListCreateImage\n"); return; }
    // kCGWindowListOptionIncludingWindow = 8, kCGWindowImageBoundsIgnoreFraming = 1
    CGImageRef img = fn(CGRectNull, 8, (uint32_t)[win windowNumber], 1);
    if (!img) { printf("capture: null image\n"); return; }
    NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithCGImage:img];
    NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    [png writeToFile:[NSString stringWithUTF8String:path] atomically:YES];
    printf("capture: %zux%zu -> %s\n", CGImageGetWidth(img), CGImageGetHeight(img), path);
    CGImageRelease(img);
}
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static void hg_hints(const clap_host*) {}
static bool hg_resize(const clap_host*, uint32_t w, uint32_t h) { printf("host: request_resize %ux%u\n", w, h); return true; }
static bool hg_show(const clap_host*) { return true; }
static void hg_closed(const clap_host*, bool) {}
static const clap_host_gui kHostGui = { hg_hints, hg_resize, hg_show, hg_show, hg_closed };
static bool ht_reg(const clap_host*, uint32_t, clap_id* id) { *id = 0; return true; }
static bool ht_unreg(const clap_host*, clap_id) { return true; }
static const clap_host_timer_support kHostTimer = { ht_reg, ht_unreg };
static const void* host_get_ext(const clap_host*, const char* id) {
    if (!strcmp(id, CLAP_EXT_GUI)) return &kHostGui;
    if (!strcmp(id, CLAP_EXT_TIMER_SUPPORT)) return &kHostTimer;
    return nullptr; }
static void host_noop(const clap_host*) {}
static uint32_t in_size(const clap_input_events*) { return 0; }
static const clap_event_header* in_get(const clap_input_events*, uint32_t) { return nullptr; }
static bool out_push(const clap_output_events*, const clap_event_header*) { return true; }

static void dumpViews(NSView* v, int depth)
{
    printf("%*s%s frame=%.0fx%.0f wantsLayer=%d hidden=%d\n", depth * 2, "",
           [[v className] UTF8String], v.frame.size.width, v.frame.size.height, (int)v.wantsLayer, (int)v.hidden);
    for (NSView* c in v.subviews) dumpViews(c, depth + 1);
}

int main(int argc, char** argv)
{
    @autoreleasepool {
    if (argc < 3) return 1;
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    if (!strcmp(argv[1], "--control")) {
        // control: our own legacy OpenGL view cleared to red, to check the machine can show GL at all
        NSWindow* win = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 400, 200)
                                                    styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
        NSOpenGLPixelFormatAttribute attrs[] = { NSOpenGLPFADoubleBuffer, 0 };
        NSOpenGLPixelFormat* pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
        printf("pixel format: %p\n", pf);
        NSOpenGLView* v = [[NSOpenGLView alloc] initWithFrame:NSMakeRect(0, 0, 400, 200) pixelFormat:pf];
        [win setContentView:v];
        [win makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        NSDate* until = [NSDate dateWithTimeIntervalSinceNow:2.0];
        while ([until timeIntervalSinceNow] > 0) {
            [[v openGLContext] makeCurrentContext];
            glClearColor(1, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
            [[v openGLContext] flushBuffer];
            NSEvent* ev;
            while ((ev = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate dateWithTimeIntervalSinceNow:0.02]
                                               inMode:NSDefaultRunLoopMode dequeue:YES])) [NSApp sendEvent:ev];
        }
        printf("GL renderer: %s / %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
        captureWindow(win, argv[2]); fflush(stdout); _exit(0);
    }

    NSBundle* bundle = [NSBundle bundleWithPath:[NSString stringWithUTF8String:argv[1]]];
    std::string bin = [[bundle executablePath] UTF8String];
    void* lib = dlopen(bin.c_str(), RTLD_NOW);
    if (!lib) { printf("dlopen: %s\n", dlerror()); return 1; }
    auto entry = (const clap_plugin_entry*)dlsym(lib, "clap_entry");
    entry->init(argv[1]);
    auto fac = (const clap_plugin_factory*)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    auto desc = fac->get_plugin_descriptor(fac, 0);
    printf("plugin: %s\n", desc->name);

    clap_host host = { CLAP_VERSION, nullptr, "test", "t", "", "1", host_get_ext, host_noop, host_noop, host_noop };
    auto pl = fac->create_plugin(fac, &host, desc->id);
    pl->init(pl);
    pl->activate(pl, 48000, 1, 1024);
    pl->start_processing(pl);

    auto gui = (const clap_plugin_gui*)pl->get_extension(pl, CLAP_EXT_GUI);
    bool ok = gui->create(pl, CLAP_WINDOW_API_COCOA, false);
    printf("gui create: %d\n", ok);
    double sc = [NSScreen mainScreen].backingScaleFactor;
    printf("backingScaleFactor: %.1f\n", sc);
    gui->set_scale(pl, sc);
    uint32_t w = 0, h = 0; gui->get_size(pl, &w, &h);
    printf("gui size: %ux%u\n", w, h);

    NSWindow* win = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, w, h)
                                                styleMask:NSWindowStyleMaskTitled
                                                  backing:NSBackingStoreBuffered defer:NO];
    [win setTitle:@"Ember GUI test"];
    [win makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    clap_window cw = {}; cw.api = CLAP_WINDOW_API_COCOA; cw.cocoa = (__bridge void*)[win contentView];
    printf("set_parent: %d\n", gui->set_parent(pl, &cw));
    printf("show: %d\n", gui->show(pl));

    auto timer = (const clap_plugin_timer_support*)pl->get_extension(pl, CLAP_EXT_TIMER_SUPPORT);
    std::vector<float> L(512), R(512), oL(512), oR(512);
    float* ins[2] = { L.data(), R.data() }; float* outs[2] = { oL.data(), oR.data() };
    clap_input_events inev = { nullptr, in_size, in_get };
    clap_output_events outev = { nullptr, out_push };
    double ph = 0;

    NSDate* until = [NSDate dateWithTimeIntervalSinceNow:4.0];
    while ([until timeIntervalSinceNow] > 0) {
        for (int k = 0; k < 512; ++k) { L[k] = R[k] = 0.4f * std::sin(ph); ph += 2 * M_PI * 440 / 48000; }
        clap_audio_buffer ib = {}; ib.data32 = ins; ib.channel_count = 2;
        clap_audio_buffer ob = {}; ob.data32 = outs; ob.channel_count = 2;
        clap_process pr = {}; pr.steady_time = -1; pr.frames_count = 512;
        pr.audio_inputs = &ib; pr.audio_inputs_count = 1; pr.audio_outputs = &ob; pr.audio_outputs_count = 1;
        pr.in_events = &inev; pr.out_events = &outev;
        pl->process(pl, &pr);
        if (timer) timer->on_timer(pl, 0);
        NSEvent* ev;
        while ((ev = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]
                                           inMode:NSDefaultRunLoopMode dequeue:YES]))
            [NSApp sendEvent:ev];
    }
    dumpViews([win contentView], 0);
    printf("windowNumber: %ld\n", (long)[win windowNumber]);
    fflush(stdout);

    captureWindow(win, argv[2]);
    fflush(stdout);
    _exit(0);
    }
}
