#include "PluginEditor.h"

#ifndef SORA_HAS_ASSETS
 #define SORA_HAS_ASSETS 0
#endif

#if SORA_HAS_ASSETS
 #include <BinaryData.h>
#endif

namespace
{
    constexpr int windowW = 1000;
    constexpr int windowH = 700;

    const juce::Colour whiteSoft (0xffeef4ff);
    const juce::Colour skyBlue   (0xff8fd0ff);

    //--------------------------------------------------------------------------
    // Необязательные собственные картинки из Resources/*.png
    juce::Image loadAsset (const juce::String& resourceName)
    {
       #if SORA_HAS_ASSETS
        int size = 0;
        if (auto* data = BinaryData::getNamedResource (resourceName.toRawUTF8(), size))
            return juce::ImageFileFormat::loadFrom (data, (size_t) size);
       #else
        juce::ignoreUnused (resourceName);
       #endif
        return {};
    }

    //--------------------------------------------------------------------------
    // Процедурные фоны: по одному на каждый пункт Character
    struct SceneDef
    {
        juce::uint32 top, mid, bottom, hillFar, hillNear, sun;
        float sunX, sunY;   // доли окна, -1 = нет
        int kind;           // 0 облака, 1 звёзды, 2 лепестки, 3 снег, 4 листья, 5 дождь, 6 крона дерева
    };

    const SceneDef sceneDefs[] =
    {
        { 0xff5fb0ea, 0xffa6d8f5, 0xffe9f6fd, 0xff9cd49a, 0xff6fba78, 0xfffff4c2, 0.78f, 0.17f, 0 },  // ясный день
        { 0xff5aa9e6, 0xff9fd3f3, 0xffe3f3fb, 0xff86c48a, 0xff5da86a, 0xfffff2b8, 0.30f, 0.12f, 6 },  // солнечная крона
        { 0xff5a4a9a, 0xffe9769a, 0xffffc78a, 0xff6b4a7a, 0xff3f2c5a, 0xffffe3a0, 0.50f, 0.33f, 0 },  // закат
        { 0xff0f1740, 0xff2b3a7a, 0xff6a78c0, 0xff1b2350, 0xff10163a, 0xfffff6d6, 0.76f, 0.16f, 1 },  // ночь
        { 0xff55606f, 0xff7d8a99, 0xffb5c0cc, 0xff5f7380, 0xff465866, 0x00000000, -1.f,  -1.f,  5 },  // дождь
        { 0xff9fd2f2, 0xffe8d4ee, 0xfffbe6ef, 0xffd5e8c2, 0xffa8d09a, 0xffffffff, 0.20f, 0.15f, 2 },  // сакура
        { 0xff3a5d9a, 0xff8f9fd0, 0xfff3c1a4, 0xff55608a, 0xff3b4470, 0xffffd9a8, 0.28f, 0.34f, 0 },  // вечер
        { 0xffe8b76a, 0xfff2d3a0, 0xfffbeed8, 0xffc98a3c, 0xff9c5f26, 0xfffff0c0, 0.80f, 0.14f, 4 },  // осень
        { 0xffb9d6ee, 0xffdcebf6, 0xfff8fbfe, 0xffeef5fb, 0xffd3e4f3, 0xffffffff, -1.f,  -1.f,  3 },  // снег
    };

    constexpr int numScenes = (int) (sizeof (sceneDefs) / sizeof (sceneDefs[0]));

    void drawCloud (juce::Graphics& g, float x, float y, float s, juce::Colour c)
    {
        g.setColour (c);
        g.fillEllipse (x,            y + 14.f * s, 90.f * s, 34.f * s);
        g.fillEllipse (x + 14.f * s, y,            44.f * s, 40.f * s);
        g.fillEllipse (x + 40.f * s, y + 6.f * s,  40.f * s, 34.f * s);
        g.fillRoundedRectangle (x + 8.f * s, y + 26.f * s, 80.f * s, 22.f * s, 11.f * s);
    }

    void fillHills (juce::Graphics& g, juce::Colour c, float w, float h,
                    float baseY, float amp, float freq, float phase)
    {
        juce::Path p;
        p.startNewSubPath (0.f, h);
        for (float x = 0.f; x <= w; x += 8.f)
            p.lineTo (x, baseY + std::sin (x * freq + phase) * amp
                              + std::sin (x * freq * 2.3f + phase * 1.7f) * amp * 0.4f);
        p.lineTo (w, h);
        p.closeSubPath();
        g.setColour (c);
        g.fillPath (p);
    }

