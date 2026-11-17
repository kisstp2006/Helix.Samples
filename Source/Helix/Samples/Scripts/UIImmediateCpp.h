#pragma once

#include <Helix/Core/Strings/Format.h>
// The engine's script API before the UI's (the UI's classes derive from the engine's).
#include <HelixEngine/HelixEngineApi.h>
#include <HelixEngine.UI/HelixUIApi.h>

#include <cmath>

// The immediate GUI sample's C++ script: OnGui through the C++ script API, a laid out area of its own (a counter
// button, a toggle, a hue slider tinting a swatch) beside the C# script's windows.
namespace helix::samples
{
    HX_CLASS(Namespace = "HelixSamples")
    class UIImmediateCpp : public ::HelixEngine::GameBehaviour
    {
    public:
        void OnGui()
        {
            namespace api = ::HelixEngine;
            api::GuiLayout::BeginArea(::helix::Rect(668, 300, 276, 140), "C++ OnGui", api::Gui::skin.Get().window);
            if (api::GuiLayout::Button(Format("Clicked {} times", clicks_).View()))
                clicks_++;
            pulse_ = api::GuiLayout::Toggle(pulse_, "Pulse the swatch (not in a batch run)");
            api::GuiLayout::BeginHorizontal();
            api::GuiLayout::Label(Format("Hue {:.2f}", f64(hue_)).View());
            hue_ = api::GuiLayout::HorizontalSlider(hue_, 0.0f, 1.0f);
            api::GuiLayout::EndHorizontal();
            api::GuiLayout::EndArea();
            // The swatch: tinted by the hue (and pulsing), in the area's title bar.
            f32 v = pulse_ && !api::Application::isBatchMode ? 0.75f + 0.25f * std::sin(f32(api::Time::realtimeSinceStartup) * 4.0f) : 1.0f;
            ::helix::Color saved = api::Gui::backgroundColor;
            api::Gui::backgroundColor = Hue(hue_, v);
            // The button's look tinted (the box's is dark).
            api::Gui::Box(::helix::Rect(904, 304, 32, 18), "", api::Gui::skin.Get().button);
            api::Gui::backgroundColor = saved;
        }

    private:
        static ::helix::Color Hue(f32 h, f32 v)
        {
            f32 r = std::fabs(h * 6 - 3) - 1, g = 2 - std::fabs(h * 6 - 2), b = 2 - std::fabs(h * 6 - 4);
            auto c = [&](f32 x) { return (x < 0 ? 0 : x > 1 ? 1 : x) * v; };
            return ::helix::Color(c(r) * 2, c(g) * 2, c(b) * 2, 1);
        }

        i32 clicks_ = 0;
        bool pulse_ = true;
        f32 hue_ = 0.55f;
    };
}
