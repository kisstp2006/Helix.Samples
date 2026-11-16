#pragma once

#include <Helix/Core/Strings/Format.h>
// The engine's script API before the UI's (the UI's classes derive from the engine's).
#include <HelixEngine/HelixEngineApi.h>
#include <HelixEngine.UI/HelixUIApi.h>

// The samples' element type of a C++ class: <Gauge value="0.6" tint="#3fd0a8"/>, a bar filled to its value. Made over
// its element (its handle is the element in its constructor), its properties set from markup through their setters;
// its parts are elements it makes itself, styled by the sample's sheets (.gauge, .gauge-track, .gauge-fill).
namespace helix::samples
{
    HX_CLASS(Namespace = "HelixSamples", HxmlElement)
    class Gauge : public ::HelixEngine::UI::Element
    {
    public:
        Gauge()
        {
            using Element = ::HelixEngine::UI::Element;
            AddToClassList("gauge");
            Element track = Element::New();
            track.AddToClassList("gauge-track");
            fill_ = Element::New();
            fill_.AddToClassList("gauge-fill");
            track.Add(fill_);
            Add(track);
            Refresh();
        }

        // How full it is (0 to 1).
        HX_PROPERTY(HxmlAttribute) f32 value() const { return value_; }
        void SetValue(f32 v)
        {
            value_ = v < 0 ? 0 : v > 1 ? 1 : v;
            Refresh();
        }

        // The fill's color.
        HX_PROPERTY(HxmlAttribute) Color tint() const { return tint_; }
        void SetTint(Color c)
        {
            tint_ = c;
            Refresh();
        }

    private:
        void Refresh()
        {
            fill_.SetInlineStyleText("width", Format("{}%", value_ * 100.0f));
            fill_.SetInlineStyleText("background-color", Format("rgba({}, {}, {}, {})", tint_.r * 255.0f, tint_.g * 255.0f, tint_.b * 255.0f, tint_.a));
        }

        ::HelixEngine::UI::Element fill_;
        f32 value_ = 0.5f;
        Color tint_ = Color(0.31f, 0.62f, 1.0f, 1.0f);
    };
}