    juce::Image renderScene (int index, int w, int h)
    {
        const auto& s = sceneDefs[index % numScenes];
        juce::Image img (juce::Image::ARGB, w, h, true);
        juce::Graphics g (img);

        const float fw = (float) w, fh = (float) h;
        juce::Random rng (1234 + index * 77);

        // небо
        juce::ColourGradient sky (juce::Colour (s.top), 0.f, 0.f, juce::Colour (s.bottom), 0.f, fh * 0.55f, false);
        sky.addColour (0.5, juce::Colour (s.mid));
        g.setGradientFill (sky);
        g.fillAll();

        // звёзды
        if (s.kind == 1)
        {
            for (int i = 0; i < 160; ++i)
            {
                g.setColour (juce::Colours::white.withAlpha (0.3f + 0.7f * rng.nextFloat()));
                const float r = 0.8f + rng.nextFloat() * 1.8f;
                g.fillEllipse (rng.nextFloat() * fw, rng.nextFloat() * fh * 0.5f, r, r);
            }
        }

        // солнце / луна
        if (s.sunX >= 0.f)
        {
            const float sx = s.sunX * fw, sy = s.sunY * fh;
            juce::ColourGradient glow (juce::Colour (s.sun).withAlpha (0.55f), sx, sy,
                                       juce::Colour (s.sun).withAlpha (0.f), sx + 170.f, sy, true);
            g.setGradientFill (glow);
            g.fillEllipse (sx - 170.f, sy - 170.f, 340.f, 340.f);
            g.setColour (juce::Colour (s.sun));
            g.fillEllipse (sx - 34.f, sy - 34.f, 68.f, 68.f);
        }

        // облака
        if (s.kind == 0 || s.kind == 2 || s.kind == 3)
        {
            const auto c = juce::Colours::white.withAlpha (0.5f);
            drawCloud (g, fw * 0.05f, 40.f,  1.4f, c);
            drawCloud (g, fw * 0.38f, 110.f, 1.0f, c);
            drawCloud (g, fw * 0.62f, 30.f,  1.2f, c);
            drawCloud (g, fw * 0.88f, 120.f, 0.9f, c);
        }
        if (s.kind == 5)
        {
            const auto c = juce::Colour (0xff48515f).withAlpha (0.65f);
            for (int i = 0; i < 6; ++i)
                drawCloud (g, -20.f + (float) i * fw * 0.2f, 10.f + rng.nextFloat() * 50.f, 1.8f, c);
        }

        // крона дерева и ствол
        if (s.kind == 6)
        {
            g.setColour (juce::Colour (0xff5b4030));
            g.fillRect (fw * 0.86f, 40.f, 36.f, fh);

            for (int layer = 0; layer < 2; ++layer)
            {
                for (int i = 0; i < 70; ++i)
                {
                    const float k = layer == 0 ? 1.f : 0.6f;
                    const float cx = rng.nextFloat() * fw;
                    const float cy = -20.f + rng.nextFloat() * 150.f + (layer == 1 ? 10.f : 0.f);
                    const float r  = (34.f + rng.nextFloat() * 56.f) * k;
                    g.setColour (layer == 0 ? juce::Colour (0xff2f6b3a).withAlpha (0.95f)
                                            : juce::Colour (0xff58a85a).withAlpha (0.8f));
                    g.fillEllipse (cx - r, cy - r, r * 2.f, r * 1.7f);
                }
            }
            for (int i = 0; i < 70; ++i)   // солнечные блики
            {
                g.setColour (juce::Colour (0xfffff7c2).withAlpha (0.25f + 0.2f * rng.nextFloat()));
                const float r = 8.f + rng.nextFloat() * 14.f;
                g.fillEllipse (rng.nextFloat() * fw, rng.nextFloat() * 150.f, r, r * 0.8f);
            }
        }

        // холмы
        fillHills (g, juce::Colour (s.hillFar),  fw, fh, fh * 0.40f, 18.f, 0.006f, (float) index);
        fillHills (g, juce::Colour (s.hillNear), fw, fh, fh * 0.47f, 14.f, 0.009f, (float) index * 1.7f + 1.f);

        // частицы
        if (s.kind == 2)   // лепестки
        {
            for (int i = 0; i < 90; ++i)
            {
                const float x = rng.nextFloat() * fw, y = rng.nextFloat() * fh * 0.52f;
                g.saveState();
                g.addTransform (juce::AffineTransform::rotation (rng.nextFloat() * 6.28f, x, y));
                g.setColour (juce::Colour (0xfff8b9cc).withAlpha (0.5f + 0.5f * rng.nextFloat()));
                g.fillEllipse (x, y, 8.f, 4.f);
                g.restoreState();
            }
        }
        else if (s.kind == 3)   // снег
        {
            for (int i = 0; i < 180; ++i)
            {
                g.setColour (juce::Colours::white.withAlpha (0.5f + 0.5f * rng.nextFloat()));
                const float r = 1.5f + rng.nextFloat() * 2.5f;
                g.fillEllipse (rng.nextFloat() * fw, rng.nextFloat() * fh * 0.52f, r, r);
            }
        }
        else if (s.kind == 4)   // листья
        {
            const juce::uint32 cols[] = { 0xffd9632b, 0xffe8a33a, 0xffb5472a };
            for (int i = 0; i < 80; ++i)
            {
                const float x = rng.nextFloat() * fw, y = rng.nextFloat() * fh * 0.52f;
                g.saveState();
                g.addTransform (juce::AffineTransform::rotation (rng.nextFloat() * 6.28f, x, y));
                g.setColour (juce::Colour (cols[rng.nextInt (3)]).withAlpha (0.85f));
                g.fillEllipse (x, y, 10.f, 5.f);
                g.restoreState();
            }
        }
        else if (s.kind == 5)   // дождь
        {
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            for (int i = 0; i < 220; ++i)
            {
                const float x = rng.nextFloat() * fw, y = rng.nextFloat() * fh * 0.55f;
                g.drawLine (x, y, x - 6.f, y + 22.f, 1.2f);
            }
        }

        return img;
    }

