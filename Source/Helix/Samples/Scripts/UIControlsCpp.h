#pragma once

#include <Helix/Core/Strings/Format.h>
// The engine's script API before the UI's (the UI's classes derive from the engine's).
#include <HelixEngine/HelixEngineApi.h>
#include <HelixEngine.UI/HelixUIApi.h>

// The controls sample's C++ script: a group of controls made through the C++ script API (their values typed, their
// events heard), each change shown in the page's status line beside the C# script's.
namespace helix::samples
{
    HX_CLASS(Namespace = "HelixSamples")
    class UIControlsCpp : public ::HelixEngine::GameBehaviour
    {
    public:
        void Start()
        {
            namespace api = ::HelixEngine::UI;
            api::UIView view = GetComponent<api::UIView>();
            api::Element root = view.rootElement;
            api::Element group = root.Find("from-cpp");
            api::Element status = root.Find("status");
            if (!group || !status)
                return;
            status_ = api::Label::HxFromId(status.HxId());

            slider_ = api::Slider::New();
            slider_.label = "Speed";
            slider_.value = 4.0f;
            group.Add(slider_);
            slider_.valueChanged.Add({[](void* self) { static_cast<UIControlsCpp*>(self)->Show(Format("C++ slider: {:.1f}", f64(f32(static_cast<UIControlsCpp*>(self)->slider_.value)))); }, this});

            toggle_ = api::Toggle::New();
            toggle_.label = "Enabled";
            toggle_.text = "From C++";
            toggle_.value = true;
            group.Add(toggle_);
            toggle_.valueChanged.Add({[](void* self) { static_cast<UIControlsCpp*>(self)->Show(Format("C++ toggle: {}", bool(static_cast<UIControlsCpp*>(self)->toggle_.value))); }, this});

            count_ = api::IntegerField::New();
            count_.label = "Count";
            count_.value = 3;
            group.Add(count_);
            count_.valueChanged.Add({[](void* self) { static_cast<UIControlsCpp*>(self)->Show(Format("C++ count: {}", i32(static_cast<UIControlsCpp*>(self)->count_.value))); }, this});

            api::Button button = api::Button::New();
            button.text = "C++ button";
            group.Add(button);
            button.clicked.Add({[](void* self) {
                                    auto* s = static_cast<UIControlsCpp*>(self);
                                    s->clicks_++;
                                    s->count_.value = s->clicks_;
                                    s->Show(Format("C++ button {}x", s->clicks_));
                                },
                                this});
        }

    private:
        void Show(const String& text)
        {
            if (status_)
                status_.text = text;
        }

        ::HelixEngine::UI::Label status_;
        ::HelixEngine::UI::Slider slider_;
        ::HelixEngine::UI::Toggle toggle_;
        ::HelixEngine::UI::IntegerField count_;
        i32 clicks_ = 0;
    };
}
