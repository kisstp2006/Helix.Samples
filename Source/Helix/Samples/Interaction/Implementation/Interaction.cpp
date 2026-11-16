#include <Helix/Samples/Interaction/Interaction.h>

#include <Helix/Core/Containers/Array.h>
#include <Helix/Core/Strings/Format.h>
#include <Helix/UI/Events/EventSystem.h>
#include <Helix/UI/Layout/Layout.h>
#include <Helix/UI/Markup/Hxml.h>

#include <initializer_list>

namespace helix::samples
{
    namespace
    {
        using ui::ElementHandle;
        using ui::ElementTree;
        using ui::EventSystem;
        using ui::UIEvent;
        using ui::Atom;
        using ui::AtomName;
        using ui::Intern;
        using ui::kNoAtom;

        // Per element state lives by its tree and handle; it is touched only from that tree's own events.
        template <class T>
        struct PerElement
        {
            struct Entry
            {
                const ElementTree* tree = nullptr;
                ElementHandle element;
                T value;
            };
            Array<Entry> entries;

            T& Of(const ElementTree& tree, ElementHandle e)
            {
                for (usize i = 0; i < entries.Size();)
                    if (entries[i].tree == &tree && !tree.IsAlive(entries[i].element))
                        entries.RemoveAt(i);
                    else
                        i++;
                for (Entry& x : entries)
                    if (x.tree == &tree && x.element == e)
                        return x.value;
                entries.Push(Entry{&tree, e, T()});
                return entries.Back().value;
            }
        };

        void AppendUtf8(String& out, u32 cp)
        {
            char b[4];
            u32 n = 0;
            if (cp < 0x80)
                b[n++] = char(cp);
            else if (cp < 0x800)
                b[n++] = char(0xC0 | (cp >> 6)), b[n++] = char(0x80 | (cp & 0x3F));
            else if (cp < 0x10000)
                b[n++] = char(0xE0 | (cp >> 12)), b[n++] = char(0x80 | ((cp >> 6) & 0x3F)), b[n++] = char(0x80 | (cp & 0x3F));
            else
                b[n++] = char(0xF0 | (cp >> 18)), b[n++] = char(0x80 | ((cp >> 12) & 0x3F)), b[n++] = char(0x80 | ((cp >> 6) & 0x3F)),
                b[n++] = char(0x80 | (cp & 0x3F));
            out += StringView(b, n);
        }

        // An element's name for the log (its type when it has none).
        String Label(const ElementTree& tree, ElementHandle e)
        {
            Atom name = tree.Name(e);
            return String(AtomName(name != kNoAtom ? name : tree.Type(e)));
        }

        // ---------------------------------------------------------------------------------------------------- log

        constexpr usize kLogLines = 13;

        struct Log
        {
            Array<String> lines;
            // The elements that show it, by tree.
            PerElement<bool> sinks;
        };

        Log& TheLog()
        {
            static Log* log = new Log();
            return *log;
        }

        void ShowLog(ElementTree& tree, ElementHandle e)
        {
            String text;
            for (const String& line : TheLog().lines)
            {
                if (!text.Empty())
                    text += "\n";
                text += line.View();
            }
            tree.SetText(e, text.Empty() ? StringView("<color=#8892a6>Events show here in their order.</color>") : text.View());
        }

        void Write(EventSystem& events, String line)
        {
            Log& log = TheLog();
            log.lines.Push(Move(line));
            while (log.lines.Size() > kLogLines)
                log.lines.RemoveAt(0);
            ElementTree& tree = events.tree();
            for (auto& s : log.sinks.entries)
                if (s.tree == &tree && tree.IsAlive(s.element))
                    ShowLog(tree, s.element);
        }

        void AttachLog(EventSystem& events, ElementHandle e)
        {
            TheLog().sinks.Of(events.tree(), e) = true;
            ShowLog(events.tree(), e);
        }

        // The events a probe logs as they reach it.
        void Probe(void*, EventSystem& events, UIEvent& e)
        {
            StringView type = AtomName(ui::EventTypeOf(e.type).name);
            String line(type.Substr(0, type.Size() > 5 ? type.Size() - 5 : type.Size()));
            line += "  ";
            line += Label(events.tree(), e.target).View();
            if (e.character)
            {
                line += "  '";
                AppendUtf8(line, e.character);
                line += "'";
            }
            else if (e.keyCode)
                line += Format("  key {}", u32(e.keyCode)).View();
            if (e.relatedTarget.IsValid())
                line += Format("  (from {})", Label(events.tree(), e.relatedTarget).View()).View();
            if (e.clickCount > 1 && e.Is(ui::Events().click))
                line += Format("  x{}", e.clickCount).View();
            Write(events, Move(line));
        }