    //--------------------------------------------------------------------------
    // Оригинальная героиня, нарисованная векторами (спрайт по умолчанию)
    void drawHeroine (juce::Graphics& g, juce::Rectangle<float> a)
    {
        const float cx = a.getCentreX();
        const float R  = a.getWidth() * 0.27f;
        const float cy = a.getY() + a.getHeight() * 0.38f;

        const juce::Colour hair    (0xff9a8ae0);
        const juce::Colour hairDark(0xff7766c4);
        const juce::Colour skin    (0xfffde3d2);
        const juce::Colour blouse  (0xfff9fcff);
        const juce::Colour collar  (0xff5aa9e6);
        const juce::Colour ribbon  (0xfff0708f);
        const juce::Colour iris    (0xff3f8fd6);
        const juce::Colour dark    (0xff3b3350);

        g.setColour (hairDark);
        g.fillRoundedRectangle (cx - R * 1.3f, cy - R * 1.2f, R * 2.6f, R * 4.0f, R * 1.1f);

        juce::Path body;
        body.startNewSubPath (cx - R * 1.6f, a.getBottom());
        body.quadraticTo (cx - R * 1.6f, cy + R * 1.5f, cx - R * 0.5f, cy + R * 1.25f);
        body.lineTo (cx + R * 0.5f, cy + R * 1.25f);
        body.quadraticTo (cx + R * 1.6f, cy + R * 1.5f, cx + R * 1.6f, a.getBottom());
        body.closeSubPath();
        g.setColour (blouse);
        g.fillPath (body);

        g.setColour (skin);
        g.fillRoundedRectangle (cx - R * 0.22f, cy + R * 0.85f, R * 0.44f, R * 0.6f, R * 0.15f);

        juce::Path col;
        col.startNewSubPath (cx - R * 0.5f,  cy + R * 1.25f);
        col.lineTo (cx,                     cy + R * 2.05f);
        col.lineTo (cx + R * 0.5f,           cy + R * 1.25f);
        col.lineTo (cx + R * 0.95f,          cy + R * 1.45f);
        col.lineTo (cx,                     cy + R * 2.75f);
        col.lineTo (cx - R * 0.95f,          cy + R * 1.45f);
        col.closeSubPath();
        g.setColour (collar);
        g.fillPath (col);

        juce::Path bow;
        bow.addTriangle (cx, cy + R * 2.05f, cx - R * 0.42f, cy + R * 1.85f, cx - R * 0.42f, cy + R * 2.3f);
        bow.addTriangle (cx, cy + R * 2.05f, cx + R * 0.42f, cy + R * 1.85f, cx + R * 0.42f, cy + R * 2.3f);
        g.setColour (ribbon);
        g.fillPath (bow);
        g.fillEllipse (cx - R * 0.11f, cy + R * 1.94f, R * 0.22f, R * 0.22f);

        g.setColour (skin);
        g.fillEllipse (cx - R, cy - R * 1.05f, R * 2.f, R * 2.15f);

        g.setColour (juce::Colour (0x55f48fb1));
        g.fillEllipse (cx - R * 0.78f, cy + R * 0.34f, R * 0.34f, R * 0.18f);
        g.fillEllipse (cx + R * 0.44f, cy + R * 0.34f, R * 0.34f, R * 0.18f);

        for (int s = -1; s <= 1; s += 2)
        {
            const float ex = cx + (float) s * R * 0.42f;
            const float ey = cy + R * 0.12f;

            g.setColour (juce::Colours::white);
            g.fillEllipse (ex - R * 0.2f, ey - R * 0.28f, R * 0.4f, R * 0.56f);
            g.setColour (iris);
            g.fillEllipse (ex - R * 0.17f, ey - R * 0.22f, R * 0.34f, R * 0.5f);
            g.setColour (dark);
            g.fillEllipse (ex - R * 0.09f, ey - R * 0.08f, R * 0.18f, R * 0.28f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (ex - R * 0.1f, ey - R * 0.2f, R * 0.1f, R * 0.1f);

            juce::Path lash;
            lash.startNewSubPath (ex - R * 0.22f, ey - R * 0.12f);
            lash.quadraticTo (ex, ey - R * 0.4f, ex + R * 0.22f, ey - R * 0.12f);
            g.setColour (dark);
            g.strokePath (lash, juce::PathStrokeType (R * 0.06f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        juce::Path mouth;
        mouth.startNewSubPath (cx - R * 0.13f, cy + R * 0.62f);
        mouth.quadraticTo (cx, cy + R * 0.76f, cx + R * 0.13f, cy + R * 0.62f);
        g.setColour (juce::Colour (0xffd9777f));
        g.strokePath (mouth, juce::PathStrokeType (R * 0.05f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        juce::Path bangs;
        bangs.startNewSubPath (cx - R * 1.1f, cy - R * 0.2f);
        bangs.quadraticTo (cx - R * 1.25f, cy - R * 1.45f, cx, cy - R * 1.3f);
        bangs.quadraticTo (cx + R * 1.25f, cy - R * 1.45f, cx + R * 1.1f, cy - R * 0.2f);
        bangs.quadraticTo (cx + R * 0.95f, cy - R * 0.7f, cx + R * 0.6f,  cy - R * 0.35f);
        bangs.quadraticTo (cx + R * 0.3f,  cy - R * 0.8f, cx + R * 0.05f, cy - R * 0.3f);
        bangs.quadraticTo (cx - R * 0.3f,  cy - R * 0.8f, cx - R * 0.55f, cy - R * 0.35f);
        bangs.quadraticTo (cx - R * 0.95f, cy - R * 0.7f, cx - R * 1.1f,  cy - R * 0.2f);
        bangs.closeSubPath();
        g.setColour (hair);
        g.fillPath (bangs);

        g.fillRoundedRectangle (cx - R * 1.12f, cy - R * 0.5f, R * 0.26f, R * 1.5f, R * 0.13f);
        g.fillRoundedRectangle (cx + R * 0.86f, cy - R * 0.5f, R * 0.26f, R * 1.5f, R * 0.13f);

        juce::Path clip;
        const float bx = cx + R * 0.82f, by = cy - R * 0.98f;
        clip.addTriangle (bx, by, bx - R * 0.34f, by - R * 0.2f, bx - R * 0.34f, by + R * 0.22f);
        clip.addTriangle (bx, by, bx + R * 0.34f, by - R * 0.2f, bx + R * 0.34f, by + R * 0.22f);
        g.setColour (juce::Colours::white);
        g.fillPath (clip);
        g.setColour (collar);
        g.fillEllipse (bx - R * 0.08f, by - R * 0.08f, R * 0.16f, R * 0.16f);
    }
}

//==============================================================================
SoraLookAndFeel::SoraLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, whiteSoft);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, whiteSoft);

    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xcc1d2238));
    setColour (juce::ComboBox::textColourId, whiteSoft);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha (0.45f));
    setColour (juce::ComboBox::arrowColourId, skyBlue);

    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf01d2238));
    setColour (juce::PopupMenu::textColourId, whiteSoft);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff4a6fb5));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

