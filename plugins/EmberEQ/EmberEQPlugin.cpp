// Ember Tube EQ - passive program equalizer with tube make-up stage
// DSP: shelving/peaking network modelled after classic 1950s passive tube EQs.

#include "DistrhoPlugin.hpp"
#include <cmath>
#include <cstring>

START_NAMESPACE_DISTRHO

static const float kLowFreqs[]   = { 20.f, 30.f, 60.f, 100.f };
static const float kHighFreqs[]  = { 3000.f, 4000.f, 5000.f, 8000.f, 10000.f, 12000.f, 16000.f };
static const float kAttenFreqs[] = { 5000.f, 10000.f, 20000.f };

// ---------------------------------------------------------------------------

struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] = {0, 0}, z2[2] = {0, 0};

    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }

    inline double process(double x, int ch) {
        const double y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }

    void set(double nb0, double nb1, double nb2, double na0, double na1, double na2) {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }

    // RBJ cookbook
    void lowShelf(double fs, double f, double gainDb, double q) {
        f = std::fmin(f, fs * 0.45);
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w = 2.0 * M_PI * f / fs, cw = std::cos(w), sw = std::sin(w);
        const double al = sw / (2.0 * q), sa = 2.0 * std::sqrt(A) * al;
        set(A * ((A + 1) - (A - 1) * cw + sa), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - sa),
            (A + 1) + (A - 1) * cw + sa, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - sa);
    }
    void highShelf(double fs, double f, double gainDb, double q) {
        f = std::fmin(f, fs * 0.45);
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w = 2.0 * M_PI * f / fs, cw = std::cos(w), sw = std::sin(w);
        const double al = sw / (2.0 * q), sa = 2.0 * std::sqrt(A) * al;
        set(A * ((A + 1) + (A - 1) * cw + sa), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - sa),
            (A + 1) - (A - 1) * cw + sa, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - sa);
    }
    void peak(double fs, double f, double gainDb, double q) {
        f = std::fmin(f, fs * 0.45);
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w = 2.0 * M_PI * f / fs, cw = std::cos(w), sw = std::sin(w);
        const double al = sw / (2.0 * q);
        set(1 + al * A, -2 * cw, 1 - al * A, 1 + al / A, -2 * cw, 1 - al / A);
    }
    // first-order shelf: unity at HF, gainDb at DC, upper corner at f (gentle 6 dB/oct passive slope)
    void lowShelf1(double fs, double f, double gainDb) {
        const double G = std::pow(10.0, gainDb / 20.0);
        const double kp = std::tan(M_PI * std::fmin(f, fs * 0.45) / fs);
        const double kz = std::tan(M_PI * std::fmin(f * G, fs * 0.45) / fs);
        set(1 + kz, kz - 1, 0, 1 + kp, kp - 1, 0);
    }
    void highPass1(double fs, double f) {
        const double k = std::tan(M_PI * f / fs);
        set(1, -1, 0, 1 + k, k - 1, 0);
    }
};

// ---------------------------------------------------------------------------

class EmberEQPlugin : public Plugin
{
public:
    EmberEQPlugin()
        : Plugin(kParameterCount, 0, 0)
    {
        for (uint32_t i = 0; i < kParameterCount; ++i)
        {
            Parameter p;
            initParameter(i, p);
            fParams[i] = p.ranges.def;
        }
        sampleRateChanged(getSampleRate());
    }

protected:
    const char* getLabel() const override       { return "EmberTubeEQ"; }
    const char* getDescription() const override { return "Passive program equalizer with tube make-up stage."; }
    const char* getMaker() const override       { return "Ember Audio"; }
    const char* getLicense() const override     { return "ISC"; }
    uint32_t getVersion() const override        { return d_version(1, 0, 0); }