        void AttachProbe(EventSystem& events, ElementHandle e)
        {
            const ui::EventTypes& t = ui::Events();
            for (ui::EventTypeId type : {t.pointerDown, t.pointerUp, t.click, t.wheel, t.focusIn, t.keyDown, t.keyUp, t.navigationMove,
                                         t.navigationSubmit, t.navigationCancel, t.pointerCapture, t.pointerCaptureOut, t.composition, t.dragPerform})
                events.RegisterCallback(e, type, &Probe, nullptr);
        }

        // ------------------------------------------------------------------------------------------------- button

        struct ButtonState
        {
            String text;
            u32 presses = 0;
        };
        PerElement<ButtonState>& Buttons()
        {
            static auto* b = new PerElement<ButtonState>();
            return *b;
        }

        void ShowButton(ElementTree& tree, ElementHandle e)
        {
            ButtonState& s = Buttons().Of(tree, e);
            tree.SetText(e, s.presses ? Format("{} ({})", s.text.View(), s.presses).View() : s.text.View());
        }

        void SetActive(ElementTree& tree, ElementHandle e, bool on)
        {
            tree.SetStates(e, on ? tree.States(e) | ui::PseudoStates::Active : tree.States(e) & ~ui::PseudoStates::Active);
        }

        void AttachButton(EventSystem& events, ElementHandle e)
        {
            const ui::EventTypes& t = ui::Events();
            events.tree().SetFocusable(e, true);
            ShowButton(events.tree(), e);
            events.RegisterCallback(e, t.pointerDown, [](void*, EventSystem& ev, UIEvent& x) {
                if (x.button != 0)
                    return;
                ev.CapturePointer(x.currentTarget, x.pointerId);
                SetActive(ev.tree(), x.currentTarget, true);
            }, nullptr);
            events.RegisterCallback(e, t.pointerUp, [](void*, EventSystem& ev, UIEvent& x) {
                if (ev.HasPointerCapture(x.currentTarget, x.pointerId))
                    ev.ReleasePointer(x.currentTarget, x.pointerId);
            }, nullptr);
            events.RegisterCallback(e, t.pointerCaptureOut, [](void*, EventSystem& ev, UIEvent& x) {
                if (x.target == x.currentTarget)
                    SetActive(ev.tree(), x.currentTarget, false);
            }, nullptr);
            auto press = [](void*, EventSystem& ev, UIEvent& x) {
                Buttons().Of(ev.tree(), x.currentTarget).presses++;
                ShowButton(ev.tree(), x.currentTarget);
            };
            events.RegisterCallback(e, t.click, press, nullptr);
            events.RegisterCallback(e, t.navigationSubmit, press, nullptr);
        }

        bool TextAttribute(ElementTree& tree, ElementHandle e, StringView name, StringView value)
        {
            if (name != StringView("text"))
                return false;
            if (tree.Type(e) == Intern("SampleButton"))
                Buttons().Of(tree, e).text = String(value);
            tree.SetText(e, value);
            return true;
        }

        // ----------------------------------------------------------------------------------------------- text box

        struct TextState
        {
            Array<u32> text;
        };
        PerElement<TextState>& Texts()
        {
            static auto* t = new PerElement<TextState>();
            return *t;
        }

        void ShowText(EventSystem& events, ElementHandle e)
        {
            ElementTree& tree = events.tree();
            const TextState& s = Texts().Of(tree, e);
            bool focused = events.Focused() == e;
            Span<const u32> composing = focused ? events.Composition() : Span<const u32>();
            if (s.text.Empty() && composing.Empty() && !focused)
            {
                tree.SetText(e, "<color=#8892a6>Click and type here</color>");
                return;
            }
            String out("<noparse>");
            for (u32 c : s.text)
                AppendUtf8(out, c);
            out += "</noparse>";
            if (!composing.Empty())
            {
                out += "<u><color=#ffd54a><noparse>";
                for (u32 c : composing)
                    AppendUtf8(out, c);
                out += "</noparse></color></u>";
            }
            if (focused)
                out += "<color=#4f9dff>|</color>";
            tree.SetText(e, out.View());
        }