juce::Font SoraLookAndFeel::getLabelFont (juce::Label& l)
{
    return vnFont (l.getFont().getHeight(), l.getFont().isBold());
}

juce::Font SoraLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return vnFont (15.f);
}

juce::Font SoraLookAndFeel::getPopupMenuFont()
{
    return vnFont (15.f);
}

void SoraLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float pos, float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (5.f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    g.setColour (juce::Colour (0x66101424));
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.f, radius * 2.f);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 3.f, radius - 3.f, 0.f, startAngle, endAngle, true);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.strokePath (track, juce::PathStrokeType (3.f));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - 3.f, radius - 3.f, 0.f, startAngle, angle, true);
    g.setColour (skyBlue);
    g.strokePath (value, juce::PathStrokeType (3.f));

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -radius + 8.f, 3.f, radius * 0.34f, 1.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (juce::Colours::white);
    g.fillPath (pointer);
}

//==============================================================================
SoraKickAudioProcessorEditor::SoraKickAudioProcessorEditor (SoraKickAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    buildScenes();
    sprite = loadAsset ("heroine_png");

    characterBox.addItemList (SoraKickAudioProcessor::getCharacterNames(), 1);
    driveTypeBox.addItemList (SoraKickAudioProcessor::getDriveTypeNames(), 1);
    layerWaveBox.addItemList (SoraKickAudioProcessor::getWaveNames(), 1);
    addAndMakeVisible (characterBox);
    addAndMakeVisible (driveTypeBox);
    addAndMakeVisible (layerWaveBox);

    characterAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "character", characterBox);
    driveTypeAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "driveType", driveTypeBox);
    layerWaveAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "layerWave", layerWaveBox);

    // порядок = раскладка сетки 11 x 2
    addKnob ("attack",      "ATTACK");
    addKnob ("sustain",     "SUSTAIN");
    addKnob ("length",      "LENGTH");
    addKnob ("sub",         "SUB");
    addKnob ("click",       "CLICK");
    addKnob ("lowCut",      "LOW CUT");
    addKnob ("highCut",     "HIGH CUT");
    addKnob ("drive",       "DRIVE");
    addKnob ("bits",        "BITS");
    addKnob ("rate",        "RATE");
    addKnob ("screech",     "SCREECH");

    addKnob ("screechFreq", "SCREECH HZ");
    addKnob ("layer",       "SUB LAYER");
    addKnob ("layerFreq",   "LAYER HZ");
    addKnob ("layerDecay",  "LAYER DECAY");
    addKnob ("layerDrop",   "PITCH DROP");
    addKnob ("dropTime",    "DROP TIME");
    addKnob ("trigger",     "TRIGGER");
    addKnob ("rumble",      "RUMBLE");
    addKnob ("rumbleFreq",  "RUMBLE HZ");
    addKnob ("mix",         "MIX");
    addKnob ("output",      "OUTPUT");

    setSize (windowW, windowH);

    // сразу показываем фон текущего Character без плавного перехода
    shownScene = prevScene = juce::jmax (0, characterBox.getSelectedId() - 1) % (int) scenes.size();
    fade = 1.f;

    startTimerHz (30);
}

