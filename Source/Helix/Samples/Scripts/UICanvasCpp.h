#pragma once

#include <Helix/Core/Strings/Format.h>
// The engine's script API before the canvas UI's (its classes derive from the engine's).
#include <HelixEngine/HelixEngineApi.h>
#include <HelixEngine.UI/HelixUIApi.h>
#include <HelixEngine.CanvasUI/HelixCanvasUIApi.h>

// The canvas UI sample's C++ script: a panel of controls made through the C++ script API (the default controls of the
// canvas UI, a button, a toggle and a slider), heard by their events, each change shown in the panel's own line. It
// waits for the C# script's HUD canvas and builds its panel there.
namespace helix::samples
{
    HX_CLASS(Namespace = "HelixSamples")
    class UICanvasCpp : public ::HelixEngine::GameBehaviour
    {
    public:
        void Update()
        {
            if (built_)
                return;
            namespace api = ::HelixEngine;
            namespace ui = ::HelixEngine::CanvasUI;
            api::GameObject hud = api::GameObject::Find("HUD");
            if (!hud)
                return;
            built_ = true;
            ::helix::canvasui::DefaultControlsResourcesData none{};

            // The panel: in the bottom right corner of the HUD.
            api::GameObject panel = ui::DefaultControls::CreatePanelInternal(none);
            panel.name = "C++ Panel";
            Attach(panel, hud, Vector2(1, 0), Vector2(1, 0), Vector2(-420, 56), Vector2(-20, 236));
            ui::Image back = panel.GetComponent<ui::Image>();
            back.color = Color(0.1f, 0.12f, 0.17f, 0.92f);

            api::GameObject title = ui::TextDefaultControls::CreateTextInternal(none);
            Attach(title, panel, Vector2(0, 1), Vector2(1, 1), Vector2(14, -40), Vector2(-14, -8));
            ::HelixEngine::Text::TextBase titleText = title.GetComponent<ui::CanvasText>();
            titleText.text = "Made by a C++ script";
            titleText.fontSize = 22.0f;
            titleText.color = Color(1, 0.72f, 0.28f, 1);

            api::GameObject line = ui::TextDefaultControls::CreateTextInternal(none);
            Attach(line, panel, Vector2(0, 0), Vector2(1, 0), Vector2(14, 8), Vector2(-14, 34));
            line_ = line.GetComponent<ui::CanvasText>();
            line_.fontSize = 18.0f;
            line_.color = Color(0.62f, 0.68f, 0.78f, 1);
            line_.text = "C++: waiting for a change";

            api::GameObject button = ui::TextDefaultControls::CreateButtonInternal(none);
            Attach(button, panel, Vector2(0, 1), Vector2(0, 1), Vector2(14, -84), Vector2(174, -48));
            ::HelixEngine::Text::TextBase buttonText = button.transform.Get().GetChild(0).gameObject.Get().GetComponent<ui::CanvasText>();
            buttonText.text = "C++ button";
            buttonText.fontSize = 18.0f;
            button_ = button.GetComponent<ui::Button>();
            clickToken_ = button_.onClickNative.Add({[](void* self) {
                                     auto* s = static_cast<UICanvasCpp*>(self);
                                     s->clicks_++;
                                     s->Show(Format("C++: the button, {} clicks", s->clicks_));
                                 },
                                 this});

            api::GameObject toggle = ui::DefaultControls::CreateToggleInternal(none);
            Attach(toggle, panel, Vector2(0, 1), Vector2(1, 1), Vector2(190, -84), Vector2(-14, -48));
            ::HelixEngine::Text::TextBase toggleText = toggle.transform.Get().GetChild(1).gameObject.Get().GetComponent<ui::CanvasText>();
            toggleText.text = "Night mode";
            toggleText.color = Color(0.92f, 0.94f, 0.98f, 1);
            toggle_ = toggle.GetComponent<ui::Toggle>();
            toggleToken_ = toggle_.onValueChangedNative.Add({[](void* self, bool on) { static_cast<UICanvasCpp*>(self)->Show(Format("C++: night mode {}", on ? "on" : "off")); }, this});

            api::GameObject slider = ui::DefaultControls::CreateSliderInternal(none);
            Attach(slider, panel, Vector2(0, 1), Vector2(1, 1), Vector2(14, -126), Vector2(-14, -104));
            slider_ = slider.GetComponent<ui::Slider>();
            slider_.minValue = 0.0f;
            slider_.maxValue = 10.0f;
            slider_.wholeNumbers = true;
            slider_.value = 4.0f;
            sliderToken_ = slider_.onValueChangedNative.Add({[](void* self, f32 v) { static_cast<UICanvasCpp*>(self)->Show(Format("C++: speed {}", i32(v))); }, this});
        }

        // The handlers are the script's: removed as it goes (quietly when a control went first).
        void OnDestroy()
        {
            button_.onClickNative.Remove(clickToken_);
            toggle_.onValueChangedNative.Remove(toggleToken_);
            slider_.onValueChangedNative.Remove(sliderToken_);
            clickToken_ = toggleToken_ = sliderToken_ = 0;
        }

    private:
        // Under the parent, placed by its anchors and offsets.
        static void Attach(const ::HelixEngine::GameObject& child, const ::HelixEngine::GameObject& parent, Vector2 anchorMin, Vector2 anchorMax,
                           Vector2 offsetMin, Vector2 offsetMax)
        {
            ::HelixEngine::Transform t = child.transform;
            t.SetParent(parent.transform, false);
            auto r = ::HelixEngine::RectTransform::HxFromId(t.HxId());
            r.anchorMin = anchorMin;
            r.anchorMax = anchorMax;
            r.offsetMin = offsetMin;
            r.offsetMax = offsetMax;
        }

        void Show(const String& text)
        {
            if (line_)
                line_.text = text.View();
        }

        ::HelixEngine::Text::TextBase line_;
        ::HelixEngine::CanvasUI::Button button_;
        ::HelixEngine::CanvasUI::Toggle toggle_;
        ::HelixEngine::CanvasUI::Slider slider_;
        u64 clickToken_ = 0, toggleToken_ = 0, sliderToken_ = 0;
        i32 clicks_ = 0;
        bool built_ = false;
    };
}
