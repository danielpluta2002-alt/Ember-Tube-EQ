#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "Ember Audio"
#define DISTRHO_PLUGIN_NAME    "Ember Tube EQ"
#define DISTRHO_PLUGIN_URI     "urn:emberaudio:tube-eq"
#define DISTRHO_PLUGIN_CLAP_ID "audio.ember.tube-eq"

#define DISTRHO_PLUGIN_BRAND_ID  Embr
#define DISTRHO_PLUGIN_UNIQUE_ID EmTq

#define DISTRHO_PLUGIN_HAS_UI        1
#define DISTRHO_PLUGIN_IS_RT_SAFE    1
#define DISTRHO_PLUGIN_NUM_INPUTS    2
#define DISTRHO_PLUGIN_NUM_OUTPUTS   2
#define DISTRHO_UI_USE_NANOVG        1
#define DISTRHO_UI_USER_RESIZABLE    1
#define DISTRHO_UI_DEFAULT_WIDTH     1000
#define DISTRHO_UI_DEFAULT_HEIGHT    400

#define DISTRHO_PLUGIN_VST3_CATEGORIES "Fx|EQ"
#define DISTRHO_PLUGIN_CLAP_FEATURES   "audio-effect", "equalizer", "stereo"
#define DISTRHO_PLUGIN_LV2_CATEGORY    "lv2:EQPlugin"

enum Parameters {
    kPower = 0,
    kLowFreq,
    kLowBoost,
    kLowAtten,
    kHighFreq,
    kHighBoost,
    kBandwidth,
    kAttenSel,
    kHighAtten,
    kDrive,
    kOutput,
    kMeter,
    kGlow,
    kParameterCount
};

#endif