SoraKickAudioProcessorEditor::~SoraKickAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SoraKickAudioProcessorEditor::buildScenes()
{
    scenes.clear();
    for (int i = 0; i < numScenes; ++i)
    {
        auto img = loadAsset ("scene" + juce::String (i + 1) + "_png");
        scenes.push_back (img.isValid() ? img : renderScene (i, windowW, windowH));
    }
}

void SoraKickAudioProcessorEditor::addKnob (const char* paramID, const char* title)
{
    auto k = std::make_unique<Knob>();

    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 16);
    k->label.setText (title, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (vnFont (10.f, true));
    k->attachment = std::make_unique<APVTS::SliderAttachment> (proc.apvts, paramID, k->slider);

    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    knobs.push_back (std::move (k));
}

//==============================================================================
void SoraKickAudioProcessorEditor::timerCallback()
{
    // выбранный Character -> фон; при смене начинаем медленный переход (~3 секунды)
    const int idx = juce::jmax (0, characterBox.getSelectedId() - 1);
    const int scene = idx % (int) scenes.size();

    if (scene != shownScene)
    {
        prevScene = shownScene;
        shownScene = scene;
        fade = 0.f;
    }

    ++blinkTicks;

    if (fade < 1.f)
    {
        fade = juce::jmin (1.f, fade + 1.f / 90.f);
        repaint();
    }
    else
    {
        repaint (940, 640, 40, 30);   // только мигающий треугольник
    }
}