    void initParameter(uint32_t index, Parameter& p) override
    {
        p.hints = kParameterIsAutomatable;
        switch (index)
        {
        case kPower:
            p.hints |= kParameterIsBoolean | kParameterIsInteger;
            p.name = "Power"; p.symbol = "power";
            p.ranges.def = 1; p.ranges.min = 0; p.ranges.max = 1;
            p.designation = kParameterDesignationNull;
            break;
        case kLowFreq: {
            p.hints |= kParameterIsInteger;
            p.name = "Low Frequency"; p.symbol = "low_freq"; p.unit = "Hz";
            p.ranges.def = 2; p.ranges.min = 0; p.ranges.max = 3;
            static ParameterEnumerationValue v[4] = { {0, "20 Hz"}, {1, "30 Hz"}, {2, "60 Hz"}, {3, "100 Hz"} };
            p.enumValues.count = 4; p.enumValues.restrictedMode = true; p.enumValues.values = v;
            p.enumValues.deleteLater = false;
            break; }
        case kLowBoost:
            p.name = "Low Boost"; p.symbol = "low_boost";
            p.ranges.def = 0; p.ranges.min = 0; p.ranges.max = 10;
            break;
        case kLowAtten:
            p.name = "Low Atten"; p.symbol = "low_atten";
            p.ranges.def = 0; p.ranges.min = 0; p.ranges.max = 10;
            break;
        case kHighFreq: {
            p.hints |= kParameterIsInteger;
            p.name = "High Frequency"; p.symbol = "high_freq"; p.unit = "kHz";
            p.ranges.def = 4; p.ranges.min = 0; p.ranges.max = 6;
            static ParameterEnumerationValue v[7] = { {0, "3 kHz"}, {1, "4 kHz"}, {2, "5 kHz"}, {3, "8 kHz"},
                                                       {4, "10 kHz"}, {5, "12 kHz"}, {6, "16 kHz"} };
            p.enumValues.count = 7; p.enumValues.restrictedMode = true; p.enumValues.values = v;
            p.enumValues.deleteLater = false;
            break; }
        case kHighBoost:
            p.name = "High Boost"; p.symbol = "high_boost";
            p.ranges.def = 0; p.ranges.min = 0; p.ranges.max = 10;
            break;
        case kBandwidth:
            p.name = "Bandwidth"; p.symbol = "bandwidth";
            p.ranges.def = 5; p.ranges.min = 0; p.ranges.max = 10;
            break;
        case kAttenSel: {
            p.hints |= kParameterIsInteger;
            p.name = "Atten Select"; p.symbol = "atten_sel"; p.unit = "kHz";
            p.ranges.def = 1; p.ranges.min = 0; p.ranges.max = 2;
            static ParameterEnumerationValue v[3] = { {0, "5 kHz"}, {1, "10 kHz"}, {2, "20 kHz"} };
            p.enumValues.count = 3; p.enumValues.restrictedMode = true; p.enumValues.values = v;
            p.enumValues.deleteLater = false;
            break; }
        case kHighAtten:
            p.name = "High Atten"; p.symbol = "high_atten";
            p.ranges.def = 0; p.ranges.min = 0; p.ranges.max = 10;
            break;
        case kDrive:
            p.name = "Tube Drive"; p.symbol = "drive";
            p.ranges.def = 3; p.ranges.min = 0; p.ranges.max = 10;
            break;
        case kOutput:
            p.name = "Output"; p.symbol = "output"; p.unit = "dB";
            p.ranges.def = 0; p.ranges.min = -12; p.ranges.max = 12;
            break;
        case kMeter:
            p.hints = kParameterIsOutput;
            p.name = "Output Level"; p.symbol = "meter"; p.unit = "dB";
            p.ranges.def = -60; p.ranges.min = -60; p.ranges.max = 6;
            break;
        case kGlow:
            p.hints = kParameterIsOutput;
            p.name = "Tube Activity"; p.symbol = "tube_glow";
            p.ranges.def = 0; p.ranges.min = 0; p.ranges.max = 1;
            break;
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        if (index == kMeter)
            return fMeterDb;
        if (index == kGlow)
            return fGlow;
        return fParams[index];
    }

    void setParameterValue(uint32_t index, float value) override
    {
        if (index < kParameterCount)
            fParams[index] = value;
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fs = newSampleRate > 0 ? newSampleRate : 48000.0;
        dcBlock.highPass1(fs, 4.0);
        meterCoef = std::exp(-1.0 / (0.3 * fs));
        actCoef = std::exp(-1.0 / (0.08 * fs));
        rampStep = 1.0 / (0.015 * fs);
        firstBlock = true;
        updateFilters(true);
    }

    void activate() override
    {
        lowBoostF.reset(); lowAttenF.reset(); highBoostF.reset(); highAttenF.reset(); dcBlock.reset();
        meterState = 0; fMeterDb = -60.f;
        dryEnv = diffEnv = driveEnv = 0; glowState = 0; fGlow = 0.f;
        firstBlock = true;
        updateFilters(true);
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const float* inL = inputs[0];
        const float* inR = inputs[1];
        float* outL = outputs[0];
        float* outR = outputs[1];

        const double powerTarget = fParams[kPower] > 0.5f ? 1.0 : 0.0;
        if (firstBlock) { power = powerTarget; firstBlock = false; }

        uint32_t pos = 0;
        while (pos < frames)
        {
            const uint32_t n = std::min<uint32_t>(32, frames - pos);
            updateFilters(false);

            for (uint32_t i = 0; i < n; ++i)
            {
                const uint32_t k = pos + i;
                const double dryL = inL[k], dryR = inR[k];
                double x[2] = { dryL, dryR };

                // linear gain smoothing
                driveGain  += (driveTarget  - driveGain)  * 0.002;
                outputGain += (outputTarget - outputGain) * 0.002;
                if (power < powerTarget) power = std::min(powerTarget, power + rampStep);
                else if (power > powerTarget) power = std::max(powerTarget, power - rampStep);

                for (int c = 0; c < 2; ++c)
                {
                    double s = x[c];
                    // passive network insertion loss is compensated by the make-up amp, so stay at unity
                    s = lowBoostF.process(s, c);
                    s = lowAttenF.process(s, c);
                    s = highBoostF.process(s, c);
                    s = highAttenF.process(s, c);
                    s = tube(s, c);
                    s = dcBlock.process(s, c);
                    x[c] = s * outputGain;
                }

                const double wet = power, dry = 1.0 - power;
                const double yL = x[0] * wet + dryL * dry;
                const double yR = x[1] * wet + dryR * dry;
                outL[k] = (float)yL;
                outR[k] = (float)yR;

                const double sq = 0.5 * (yL * yL + yR * yR);
                meterState = sq + meterCoef * (meterState - sq);

                // tube activity: how much the processed signal differs from the input (EQ + saturation)
                // and how hard the tube stage is being driven
                const double dL = yL - dryL, dR = yR - dryR;
                const double dsq = 0.5 * (dL * dL + dR * dR);
                const double isq = 0.5 * (dryL * dryL + dryR * dryR);
                const double gsq = isq * driveGain * driveGain * power;
                dryEnv   = isq + actCoef * (dryEnv - isq);
                diffEnv  = dsq + actCoef * (diffEnv - dsq);
                driveEnv = gsq + actCoef * (driveEnv - gsq);
            }
            pos += n;
        }

        const double rms = std::sqrt(meterState);
        fMeterDb = rms > 1e-6 ? (float)std::fmax(-60.0, 20.0 * std::log10(rms) + 14.0) : -60.f;
        if (! std::isfinite(fMeterDb)) fMeterDb = -60.f;

        // map activity to 0..1 glow, with fast attack / slow release
        double target = 0.0;
        if (dryEnv > 1e-9)
        {
            const double eqAmt    = std::fmin(1.0, std::sqrt(diffEnv / dryEnv) * 1.6);   // -4 dB difference = full
            const double driveAmt = std::fmin(1.0, std::sqrt(driveEnv) / 0.9);          // tube pushed into its knee
            const double presence = std::fmin(1.0, std::sqrt(dryEnv) / 0.01);           // fade out on silence
            target = std::fmin(1.0, (0.55 * eqAmt + 0.65 * driveAmt) * presence) * power;
        }
        const double blockSec = frames / fs;
        const double tc = target > glowState ? 0.04 : 0.35;
        glowState += (target - glowState) * (1.0 - std::exp(-blockSec / tc));
        fGlow = std::isfinite(glowState) ? (float)glowState : 0.f;
    }

private:
    // Two-stage triode-like soft saturation (asymmetric -> even harmonics), level compensated.
    inline double tube(double x, int /*c*/)
    {
        const double g = driveGain;
        const double bias = 0.12;
        const double in = x * g;
        const double y = std::tanh(in + bias) - std::tanh(bias);
        const double slope = 1.0 - std::tanh(bias) * std::tanh(bias); // derivative at 0
        return y / (g * slope);
    }

    void updateFilters(bool force)
    {
        const int lf = clampIdx(fParams[kLowFreq], 3);
        const int hf = clampIdx(fParams[kHighFreq], 6);
        const int af = clampIdx(fParams[kAttenSel], 2);

        // smooth knob values to avoid zipper noise
        const double coef = force ? 1.0 : 0.15;
        for (int i = 0; i < 5; ++i)
        {
            static const int idx[5] = { kLowBoost, kLowAtten, kHighBoost, kBandwidth, kHighAtten };
            sm[i] += (fParams[idx[i]] - sm[i]) * coef;
        }

        const double lowBoostDb  = sm[0] * 1.35;            // up to +13.5 dB
        const double lowAttenDb  = -sm[1] * 1.75;           // up to -17.5 dB
        const double highBoostDb = sm[2] * 1.6;             // up to +16 dB
        const double bw          = sm[3] / 10.0;            // 0 = sharp, 1 = broad
        const double highAttenDb = -sm[4] * 1.6;            // up to -16 dB

        const double key[8] = { lowBoostDb, lowAttenDb, highBoostDb, bw, highAttenDb, (double)lf, (double)hf, (double)af };
        bool changed = force;
        for (int i = 0; i < 8; ++i)
            if (std::fabs(key[i] - lastKey[i]) > 1e-4) { changed = true; break; }

        if (changed)
        {
            const double f0 = kLowFreqs[lf];
            // Boost: resonant shelf (slight bump above the corner like the inductor network)
            lowBoostF.lowShelf(fs, f0 * 1.25, lowBoostDb, 0.9);
            // Atten: gentle first-order shelf sitting higher -> the famous "boost + cut" dip
            lowAttenF.lowShelf1(fs, f0 * 2.6, lowAttenDb);
            const double q = 2.6 * std::pow(0.4 / 2.6, bw);
            highBoostF.peak(fs, kHighFreqs[hf], highBoostDb, q);
            highAttenF.highShelf(fs, kAttenFreqs[af], highAttenDb, 0.55);
            std::memcpy(lastKey, key, sizeof(key));
        }

        // Drive 0..10 => 0.5..9 (linear gain into the saturator)
        driveTarget = 0.5 * std::pow(18.0, fParams[kDrive] / 10.0);
        outputTarget = std::pow(10.0, fParams[kOutput] / 20.0);
        if (force) { driveGain = driveTarget; outputGain = outputTarget; }
    }

    static int clampIdx(float v, int max)
    {
        int i = (int)std::lround(v);
        return i < 0 ? 0 : (i > max ? max : i);
    }

    float fParams[kParameterCount] = {};
    double fs = 48000.0;
    Biquad lowBoostF, lowAttenF, highBoostF, highAttenF, dcBlock;
    double sm[5] = {0, 0, 0, 5, 0};
    double lastKey[8] = {-999, -999, -999, -999, -999, -999, -999, -999};
    double driveGain = 1, driveTarget = 1, outputGain = 1, outputTarget = 1;
    double power = 1, rampStep = 0.001;
    bool firstBlock = true;
    double meterState = 0, meterCoef = 0.999;
    float fMeterDb = -60.f;
    double actCoef = 0.999, dryEnv = 0, diffEnv = 0, driveEnv = 0, glowState = 0;
    float fGlow = 0.f;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EmberEQPlugin)
};

Plugin* createPlugin()
{
    return new EmberEQPlugin();
}

END_NAMESPACE_DISTRHO
