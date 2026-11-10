// Ember Tube EQ - vintage rack-panel interface (NanoVG)

#include "DistrhoUI.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

START_NAMESPACE_DISTRHO

static constexpr float kW = 1000.f;
static constexpr float kH = 400.f;
static constexpr float kPi = 3.14159265358979f;

struct Knob {
    uint32_t param;
    float cx, cy, r;
    float min, max, def;
    int steps;               // 0 = continuous, otherwise number of detents
    const char* title;
    const char* sub;         // small caption under the title
    const char* const* labels; // detent labels (for selectors)
    bool chicken;            // chicken-head pointer (selectors) vs round skirt knob
};

static const char* const kLowLabels[]   = { "20", "30", "60", "100" };
static const char* const kHighLabels[]  = { "3", "4", "5", "8", "10", "12", "16" };
static const char* const kAttLabels[]   = { "5", "10", "20" };

class EmberEQUI : public UI
{
public:
    EmberEQUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT, true)
    {
        loadSharedResources();

        const Knob knobs[] = {
            { kLowFreq,   150, 182, 30, 0, 3,  2, 4, "LOW FREQUENCY", "CPS",       kLowLabels,  true  },
            { kLowBoost,  290, 182, 34, 0, 10, 0, 0, "BOOST",         nullptr,     nullptr,     false },
            { kLowAtten,  430, 182, 34, 0, 10, 0, 0, "ATTEN",         nullptr,     nullptr,     false },
            { kHighFreq,  570, 182, 30, 0, 6,  4, 7, "HIGH FREQUENCY","KCS",       kHighLabels, true  },
            { kBandwidth, 150, 328, 30, 0, 10, 5, 0, "BANDWIDTH",     nullptr,     nullptr,     false },
            { kHighBoost, 290, 328, 34, 0, 10, 0, 0, "BOOST",         nullptr,     nullptr,     false },
            { kHighAtten, 430, 328, 34, 0, 10, 0, 0, "ATTEN",         nullptr,     nullptr,     false },
            { kAttenSel,  570, 328, 30, 0, 2,  1, 3, "ATTEN SEL",     "KCS",       kAttLabels,  true  },
            { kDrive,     730, 330, 28, 0, 10, 3, 0, "TUBE DRIVE",    nullptr,     nullptr,     false },
            { kOutput,    890, 330, 28, -12, 12, 0, 0, "OUTPUT",      "DB",        nullptr,     false },
        };
        static_assert(sizeof(knobs) / sizeof(knobs[0]) == kNumKnobs, "knob count");
        std::memcpy(fKnobs, knobs, sizeof(knobs));

        for (uint32_t i = 0; i < kParameterCount; ++i) fValues[i] = 0.f;
        fValues[kPower] = 1.f; fValues[kLowFreq] = 2; fValues[kHighFreq] = 4; fValues[kAttenSel] = 1;
        fValues[kBandwidth] = 5; fValues[kDrive] = 3; fValues[kMeter] = -60.f; fValues[kGlow] = 0.f;

        // pre-generate brushed metal streaks (deterministic)
        uint32_t seed = 1234567u;
        for (int i = 0; i < kStreaks; ++i)
        {
            seed = seed * 1664525u + 1013904223u; fStreakY[i] = (seed >> 8) % 4000 / 10.f;
            seed = seed * 1664525u + 1013904223u; fStreakA[i] = ((seed >> 8) % 100) / 100.f;
        }

        setGeometryConstraints(DISTRHO_UI_DEFAULT_WIDTH / 2, DISTRHO_UI_DEFAULT_HEIGHT / 2, true);
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if (index >= kParameterCount) return;
        if (index == kMeter)
        {
            if (std::fabs(value - fValues[kMeter]) < 0.05f) return;
        }
        if (index == kGlow)
        {
            if (std::fabs(value - fValues[kGlow]) < 0.004f) return;
        }
        fValues[index] = value;
        repaint();
    }

    // ------------------------------------------------------------------
    // drawing

    void onNanoDisplay() override
    {
        const float s = getWidth() / kW;
        save();
        scale(s, s);

        drawPanel();
        drawSections();
        for (int i = 0; i < kNumKnobs; ++i)
            drawKnob(fKnobs[i]);
        drawMeter();
        drawPower();
        drawTube();

        restore();
    }

    void drawPanel()
    {
        // rack body
        beginPath();
        rect(0, 0, kW, kH);
        fillPaint(linearGradient(0, 0, 0, kH, Color(118, 138, 156), Color(86, 105, 124)));
        fill();

        // brushed streaks
        for (int i = 0; i < kStreaks; ++i)
        {
            beginPath();
            moveTo(0, fStreakY[i]);
            lineTo(kW, fStreakY[i] + 0.4f);
            strokeColor(fStreakA[i] > 0.5f ? Color(255, 255, 255, fStreakA[i] * 0.05f)
                                           : Color(0, 0, 0, fStreakA[i] * 0.06f));
            strokeWidth(1.f);
            stroke();
        }

        // top bevel / bottom shadow
        beginPath(); rect(0, 0, kW, 3); fillColor(Color(255, 255, 255, 0.157f)); fill();
        beginPath(); rect(0, kH - 4, kW, 4); fillColor(Color(0, 0, 0, 0.275f)); fill();

        // rack ears
        for (int side = 0; side < 2; ++side)
        {
            const float x = side == 0 ? 0.f : kW - 40.f;
            beginPath();
            rect(x, 0, 40, kH);
            fillPaint(linearGradient(x, 0, x + 40, 0, Color(98, 116, 134), Color(80, 97, 114)));
            fill();
            beginPath();
            moveTo(side == 0 ? 40.f : kW - 40.f, 0);
            lineTo(side == 0 ? 40.f : kW - 40.f, kH);
            strokeColor(Color(0, 0, 0, 0.235f)); strokeWidth(1.5f); stroke();

            for (int k = 0; k < 2; ++k)
            {
                const float cy = k == 0 ? 60.f : kH - 60.f;
                // slotted mounting hole
                beginPath();
                roundedRect(x + 12, cy - 13, 16, 26, 8);
                fillColor(Color(20, 24, 28)); fill();
                // screw head
                drawScrew(x + 20, cy, 9);
            }
        }

        // title plate
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fontSize(30);
        fillColor(Color(0, 0, 0, 0.314f));
        text(71, 46, "EMBER", nullptr);
        fillColor(cream());
        text(70, 44, "EMBER", nullptr);

        fontSize(11);
        fillColor(cream(200));
        text(178, 37, "PROGRAM EQUALIZER", nullptr);
        text(178, 52, "TYPE EM-1   •   VACUUM TUBE", nullptr);

        // engraved divider
        beginPath(); moveTo(60, 74); lineTo(kW - 60, 74);
        strokeColor(Color(0, 0, 0, 0.275f)); strokeWidth(1.5f); stroke();
        beginPath(); moveTo(60, 75.5f); lineTo(kW - 60, 75.5f);
        strokeColor(Color(255, 255, 255, 0.137f)); strokeWidth(1.f); stroke();

        // vertical divider between EQ and output sections
        beginPath(); moveTo(655, 90); lineTo(655, kH - 25);
        strokeColor(Color(0, 0, 0, 0.275f)); strokeWidth(1.5f); stroke();
        beginPath(); moveTo(656.5f, 90); lineTo(656.5f, kH - 25);
        strokeColor(Color(255, 255, 255, 0.137f)); strokeWidth(1.f); stroke();

        // screws on face
        drawScrew(60, 28, 6); drawScrew(kW - 60, 28, 6);
        drawScrew(60, kH - 22, 6); drawScrew(kW - 60, kH - 22, 6);

        fontSize(9);
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        fillColor(cream(150));
        text(kW - 70, 44, "SERIAL No. 1957-0042", nullptr);
    }

    void drawSections()
    {
        beginPath(); moveTo(70, 247); lineTo(640, 247);
        strokeColor(Color(0, 0, 0, 0.25f)); strokeWidth(1.5f); stroke();
        beginPath(); moveTo(70, 248.5f); lineTo(640, 248.5f);
        strokeColor(Color(255, 255, 255, 0.12f)); strokeWidth(1.f); stroke();
    }

    // engraved silkscreen bracket with centred caption
    void bracket(float x0, float x1, float y, const char* label)
    {
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        fontSize(9);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        Rectangle<float> b;
        textBounds(0, 0, label, nullptr, b);
        const float tw = b.getWidth();
        const float cx = (x0 + x1) * 0.5f;
        beginPath();
        moveTo(x0, y + 6); lineTo(x0, y); lineTo(cx - tw / 2 - 8, y);
        moveTo(cx + tw / 2 + 8, y); lineTo(x1, y); lineTo(x1, y + 6);
        strokeColor(cream(120)); strokeWidth(1.f); stroke();
        fillColor(cream(170));
        text(cx, y, label, nullptr);
    }

    void drawKnob(const Knob& k)
    {
        const float v = fValues[k.param];
        const float norm = (v - k.min) / (k.max - k.min);

        // title above
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fontSize(12);
        fillColor(cream());
        text(k.cx, k.cy - k.r - 42, k.title, nullptr);

        const float a0 = -kPi * 0.75f, a1 = kPi * 0.75f; // -135..135 deg (0 = up)

        // scale
        if (k.steps > 0)
        {
            const float span = k.steps <= 4 ? 0.55f : 0.75f;
            for (int i = 0; i < k.steps; ++i)
            {
                const float t = k.steps > 1 ? (float)i / (k.steps - 1) : 0.f;
                const float a = -kPi * span + t * 2 * kPi * span;
                const float ax = std::sin(a), ay = -std::cos(a);
                beginPath();
                moveTo(k.cx + ax * (k.r + 6), k.cy + ay * (k.r + 6));
                lineTo(k.cx + ax * (k.r + 11), k.cy + ay * (k.r + 11));
                strokeColor(cream()); strokeWidth(1.6f); stroke();
                fontSize(11);
                fillColor((int)std::lround(v) == i ? Color(255, 236, 190) : cream(210));
                text(k.cx + ax * (k.r + 22), k.cy + ay * (k.r + 21), k.labels[i], nullptr);
            }
        }
        else
        {
            const int majors = 11;
            for (int i = 0; i <= 50; ++i)
            {
                const float t = i / 50.f;
                const float a = a0 + t * (a1 - a0);
                const float ax = std::sin(a), ay = -std::cos(a);
                const bool major = (i % 5) == 0;
                beginPath();
                moveTo(k.cx + ax * (k.r + 5), k.cy + ay * (k.r + 5));
                lineTo(k.cx + ax * (k.r + (major ? 11 : 8)), k.cy + ay * (k.r + (major ? 11 : 8)));
                strokeColor(cream(major ? 255 : 150)); strokeWidth(major ? 1.6f : 0.9f); stroke();
            }
            fontSize(10);
            fillColor(cream(220));
            const bool isOut = k.param == kOutput;
            for (int i = 0; i < majors; i += (isOut ? 1 : (k.r < 31 ? 2 : 1)))
            {
                if (isOut && i > 4) break;
                const float t = isOut ? i / 4.f : i / 10.f;
                const float a = a0 + t * (a1 - a0);
                char buf[8];
                if (isOut) std::snprintf(buf, sizeof(buf), i == 2 ? "0" : "%+d", (int)std::lround(-12 + t * 24));
                else std::snprintf(buf, sizeof(buf), "%d", i);
                text(k.cx + std::sin(a) * (k.r + 20), k.cy - std::cos(a) * (k.r + 19), buf, nullptr);
            }
            if (k.param == kBandwidth)
            {
                fontSize(9);
                text(k.cx - k.r - 14, k.cy + k.r + 24, "SHARP", nullptr);
                text(k.cx + k.r + 14, k.cy + k.r + 24, "BROAD", nullptr);
            }
        }

        if (k.sub)
        {
            fontSize(9);
            fillColor(cream(180));
            text(k.cx, k.cy + k.r + 24, k.sub, nullptr);
        }

        // knob angle
        float ang;
        if (k.steps > 0)
        {
            const float span = k.steps <= 4 ? 0.55f : 0.75f;
            const float t = k.steps > 1 ? std::round(v - k.min) / (k.steps - 1) : 0.f;
            ang = -kPi * span + t * 2 * kPi * span;
        }
        else
            ang = a0 + norm * (a1 - a0);

        // drop shadow
        const float sr = k.chicken ? k.r * 0.7f : k.r + 2;
        beginPath();
        circle(k.cx + 2, k.cy + 4, sr + 8);
        fillPaint(radialGradient(k.cx + 2, k.cy + 4, sr - 4, sr + 8, Color(0, 0, 0, 0.471f), Color(0, 0, 0, 0.000f)));
        fill();

        if (k.chicken)
            drawChickenHead(k.cx, k.cy, k.r, ang);
        else
            drawRoundKnob(k.cx, k.cy, k.r, ang);
    }

    void drawRoundKnob(float cx, float cy, float r, float ang)
    {
        // skirt
        beginPath();
        circle(cx, cy, r);
        fillPaint(linearGradient(cx, cy - r, cx, cy + r, Color(48, 46, 44), Color(12, 12, 12)));
        fill();
        // fluted grip
        const int flutes = 24;
        for (int i = 0; i < flutes; ++i)
        {
            const float a = ang + i * 2 * kPi / flutes;
            beginPath();
            moveTo(cx + std::sin(a) * r * 0.62f, cy - std::cos(a) * r * 0.62f);
            lineTo(cx + std::sin(a) * r * 0.74f, cy - std::cos(a) * r * 0.74f);
            strokeColor(Color(255, 255, 255, 0.071f)); strokeWidth(2.f); stroke();
        }
        // cap
        beginPath();
        circle(cx, cy, r * 0.6f);
        fillPaint(radialGradient(cx - r * 0.2f, cy - r * 0.25f, 1, r * 0.75f, Color(70, 67, 63), Color(18, 18, 18)));
        fill();
        beginPath();
        circle(cx, cy, r);
        strokeColor(Color(0, 0, 0, 0.627f)); strokeWidth(1.f); stroke();
        // pointer line on skirt
        const float ax = std::sin(ang), ay = -std::cos(ang);
        beginPath();
        moveTo(cx + ax * r * 0.35f, cy + ay * r * 0.35f);
        lineTo(cx + ax * (r - 2), cy + ay * (r - 2));
        strokeColor(Color(245, 238, 220)); strokeWidth(3.f); lineCap(ROUND); stroke();
    }

    void drawChickenHead(float cx, float cy, float r, float ang)
    {
        save();
        translate(cx, cy);
        rotate(ang);

        // base disc
        beginPath();
        circle(0, 0, r * 0.62f);
        fillPaint(linearGradient(0, -r, 0, r, Color(50, 47, 44), Color(10, 10, 10)));
        fill();

        // beak (pointer) - tapered body extending upward
        beginPath();
        moveTo(-r * 0.42f, r * 0.15f);
        bezierTo(-r * 0.48f, -r * 0.45f, -r * 0.18f, -r * 1.02f, 0, -r * 1.12f);
        bezierTo(r * 0.18f, -r * 1.02f, r * 0.48f, -r * 0.45f, r * 0.42f, r * 0.15f);
        bezierTo(r * 0.36f, r * 0.62f, -r * 0.36f, r * 0.62f, -r * 0.42f, r * 0.15f);
        closePath();
        fillPaint(linearGradient(-r * 0.5f, 0, r * 0.5f, 0, Color(58, 55, 51), Color(14, 14, 14)));
        fill();
        strokeColor(Color(0, 0, 0, 0.706f)); strokeWidth(1.f); stroke();

        // highlight ridge
        beginPath();
        moveTo(-r * 0.12f, -r * 0.2f);
        bezierTo(-r * 0.14f, -r * 0.6f, -r * 0.06f, -r * 0.92f, 0, -r * 1.0f);
        strokeColor(Color(255, 255, 255, 0.157f)); strokeWidth(2.f); stroke();

        // white index line
        beginPath();
        moveTo(0, -r * 0.25f);
        lineTo(0, -r * 1.02f);
        strokeColor(Color(245, 238, 220)); strokeWidth(2.6f); lineCap(ROUND); stroke();

        restore();
    }

    void drawMeter()
    {
        const float x = 690, y = 96, w = 240, h = 128;

        // bezel
        beginPath();
        roundedRect(x - 8, y - 8, w + 16, h + 16, 8);
        fillPaint(linearGradient(x, y - 8, x, y + h + 8, Color(40, 40, 40), Color(10, 10, 10)));
        fill();

        // face
        beginPath();
        roundedRect(x, y, w, h, 4);
        fillPaint(linearGradient(x, y, x, y + h, Color(246, 226, 170), Color(222, 196, 132)));
        fill();

        const float px = x + w / 2, py = y + h + 28; // needle pivot (below window)
        const float rr = 140;
        const float amin = -0.82f, amax = 0.82f;

        auto dbToAng = [&](float db) {
            // VU-like: -20..+3
            const float lin = std::pow(10.f, db / 20.f);
            const float l0 = std::pow(10.f, -20.f / 20.f), l1 = std::pow(10.f, 3.f / 20.f);
            float t = (lin - l0) / (l1 - l0);
            t = t < 0 ? 0 : (t > 1.05f ? 1.05f : t);
            return amin + t * (amax - amin);
        };

        save();
        scissor(x, y, w, h);

        // red zone arc
        beginPath();
        arc(px, py, rr - 18, dbToAng(0) - kPi / 2, amax - kPi / 2, CW);
        strokeColor(Color(190, 30, 25)); strokeWidth(6.f); stroke();
        // main arc
        beginPath();
        arc(px, py, rr - 24, amin - kPi / 2, dbToAng(0) - kPi / 2, CW);
        strokeColor(Color(30, 25, 20)); strokeWidth(1.4f); stroke();

        static const float marks[] = { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 };
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fontSize(10);
        for (float m : marks)
        {
            const float a = dbToAng(m);
            const float ax = std::sin(a), ay = -std::cos(a);
            beginPath();
            moveTo(px + ax * (rr - 24), py + ay * (rr - 24));
            lineTo(px + ax * (rr - 14), py + ay * (rr - 14));
            strokeColor(m > 0 ? Color(190, 30, 25) : Color(30, 25, 20)); strokeWidth(1.4f); stroke();
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", (int)std::fabs(m));
            fillColor(m > 0 ? Color(190, 30, 25) : Color(30, 25, 20));
            text(px + ax * (rr - 3), py + ay * (rr - 3), buf, nullptr);
        }
        fontSize(18);
        fillColor(Color(30, 25, 20));
        text(px, y + h - 32, "VU", nullptr);
        fontSize(8);
        text(px, y + h - 16, "EMBER AUDIO", nullptr);

        // needle
        float db = fValues[kPower] > 0.5f ? fValues[kMeter] : -60.f;
        const float a = dbToAng(db);
        beginPath();
        moveTo(px, py);
        lineTo(px + std::sin(a) * (rr - 6), py - std::cos(a) * (rr - 6));
        strokeColor(Color(20, 20, 20)); strokeWidth(1.6f); stroke();

        restore();

        // glass glare
        beginPath();
        roundedRect(x, y, w, h * 0.45f, 4);
        fillPaint(linearGradient(x, y, x, y + h * 0.45f, Color(255, 255, 255, 0.235f), Color(255, 255, 255, 0.000f)));
        fill();

        // pivot cover
        beginPath();
        rect(x, y + h - 6, w, 6);
        fillColor(Color(25, 25, 25, 0.784f)); fill();
    }

    // ------------------------------------------------------------------
    // vacuum tube seen through a window in the faceplate; glows with processing activity

    void drawTube()
    {
        const bool on = fValues[kPower] > 0.5f;
        // heater always glows faintly when powered; activity drives it brighter
        const float act = on ? fValues[kGlow] : 0.f;
        const float g = on ? 0.22f + 0.78f * act : 0.f;

        const float wx = 405, wy = 14, ww = 215, wh = 52;     // window
        const float cx = wx + ww * 0.5f, cy = wy + wh * 0.5f;

        // recessed window with dark interior lit by the tube
        beginPath();
        roundedRect(wx - 4, wy - 4, ww + 8, wh + 8, 9);
        fillPaint(linearGradient(wx, wy - 4, wx, wy + wh + 4, Color(40, 48, 56), Color(150, 168, 184)));
        fill();
        beginPath();
        roundedRect(wx, wy, ww, wh, 6);
        fillPaint(linearGradient(wx, wy, wx, wy + wh, Color(8, 9, 10), Color(26, 26, 26)));
        fill();
        if (g > 0.f)
        {
            beginPath();
            roundedRect(wx, wy, ww, wh, 6);
            fillPaint(radialGradient(cx - 10, cy, 6, ww * 0.55f,
                                     Color(255, 110, 30, 0.38f * g), Color(255, 90, 20, 0.f)));
            fill();
        }

        save();
        scissor(wx, wy, ww, wh);

        // tube lies horizontally: base/pins on the right, getter dome on the left
        const float tx0 = wx + 18, tx1 = wx + ww - 34, th = 34;
        const float ty0 = cy - th * 0.5f;

        // pins + bakelite base
        for (int i = 0; i < 4; ++i)
        {
            beginPath();
            rect(tx1 + 10, cy - 12 + i * 8, 14, 2);
            fillColor(Color(170, 170, 165)); fill();
        }
        beginPath();
        roundedRect(tx1 - 4, ty0 + 2, 16, th - 4, 3);
        fillPaint(linearGradient(0, ty0, 0, ty0 + th, Color(60, 50, 40), Color(20, 16, 12)));
        fill();

        // internal structure (behind glass)
        const float px0 = tx0 + 34, px1 = tx1 - 14;
        // mica spacers
        for (int i = 0; i < 2; ++i)
        {
            const float mx = i == 0 ? px0 - 6 : px1 + 2;
            beginPath();
            rect(mx, ty0 + 4, 4, th - 8);
            fillColor(Color(200, 190, 160, 0.55f)); fill();
        }
        // two triode sections: plates with a glowing cathode/heater gap
        const float secW = (px1 - px0 - 8) * 0.5f;
        for (int sct = 0; sct < 2; ++sct)
        {
            const float sx = px0 + sct * (secW + 8);
            // heater glow through the gap between the plate halves
            if (g > 0.f)
            {
                beginPath();
                rect(sx - 4, cy - 9, secW + 8, 18);
                fillPaint(radialGradient(sx + secW * 0.5f, cy, 1, secW * 0.75f,
                                         Color(255, 170, 70, 0.95f * g), Color(255, 80, 10, 0.f)));
                fill();
                beginPath();
                rect(sx, cy - 1.2f, secW, 2.4f);
                fillColor(Color(255, 200 + (int)(40 * g), 120 + (int)(90 * g), 0.35f + 0.65f * g));
                fill();
            }
            // plates (upper / lower halves)
            for (int h = 0; h < 2; ++h)
            {
                const float py = h == 0 ? ty0 + 5 : cy + 3;
                beginPath();
                rect(sx, py, secW, th * 0.5f - 8);
                fillPaint(linearGradient(0, py, 0, py + th * 0.5f - 8,
                                         Color(70, 70, 72), Color(30, 30, 32)));
                fill();
                // plate edge catching the glow
                beginPath();
                rect(sx, h == 0 ? py + th * 0.5f - 9.5f : py, secW, 1.5f);
                fillColor(Color(255, 140, 60, 0.15f + 0.6f * g));
                fill();
            }
            // grid wires
            for (int w = 0; w < 6; ++w)
            {
                const float gx = sx + 3 + w * (secW - 6) / 5.f;
                beginPath();
                moveTo(gx, cy - 6); lineTo(gx, cy + 6);
                strokeColor(Color(150, 140, 120, 0.6f)); strokeWidth(0.6f); stroke();
            }
        }

        // getter flash (silver mirror at the tip)
        beginPath();
        ellipse(tx0 + 14, cy, 14, th * 0.5f - 2);
        fillPaint(linearGradient(tx0, ty0, tx0, ty0 + th, Color(200, 205, 215, 0.85f), Color(60, 62, 70, 0.9f)));
        fill();

        // glass envelope
        beginPath();
        moveTo(tx1, ty0);
        lineTo(tx0 + th * 0.5f, ty0);
        arcTo(tx0 - 2, ty0, tx0 - 2, cy, th * 0.5f);
        arcTo(tx0 - 2, ty0 + th, tx0 + th * 0.5f, ty0 + th, th * 0.5f);
        lineTo(tx1, ty0 + th);
        closePath();
        fillPaint(linearGradient(0, ty0, 0, ty0 + th, Color(255, 255, 255, 0.10f), Color(255, 255, 255, 0.02f)));
        fill();
        strokeColor(Color(220, 230, 240, 0.35f)); strokeWidth(1.f); stroke();
        // glass highlight
        beginPath();
        roundedRect(tx0 + 10, ty0 + 3, tx1 - tx0 - 16, 3, 1.5f);
        fillColor(Color(255, 255, 255, 0.28f)); fill();
        // warm reflection on the glass when hot
        if (g > 0.f)
        {
            beginPath();
            roundedRect(tx0 + 10, ty0 + th - 6, tx1 - tx0 - 16, 2.5f, 1.2f);
            fillColor(Color(255, 150, 70, 0.35f * g)); fill();
        }

        restore();

        // light spilling out of the window onto the panel
        if (g > 0.f)
        {
            beginPath();
            rect(wx - 40, wy - 20, ww + 80, wh + 40);
            fillPaint(boxGradient(wx, wy, ww, wh, 8, 30,
                                  Color(255, 120, 40, 0.18f * g * g), Color(255, 120, 40, 0.f)));
            fill();
        }

        // caption
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        fontSize(9);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(cream(170));
        text(wx + ww + 14, cy - 6, "V1", nullptr);
        text(wx + ww + 14, cy + 7, "12AX7", nullptr);
    }

    void drawPower()
    {
        const bool on = fValues[kPower] > 0.5f;
        const float cx = 810, cy = 322;

        fontFace(NANOVG_DEJAVU_SANS_TTF);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fontSize(12);
        fillColor(cream());
        text(cx, 266, "POWER", nullptr);

        // jewel lamp
        beginPath();
        circle(cx, 290, 10);
        fillColor(Color(30, 30, 30)); fill();
        beginPath();
        circle(cx, 290, 7.5f);
        fillPaint(radialGradient(cx - 2, 288, 1, 9,
                                 on ? Color(255, 120, 90) : Color(110, 30, 25),
                                 on ? Color(200, 20, 10) : Color(60, 10, 8)));
        fill();
        if (on)
        {
            beginPath();
            circle(cx, 290, 18);
            fillPaint(radialGradient(cx, 290, 6, 18, Color(255, 60, 30, 0.353f), Color(255, 60, 30, 0.000f)));
            fill();
        }

        // toggle switch plate
        beginPath();
        roundedRect(cx - 14, cy - 8, 28, 40, 4);
        fillColor(Color(60, 70, 80)); fill();
        beginPath();
        circle(cx, cy + 12, 8);
        fillPaint(radialGradient(cx - 2, cy + 10, 1, 9, Color(230, 230, 230), Color(120, 120, 120)));
        fill();
        // bat lever
        const float ty = on ? cy - 6 : cy + 30;
        beginPath();
        moveTo(cx - 3, cy + 12);
        lineTo(cx - 4.5f, ty);
        lineTo(cx + 4.5f, ty);
        lineTo(cx + 3, cy + 12);
        closePath();
        fillPaint(linearGradient(cx - 5, 0, cx + 5, 0, Color(250, 250, 250), Color(140, 140, 140)));
        fill();
        beginPath();
        circle(cx, ty, 5);
        fillPaint(radialGradient(cx - 1.5f, ty - 1.5f, 1, 6, Color(255, 255, 255), Color(150, 150, 150)));
        fill();

        fontSize(9);
        fillColor(cream(200));
        text(cx + 28, cy - 4, "ON", nullptr);
        text(cx + 28, cy + 30, "OFF", nullptr);
    }

    void drawScrew(float cx, float cy, float r)
    {
        beginPath();
        circle(cx, cy, r);
        fillPaint(radialGradient(cx - r * 0.3f, cy - r * 0.3f, 1, r * 1.2f, Color(220, 222, 224), Color(110, 112, 116)));
        fill();
        strokeColor(Color(0, 0, 0, 0.392f)); strokeWidth(1.f); stroke();
        beginPath();
        moveTo(cx - r * 0.7f, cy - r * 0.2f);
        lineTo(cx + r * 0.7f, cy + r * 0.2f);
        strokeColor(Color(60, 60, 64)); strokeWidth(r * 0.28f); stroke();
    }

    static Color cream(int alpha = 255) { return Color(240, 228, 196, alpha / 255.f); }

    // ------------------------------------------------------------------
    // interaction

    int hitTest(double mx, double my) const
    {
        for (int i = 0; i < kNumKnobs; ++i)
        {
            const float dx = mx - fKnobs[i].cx, dy = my - fKnobs[i].cy;
            if (dx * dx + dy * dy <= (fKnobs[i].r + 8) * (fKnobs[i].r + 8))
                return i;
        }
        return -1;
    }

    bool hitPower(double mx, double my) const
    {
        return mx > 790 && mx < 830 && my > 278 && my < 355;
    }

    void setValue(uint32_t param, float v)
    {
        fValues[param] = v;
        setParameterValue(param, v);
        repaint();
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1) return false;
        const float s = getWidth() / kW;
        const double mx = ev.pos.getX() / s, my = ev.pos.getY() / s;

        if (ev.press)
        {
            if (hitPower(mx, my))
            {
                editParameter(kPower, true);
                setValue(kPower, fValues[kPower] > 0.5f ? 0.f : 1.f);
                editParameter(kPower, false);
                return true;
            }

            const int k = hitTest(mx, my);
            if (k < 0) return false;
            const Knob& kn = fKnobs[k];

            // double click => reset
            if (k == fLastClickKnob && ev.time - fLastClickTime < 350)
            {
                editParameter(kn.param, true);
                setValue(kn.param, kn.def);
                editParameter(kn.param, false);
                fLastClickKnob = -1;
                return true;
            }
            fLastClickKnob = k;
            fLastClickTime = ev.time;

            fDrag = k;
            fDragY = my;
            fDragValue = fValues[kn.param];
            editParameter(kn.param, true);
            return true;
        }
        else if (fDrag >= 0)
        {
            editParameter(fKnobs[fDrag].param, false);
            fDrag = -1;
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if (fDrag < 0) return false;
        const float s = getWidth() / kW;
        const double my = ev.pos.getY() / s;
        const Knob& kn = fKnobs[fDrag];

        const double range = kn.max - kn.min;
        const double sens = (ev.mod & kModifierShift) ? 900.0 : (kn.steps > 0 ? 120.0 : 220.0);
        fDragValue += (fDragY - my) / sens * range;
        fDragY = my;
        if (fDragValue < kn.min) fDragValue = kn.min;
        if (fDragValue > kn.max) fDragValue = kn.max;

        float v = (float)fDragValue;
        if (kn.steps > 0) v = std::round(v);
        if (v != fValues[kn.param]) setValue(kn.param, v);
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float s = getWidth() / kW;
        const int k = hitTest(ev.pos.getX() / s, ev.pos.getY() / s);
        if (k < 0) return false;
        const Knob& kn = fKnobs[k];
        const float dir = ev.delta.getY() > 0 ? 1.f : (ev.delta.getY() < 0 ? -1.f : 0.f);
        float step = kn.steps > 0 ? 1.f : (kn.max - kn.min) / ((ev.mod & kModifierShift) ? 200.f : 40.f);
        float v = fValues[kn.param] + dir * step;
        if (v < kn.min) v = kn.min;
        if (v > kn.max) v = kn.max;
        editParameter(kn.param, true);
        setValue(kn.param, v);
        editParameter(kn.param, false);
        return true;
    }

private:
    static constexpr int kNumKnobs = 10;
    static constexpr int kStreaks = 160;
    Knob fKnobs[kNumKnobs];
    float fValues[kParameterCount];
    float fStreakY[kStreaks], fStreakA[kStreaks];

    int fDrag = -1;
    double fDragY = 0, fDragValue = 0;
    int fLastClickKnob = -1;
    uint fLastClickTime = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EmberEQUI)
};

UI* createUI()
{
    return new EmberEQUI();
}

END_NAMESPACE_DISTRHO