void SoraKickAudioProcessorEditor::paint (juce::Graphics& g)
{
    const int w = getWidth(), h = getHeight();

    // фон с плавной сменой
    auto& prev = scenes[(size_t) prevScene];
    g.setOpacity (1.f);
    g.drawImage (prev, 0, 0, w, h, 0, 0, prev.getWidth(), prev.getHeight());

    if (fade > 0.f && shownScene != prevScene)
    {
        auto& cur = scenes[(size_t) shownScene];
        g.setOpacity (fade);
        g.drawImage (cur, 0, 0, w, h, 0, 0, cur.getWidth(), cur.getHeight());
        g.setOpacity (1.f);
    }

    // спрайт героини
    if (sprite.isValid())
        g.drawImageWithin (sprite, 560, 0, 420, 700,
                           juce::RectanglePlacement::xMid | juce::RectanglePlacement::yBottom);
    else
        drawHeroine (g, { 620.f, 6.f, 340.f, 520.f });

    // заголовок
    g.setFont (vnFont (32.f, true));
    g.setColour (juce::Colour (0xff1d2238).withAlpha (0.6f));
    g.drawText ("ClannadKI", 26, 18, 400, 40, juce::Justification::centredLeft);
    g.setColour (juce::Colours::white);
    g.drawText ("ClannadKI", 24, 16, 400, 40, juce::Justification::centredLeft);

    // диалоговое окно
    const juce::Rectangle<float> box (20.f, 340.f, 960.f, 340.f);
    juce::ColourGradient boxGrad (juce::Colour (0xe61f2542), 0.f, box.getY(),
                                  juce::Colour (0xe6161b33), 0.f, box.getBottom(), false);
    g.setGradientFill (boxGrad);
    g.fillRoundedRectangle (box, 10.f);
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.drawRoundedRectangle (box.reduced (0.75f), 10.f, 1.5f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (box.reduced (5.f), 7.f, 1.f);

    // табличка с именем (как у говорящего персонажа)
    const juce::Rectangle<float> nameplate (36.f, 322.f, 210.f, 30.f);
    g.setColour (juce::Colour (0xf01b2038));
    g.fillRoundedRectangle (nameplate, 4.f);
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.drawRoundedRectangle (nameplate.reduced (0.5f), 4.f, 1.f);
    g.setColour (juce::Colours::white);
    g.setFont (vnFont (14.f, true));
    auto name = characterBox.getText();
    g.drawText (name.isEmpty() ? juce::String ("ClannadKI") : name,
                nameplate.toNearestInt(), juce::Justification::centred);

    // подписи к выпадающим спискам
    g.setColour (juce::Colours::white.withAlpha (0.75f));
    g.setFont (vnFont (10.f, true));
    g.drawText ("CHARACTER",  40,  354, 200, 12, juce::Justification::centredLeft);
    g.drawText ("DRIVE TYPE", 290, 354, 150, 12, juce::Justification::centredLeft);
    g.drawText ("LAYER WAVE", 460, 354, 150, 12, juce::Justification::centredLeft);

    // мигающий треугольник "дальше" в углу окна
    juce::Path tri;
    tri.addTriangle (952.f, 648.f, 968.f, 648.f, 960.f, 660.f);
    const float blink = 0.35f + 0.5f * (0.5f + 0.5f * std::sin ((float) blinkTicks * 0.2f));
    g.setColour (juce::Colours::white.withAlpha (blink));
    g.fillPath (tri);
}

void SoraKickAudioProcessorEditor::resized()
{
    characterBox.setBounds (40, 368, 230, 26);
    driveTypeBox.setBounds (290, 368, 150, 26);
    layerWaveBox.setBounds (460, 368, 150, 26);

    const juce::Rectangle<int> area (36, 402, 900, 268);
    const int cols = 11, rows = 2;
    const int cw = area.getWidth() / cols;
    const int ch = area.getHeight() / rows;

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int row = (int) i / cols;
        const int col = (int) i % cols;
        auto cell = juce::Rectangle<int> (area.getX() + col * cw, area.getY() + row * ch, cw, ch).reduced (2);
        knobs[i]->label.setBounds (cell.removeFromTop (14));
        knobs[i]->slider.setBounds (cell);
    }
}