        void AttachTextBox(EventSystem& events, ElementHandle e)
        {
            const ui::EventTypes& t = ui::Events();
            events.tree().SetFocusable(e, true);
            ShowText(events, e);
            events.RegisterCallback(e, t.keyDown, [](void*, EventSystem& ev, UIEvent& x) {
                TextState& s = Texts().Of(ev.tree(), x.currentTarget);
                if (x.character >= 32 && x.character != 127)
                    s.text.Push(x.character);
                else if (x.keyCode == 8 && !s.text.Empty())
                    s.text.Pop();
                else
                    return;
                ShowText(ev, x.currentTarget);
            }, nullptr);
            // The arrows along the line belong to it.
            events.RegisterCallback(e, t.navigationMove, [](void*, EventSystem&, UIEvent& x) {
                if (x.direction == ui::NavigationDirection::Left || x.direction == ui::NavigationDirection::Right)
                    x.StopPropagation();
            }, nullptr);
            events.RegisterCallback(e, t.focus, [](void*, EventSystem& ev, UIEvent& x) {
                // The input method's candidates open under the box.
                Rect r = ev.layout().WorldRect(x.currentTarget);
                ev.SetTextInput(x.currentTarget, Rect(6, 4, 1, r.height - 8));
                ShowText(ev, x.currentTarget);
            }, nullptr);
            auto refresh = [](void*, EventSystem& ev, UIEvent& x) { ShowText(ev, x.currentTarget); };
            events.RegisterCallback(e, t.blur, refresh, nullptr);
            events.RegisterCallback(e, t.composition, refresh, nullptr);
        }

        // ---------------------------------------------------------------------------------------- drag and drop

        Atom ItemKind() { return Intern("SampleItem"); }

        // What is dragged: one item at a time.
        struct Dragged
        {
            ElementHandle item;
        };
        Dragged& TheDragged()
        {
            static Dragged d;
            return d;
        }

        void AttachDragItem(EventSystem& events, ElementHandle e)
        {
            const ui::EventTypes& t = ui::Events();
            events.RegisterCallback(e, t.pointerDown, [](void*, EventSystem& ev, UIEvent& x) {
                if (x.button != 0)
                    return;
                TheDragged().item = x.currentTarget;
                ev.tree().AddClass(x.currentTarget, Intern("dragging"));
                // Its own release ends its look; the drop goes to what is under the pointer.
                ev.CapturePointer(x.currentTarget, x.pointerId);
                ev.StartDrag(x.pointerId, ItemKind(), &TheDragged());
            }, nullptr);
            events.RegisterCallback(e, t.pointerUp, [](void*, EventSystem& ev, UIEvent& x) {
                ev.tree().RemoveClass(x.currentTarget, Intern("dragging"));
                if (ev.HasPointerCapture(x.currentTarget, x.pointerId))
                    ev.ReleasePointer(x.currentTarget, x.pointerId);
            }, nullptr);
        }

        void AttachDropList(EventSystem& events, ElementHandle e)
        {
            const ui::EventTypes& t = ui::Events();
            events.RegisterCallback(e, t.dragEnter, [](void*, EventSystem& ev, UIEvent& x) {
                if (x.target == x.currentTarget)
                    ev.tree().AddClass(x.currentTarget, Intern("drop-target"));
            }, nullptr);
            auto leave = [](void*, EventSystem& ev, UIEvent& x) {
                if (x.target == x.currentTarget || x.Is(ui::Events().dragExited) || x.Is(ui::Events().dragPerform))
                    ev.tree().RemoveClass(x.currentTarget, Intern("drop-target"));
            };
            events.RegisterCallback(e, t.dragLeave, leave, nullptr);
            events.RegisterCallback(e, t.dragExited, leave, nullptr);
            events.RegisterCallback(e, t.dragUpdated, [](void*, EventSystem& ev, UIEvent&) {
                if (ev.DragKind() == ItemKind())
                    ev.SetVisualMode(ui::DragVisualMode::Move);
            }, nullptr);
            events.RegisterCallback(e, t.dragPerform, [](void*, EventSystem& ev, UIEvent& x) {
                auto* d = static_cast<Dragged*>(ev.DragData());
                ElementTree& tree = ev.tree();
                if (d && tree.IsAlive(d->item) && d->item != x.currentTarget && !tree.IsAncestor(d->item, x.currentTarget))
                    tree.Append(x.currentTarget, d->item);
                tree.RemoveClass(x.currentTarget, Intern("drop-target"));
            }, nullptr);
        }
    }

    void RegisterSampleInteraction()
    {
        ui::RegisterElementType("SampleButton", &TextAttribute, nullptr, &AttachButton);
        ui::RegisterElementType("SampleTextBox", nullptr, nullptr, &AttachTextBox);
        ui::RegisterElementType("SampleDragItem", &TextAttribute, nullptr, &AttachDragItem);
        ui::RegisterElementType("SampleDropList", nullptr, nullptr, &AttachDropList);
        ui::RegisterElementType("SampleProbe", nullptr, nullptr, &AttachProbe);
        ui::RegisterElementType("SampleEventLog", nullptr, nullptr, &AttachLog);
    }
}
