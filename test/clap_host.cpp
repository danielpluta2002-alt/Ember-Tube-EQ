// Minimal CLAP test host: frequency response + GUI screenshot helper.
// usage: clap_host <plugin.clap> response "<id=val,...>"   -> prints gain at test freqs
//        clap_host <plugin.clap> gui "<id=val,...>" seconds  -> opens embedded X11 GUI
#include "clap/clap.h"
#include <X11/Xlib.h>
#include <dlfcn.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

static void hg_hints(const clap_host*) {}
static bool hg_resize(const clap_host*, uint32_t, uint32_t) { return true; }
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

struct Ev { clap_event_param_value ev; };
static std::vector<Ev> gEvents;
static uint32_t in_size(const clap_input_events*) { return gEvents.size(); }
static const clap_event_header* in_get(const clap_input_events*, uint32_t i) { return &gEvents[i].ev.header; }
static bool out_push(const clap_output_events*, const clap_event_header*) { return true; }

int main(int argc, char** argv)
{
    if (argc < 3) return 1;
    void* lib = dlopen(argv[1], RTLD_NOW);
    if (!lib) { printf("dlopen: %s\n", dlerror()); return 1; }
    auto entry = (const clap_plugin_entry*)dlsym(lib, "clap_entry");
    entry->init(argv[1]);
    auto fac = (const clap_plugin_factory*)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    auto desc = fac->get_plugin_descriptor(fac, 0);
    printf("plugin: %s (%s) v%s\n", desc->name, desc->id, desc->version);

    clap_host host = { CLAP_VERSION, nullptr, "test", "t", "", "1", host_get_ext, host_noop, host_noop, host_noop };
    auto pl = fac->create_plugin(fac, &host, desc->id);
    pl->init(pl);
    auto params = (const clap_plugin_params*)pl->get_extension(pl, CLAP_EXT_PARAMS);
    uint32_t np = params->count(pl);
    std::vector<clap_id> ids;
    for (uint32_t i = 0; i < np; ++i) {
        clap_param_info info; params->get_info(pl, i, &info); ids.push_back(info.id);
        if (std::string(argv[2]) == "params")
            printf("  [%u] id=%u %s  %.1f..%.1f def %.1f\n", i, info.id, info.name, info.min_value, info.max_value, info.default_value);
    }

    // parse param assignments "idx=val,idx=val"
    std::vector<std::pair<int,double>> sets;
    if (argc > 3) {
        std::string s = argv[3]; size_t p = 0;
        while (p < s.size()) {
            size_t c = s.find(',', p); if (c == std::string::npos) c = s.size();
            std::string kv = s.substr(p, c - p); size_t e = kv.find('=');
            if (e != std::string::npos) sets.push_back({atoi(kv.c_str()), atof(kv.c_str() + e + 1)});
            p = c + 1;
        }
    }
    const double fs = 48000;
    pl->activate(pl, fs, 1, 4096);
    pl->start_processing(pl);

    clap_input_events inev = { nullptr, in_size, in_get };
    clap_output_events outev = { nullptr, out_push };

    auto run = [&](float* L, float* R, float* oL, float* oR, uint32_t n) {
        float* ins[2] = { L, R }; float* outs[2] = { oL, oR };
        clap_audio_buffer ib = {}; ib.data32 = ins; ib.channel_count = 2;
        clap_audio_buffer ob = {}; ob.data32 = outs; ob.channel_count = 2;
        clap_process pr = {}; pr.steady_time = -1; pr.frames_count = n;
        pr.audio_inputs = &ib; pr.audio_inputs_count = 1; pr.audio_outputs = &ob; pr.audio_outputs_count = 1;
        pr.in_events = &inev; pr.out_events = &outev;
        pl->process(pl, &pr);
        gEvents.clear();
    };

    for (auto& s : sets) {
        Ev e = {}; e.ev.header.size = sizeof(e.ev); e.ev.header.type = CLAP_EVENT_PARAM_VALUE;
        e.ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID; e.ev.param_id = ids[s.first]; e.ev.value = s.second;
        e.ev.note_id = -1; e.ev.port_index = -1; e.ev.channel = -1; e.ev.key = -1;
        gEvents.push_back(e);
    }

    const uint32_t N = 4096;
    std::vector<float> L(N), R(N), oL(N), oR(N);

    if (std::string(argv[2]) == "response") {
        // settle parameters
        for (int i = 0; i < 20; ++i) run(L.data(), R.data(), oL.data(), oR.data(), N);
        const double freqs[] = {20,30,40,60,80,100,150,200,300,500,1000,2000,3000,4000,5000,8000,10000,12000,16000,20000};
        for (double f : freqs) {
            double ph = 0, sumIn = 0, sumOut = 0;
            for (int blk = 0; blk < 24; ++blk) {
                for (uint32_t i = 0; i < N; ++i) { L[i] = R[i] = 0.05f * std::sin(ph); ph += 2*M_PI*f/fs; }
                run(L.data(), R.data(), oL.data(), oR.data(), N);
                if (blk >= 12) for (uint32_t i = 0; i < N; ++i) { sumIn += L[i]*L[i]; sumOut += oL[i]*oL[i]; }
            }
            printf("%7.0f Hz : %+6.2f dB\n", f, 10*std::log10(sumOut/sumIn));
        }
    } else if (std::string(argv[2]) == "gui") {
        for (int i = 0; i < 20; ++i) {
            for (uint32_t k = 0; k < N; ++k) L[k] = R[k] = 0.4f * std::sin(2*M_PI*440*k/fs);
            run(L.data(), R.data(), oL.data(), oR.data(), N);
        }
        auto gui = (const clap_plugin_gui*)pl->get_extension(pl, CLAP_EXT_GUI);
        Display* d = XOpenDisplay(nullptr);
        gui->create(pl, CLAP_WINDOW_API_X11, false);
        uint32_t w, h; gui->get_size(pl, &w, &h);
        printf("gui size %ux%u\n", w, h);
        Window win = XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0, w, h, 0, 0, 0);
        XMapWindow(d, win); XFlush(d);
        clap_window cw = {}; cw.api = CLAP_WINDOW_API_X11; cw.x11 = win;
        gui->set_parent(pl, &cw);
        gui->show(pl);
        auto timer = (const clap_plugin_timer_support*)pl->get_extension(pl, CLAP_EXT_TIMER_SUPPORT);
        int secs = argc > 4 ? atoi(argv[4]) : 3;
        for (int i = 0; i < secs * 30; ++i) {
            for (uint32_t k = 0; k < 1600; ++k) L[k] = R[k] = 0.4f * std::sin(2*M_PI*440*k/fs);
            run(L.data(), R.data(), oL.data(), oR.data(), 1600);
            if (timer) timer->on_timer(pl, 0);
            while (XPending(d)) { XEvent ev; XNextEvent(d, &ev); }
            usleep(33000);
        }
        printf("done\n");
        fflush(stdout);
        _exit(0);
    }
    pl->stop_processing(pl);
    pl->deactivate(pl);
    pl->destroy(pl);
    return 0;
}
