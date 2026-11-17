#include <Helix/Samples/Graph/GraphView.h>

#include <Helix/Samples/Graph/GraphModel.h>

#include <Helix/Core/Containers/Array.h>
#include <Helix/Core/Strings/Format.h>
#include <Helix/Engine/Application/Application.h>
#include <Helix/Engine/Texture/Texture.h>
#include <Helix/Engine/Time/Time.h>
#include <Helix/UI/Engine/Element.h>
#include <Helix/UI/Events/EventSystem.h>
#include <Helix/UI/Layout/Layout.h>
#include <Helix/UI/Markup/Hxml.h>
#include <Helix/UI/Rendering/Painter.h>
#include <Helix/UI/Styles/StyleEngine.h>
#include <Helix/UI/World/ElementWorld.h>

#include <cmath>
#include <cstdlib>

namespace helix::samples
{
    namespace
    {
        namespace g = graph;
        using ui::Atom;
        using ui::ElementHandle;
        using ui::ElementTree;
        using ui::EventSystem;
        using ui::Intern;
        using ui::Painter;
        using ui::UIEvent;

        constexpr u16 kKeyBackspace = 8, kKeySpace = 32, kKeyEscape = 27, kKeyA = 97, kKeyF = 102, kKeyDelete = 127;
        constexpr u8 kShift = 1, kControl = 2, kAlt = 4;
        const Color kSelection(0.267f, 0.753f, 1.0f, 1);

        ui::ElementWorld& World() { return ui::ElementWorld::Main(); }
        Rect RectOf(ElementHandle e) { return World().layout().WorldRect(e); }
        Vector2 Center(const Rect& r) { return Vector2(r.x + r.width * 0.5f, r.y + r.height * 0.5f); }

        String Hex(Color c)
        {
            auto byte = [](f32 v) { return u32(std::lround((v < 0 ? 0 : v > 1 ? 1 : v) * 255)); };
            const char* digits = "0123456789abcdef";
            String s("#");
            for (u32 b : {byte(c.r), byte(c.g), byte(c.b)})
            {
                s += StringView(digits + (b >> 4), 1);
                s += StringView(digits + (b & 15), 1);
            }
            return s;
        }

        String Number(f32 v)
        {
            if (std::fabs(v - std::round(v)) < 1e-4f)
                return Format("{}", i32(std::lround(v)));
            return Format("{:.2f}", f64(v));
        }

        f32 Clamp(f32 v, f32 a, f32 b) { return v < a ? a : v > b ? b : v; }


        // ------------------------------------------------------------------------------------------- the view

        // What an element of the graph is for, found from an event's target up.
        enum class Role : u8
        {
            Node,
            Port,
            Inline,
            PreviewToggle,
            Menu,
            MenuItem
        };

        struct Hit
        {
            ElementHandle element;
            Role role = Role::Node;
            u32 node = 0, port = 0, component = 0;
            bool output = false;
            i32 item = -1;
        };

        struct NodeElements
        {
            u32 id = 0;
            ElementHandle root;
            Array<ElementHandle> inputs, outputs;
        };

        enum class Drag : u8
        {
            None,
            Pan,
            Move,
            Band,
            Edge,
            Scrub
        };

        struct View
        {
            ElementHandle element, content, overlay, menu;
            Atom inspector = ui::kNoAtom, status = ui::kNoAtom, nodeTab = ui::kNoAtom, graphTab = ui::kNoAtom;
            // The room panels over the view take (left, top, right, bottom): framing keeps the graph out of it.
            f32 insets[4] = {0, 0, 0, 0};
            Vector2 pan = Vector2(40, 40);
            f32 zoom = 1;
            bool framed = false;
            Array<NodeElements> nodes;
            Array<Hit> hits;
            Array<u32> selected;
            i32 selectedEdge = -1;
            Drag drag = Drag::None;
            Vector2 pressAt, at;
            Array<Vector2> moveStart;
            bool moved = false;
            // An edge being drawn: from this port (an output, or an input dragged backwards) to the pointer.
            u32 edgeNode = 0, edgePort = 0;
            bool edgeFromOutput = true;
            // An inline value being scrubbed.
            u32 scrubNode = 0, scrubInput = 0, scrubComponent = 0;
            f32 scrubStart = 0;
            ElementHandle scrubElement;
            // The create node menu: where it adds (graph space), the category open (-1: the list of categories).
            Vector2 menuAt, menuLocal;
            i32 menuCategory = -1;
        };

        Array<View*>& Views()
        {
            static Array<View*>* views = new Array<View*>();
            return *views;
        }

        View* ViewOf(ElementHandle e)
        {
            for (View* v : Views())
                if (v->element == e || v->overlay == e)
                    return v;
            return nullptr;
        }

        // A preview: an image the graph's result is written into (its element's background), written again when
        // the graph changes or its time moves on (at most 30 times a second).
        struct PreviewOf
        {
            ElementHandle element;
            u32 node = 0;
            bool main = false;
            engine::PPtr<engine::Texture2D> texture;
            Array<Color32> pixels;
            u32 version = ~0u;
            f32 time = -1;
        };

        Array<PreviewOf>& Previews()
        {
            static Array<PreviewOf>* previews = new Array<PreviewOf>();
            return *previews;
        }

        constexpr i32 kNodePreviewSize = 56;
        constexpr i32 kMainPreviewWidth = 160, kMainPreviewHeight = 122;

        // When the node previews were written last (all of them in one pass of the graph).
        struct NodePreviewState
        {
            u32 version = ~0u;
            f32 time = -1;
        };
        NodePreviewState& NodePreviews()
        {
            static NodePreviewState* state = new NodePreviewState();
            return *state;
        }

        PreviewOf* PreviewEntry(ElementHandle e)
        {
            for (PreviewOf& x : Previews())
                if (x.element == e)
                    return &x;
            return nullptr;
        }

        // The preview's image made and set as its element's background.
        void BindPreview(PreviewOf& entry)
        {
            i32 w = entry.main ? kMainPreviewWidth : kNodePreviewSize, h = entry.main ? kMainPreviewHeight : kNodePreviewSize;
            engine::Texture2D* t = engine::Texture2D::Create(w, h, engine::TextureFormat::RGBA32, false, true);
            t->hideFlags = engine::HideFlags::DontSave;
            t->SetFilterMode(engine::FilterMode::Bilinear);
            t->SetWrapMode(engine::TextureWrapMode::Clamp);
            entry.texture = engine::PPtr<engine::Texture2D>(t);
            entry.pixels.Resize(usize(w) * usize(h));
            if (ui::Element* x = ui::Element::Of(World(), entry.element))
                x->SetInlineStyleTexture("background-image", t, Vector2(0, 0));
        }

        Vector2 LocalOf(const View& v, Vector2 panel)
        {
            Rect r = RectOf(v.element);
            return panel - Vector2(r.x, r.y);
        }

        Vector2 ToGraph(const View& v, Vector2 local) { return (local - v.pan) / v.zoom; }

        // An element of the content (laid out without the view's transform) in the view's local space.
        Vector2 ContentToLocal(const View& v, Vector2 content)
        {
            Rect c = RectOf(v.content), r = RectOf(v.element);
            return Vector2(c.x - r.x, c.y - r.y) + v.pan + (content - Vector2(c.x, c.y)) * v.zoom;
        }

        Vector2 LocalToContent(const View& v, Vector2 local)
        {
            Rect c = RectOf(v.content), r = RectOf(v.element);
            return Vector2(c.x, c.y) + (local - Vector2(c.x - r.x, c.y - r.y) - v.pan) / v.zoom;
        }

        Vector2 ConnectorAt(const View& v, ElementHandle connector) { return ContentToLocal(v, Center(RectOf(connector))); }

        const NodeElements* ElementsOf(const View& v, u32 node)
        {
            for (const NodeElements& n : v.nodes)
                if (n.id == node)
                    return &n;
            return nullptr;
        }

        bool IsSelected(const View& v, u32 node)
        {
            for (u32 id : v.selected)
                if (id == node)
                    return true;
            return false;
        }

        // ------------------------------------------------------------------------------------------- building

        ElementHandle Make(ElementTree& tree, ElementHandle parent, StringView classes, StringView text = StringView())
        {
            ElementHandle e = tree.Create(Intern("Element"));
            usize at = 0;
            while (at < classes.Size())
            {
                usize end = classes.Find(' ', at);
                if (end == StringView::npos)
                    end = classes.Size();
                if (end > at)
                    tree.AddClass(e, Intern(classes.Substr(at, end - at)));
                at = end + 1;
            }
            if (!text.Empty())
                tree.SetText(e, text);
            tree.Append(parent, e);
            return e;
        }

        void Style(ElementHandle e, StringView declarations) { World().styles().SetInlineStyle(e, declarations); }

        void Clear(ElementTree& tree, ElementHandle e)
        {
            while (tree.FirstChild(e).IsValid())
                tree.Destroy(tree.FirstChild(e));
        }

        void AddHit(View& v, ElementHandle e, Role role, u32 node = 0, u32 port = 0, bool output = false, u32 component = 0, i32 item = -1)
        {
            Hit h;
            h.element = e;
            h.role = role;
            h.node = node;
            h.port = port;
            h.output = output;
            h.component = component;
            h.item = item;
            v.hits.Push(h);
        }

        const Hit* HitOf(const View& v, const ElementTree& tree, ElementHandle target)
        {
            for (ElementHandle e = target; e.IsValid() && !(e == v.element); e = tree.Parent(e))
                for (const Hit& h : v.hits)
                    if (h.element == e)
                        return &h;
            return nullptr;
        }

        StringView ComponentName(u32 i)
        {
            static const StringView names[] = {"X", "Y", "Z", "W"};
            return names[i & 3];
        }

        String PortLabel(const g::Graph& graph, u32 node, const g::PortDef& def, bool output, u32 port)
        {
            u32 type = g::ResolvedType(graph, node, output, port);
            u32 components = g::PortTypeAt(type).components;
            if (def.name.Empty())
                return String();
            return Format("{}({})", def.name, components == 0 ? 1u : components);
        }

        // The value an unconnected input shows to the left of its port.
        void MakeInline(View& v, ElementTree& tree, ElementHandle row, const g::Node& node, u32 input, const g::PortDef& def)
        {
            const g::PortType& type = g::PortTypeAt(def.type);
            u32 components = type.components == 0 ? 1 : type.components;
            g::Value value = node.inputValues[input];
            f32 width = 0;
            ElementHandle box = Make(tree, row, "graph-inline");
            if (def.inlineKind == g::InlineKind::Color)
            {
                ElementHandle swatch = Make(tree, box, "graph-inline__swatch");
                Style(swatch, Format("background-color: {};", Hex(Color(value.x, value.y, value.z, 1)).View()).View());
                width = 40;
            }
            else if (def.inlineKind == g::InlineKind::Channel)
            {
                Make(tree, box, "graph-inline__channel", def.name == StringView("UV") ? StringView("UV0") : StringView("Object Space"));
                width = def.name == StringView("UV") ? 40.0f : 84.0f;
            }
            else
            {
                f32 c[4] = {value.x, value.y, value.z, value.w};
                for (u32 k = 0; k < components; k++)
                {
                    ElementHandle field = Make(tree, box, "graph-inline__field");
                    Make(tree, field, "graph-inline__axis", ComponentName(k));
                    Make(tree, field, "graph-inline__value", Number(c[k]).View());
                    AddHit(v, field, Role::Inline, node.id, input, false, k);
                }
                width = f32(components) * 50;
            }
            Style(box, Format("left: {}px; width: {}px;", -width - 16, width).View());
            Make(tree, row, "graph-inline__wire");
            ElementHandle dot = Make(tree, row, "graph-inline__dot");
            Style(dot, Format("background-color: {};", Hex(g::PortTypeAt(g::ResolvedType(g::TheGraph(), node.id, false, input)).color).View()).View());
        }

        void BuildNode(View& v, ElementTree& tree, const g::Graph& graph, const g::Node& node)
        {
            const g::NodeKind& kind = g::NodeKindAt(node.kind);
            NodeElements out;
            out.id = node.id;
            bool property = kind.inputs.Empty() && kind.outputs.Size() == 1 && kind.outputs[0].name.Empty();
            StringView classes = kind.context ? StringView("graph-node graph-node--context") :
                                 property     ? StringView("graph-node graph-node--property") :
                                                StringView("graph-node");
            ElementHandle root = Make(tree, v.content, classes);
            out.root = root;
            if (IsSelected(v, node.id))
                tree.AddClass(root, Intern("graph-node--selected"));
            Style(root, Format("left: {}px; top: {}px;", node.position.x, node.position.y).View());
            AddHit(v, root, Role::Node, node.id);

            if (property)
            {
                ElementHandle row = Make(tree, root, "graph-node__property");
                Make(tree, row, "graph-node__property-dot");
                Make(tree, row, "graph-node__property-label", node.title.View());
                ElementHandle port = Make(tree, row, "graph-port graph-port--output");
                ElementHandle connector = Make(tree, port, "graph-port__connector");
                AddHit(v, port, Role::Port, node.id, 0, true);
                out.outputs.Push(connector);
            }
            else if (kind.context)
            {
                ElementHandle title = Make(tree, root, "graph-node__title graph-node__title--context");
                Make(tree, title, "graph-node__title-label", kind.name);
                ElementHandle blocks = Make(tree, root, "graph-node__blocks");
                for (u32 i = 0; i < kind.inputs.Size(); i++)
                {
                    const g::PortDef& def = kind.inputs[i];
                    ElementHandle block = Make(tree, blocks, "graph-block");
                    AddHit(v, block, Role::Node, node.id);
                    ElementHandle port = Make(tree, block, "graph-port graph-port--input");
                    ElementHandle connector = Make(tree, port, "graph-port__connector");
                    Make(tree, block, "graph-block__label", PortLabel(graph, node.id, def, false, i).View());
                    AddHit(v, port, Role::Port, node.id, i, false);
                    out.inputs.Push(connector);
                    if (!graph.Into(node.id, i) && def.inlineKind != g::InlineKind::None)
                        MakeInline(v, tree, port, node, i, def);
                }
            }
            else
            {
                ElementHandle title = Make(tree, root, "graph-node__title");
                Make(tree, title, "graph-node__title-label", kind.name);
                Make(tree, title, "graph-node__title-arrow");
                // The category color mode's stripe.
                if (Color c; g::CategoryColor(kind.category, c))
                {
                    ElementHandle stripe = Make(tree, root, "graph-node__stripe");
                    Style(stripe, Format("background-color: {};", Hex(c).View()).View());
                }
                ElementHandle ports = Make(tree, root, "graph-node__ports");
                ElementHandle inputs = Make(tree, ports, "graph-node__inputs");
                ElementHandle outputs = Make(tree, ports, "graph-node__outputs");
                if (kind.inputs.Empty())
                    tree.AddClass(inputs, Intern("graph-node__inputs--empty"));
                for (u32 i = 0; i < kind.inputs.Size(); i++)
                {
                    const g::PortDef& def = kind.inputs[i];
                    ElementHandle port = Make(tree, inputs, "graph-port graph-port--input");
                    ElementHandle connector = Make(tree, port, "graph-port__connector");
                    Make(tree, port, "graph-port__label", PortLabel(graph, node.id, def, false, i).View());
                    AddHit(v, port, Role::Port, node.id, i, false);
                    out.inputs.Push(connector);
                    if (!graph.Into(node.id, i) && def.inlineKind != g::InlineKind::None)
                        MakeInline(v, tree, port, node, i, def);
                }
                for (u32 i = 0; i < kind.outputs.Size(); i++)
                {
                    const g::PortDef& def = kind.outputs[i];
                    ElementHandle port = Make(tree, outputs, "graph-port graph-port--output");
                    Make(tree, port, "graph-port__label", PortLabel(graph, node.id, def, true, i).View());
                    ElementHandle connector = Make(tree, port, "graph-port__connector");
                    AddHit(v, port, Role::Port, node.id, i, true);
                    out.outputs.Push(connector);
                }
                if (kind.preview)
                {
                    if (node.previewOpen)
                    {
                        ElementHandle preview = tree.Create(Intern("SampleNodePreview"));
                        tree.AddClass(preview, Intern("graph-node__preview"));
                        tree.Append(root, preview);
                        PreviewOf entry;
                        entry.element = preview;
                        entry.node = node.id;
                        Previews().Push(Move(entry));
                        BindPreview(Previews().Back());
                    }
                    ElementHandle toggle = Make(tree, root, node.previewOpen ? StringView("graph-node__preview-toggle graph-node__preview-toggle--open")
                                                                             : StringView("graph-node__preview-toggle"));
                    Make(tree, toggle, node.previewOpen ? StringView("graph-node__chevron graph-node__chevron--up") : StringView("graph-node__chevron"));
                    AddHit(v, toggle, Role::PreviewToggle, node.id);
                }
            }
            // The ports' colors: their (resolved) types', filled when connected.
            auto paint = [&](const Array<ElementHandle>& connectors, bool output) {
                for (u32 i = 0; i < connectors.Size(); i++)
                {
                    Color c = g::PortTypeAt(g::ResolvedType(graph, node.id, output, i)).color;
                    bool connected = false;
                    for (const g::Edge& e : graph.edges)
                        connected = connected || (output ? (e.fromNode == node.id && e.fromPort == i) : (e.toNode == node.id && e.toPort == i));
                    Style(connectors[i], connected ? Format("border-color: {}; background-color: {};", Hex(c).View(), Hex(c).View()).View()
                                                   : Format("border-color: {};", Hex(c).View()).View());
                }
            };
            paint(out.inputs, false);
            paint(out.outputs, true);
            v.nodes.Push(Move(out));
        }

        void BuildMenu(View& v, ElementTree& tree);

        // Every node again (after a change of the graph's structure), the menu too while it is open.
        void Rebuild(View& v, ElementTree& tree)
        {
            for (usize i = 0; i < Previews().Size();)
                if (!tree.IsAlive(Previews()[i].element) || tree.IsAncestor(v.content, Previews()[i].element))
                {
                    if (engine::Texture2D* t = Previews()[i].texture.Get())
                        engine::Object::DestroyImmediate(t);
                    Previews().RemoveAt(i);
                }
                else
                    i++;
            Clear(tree, v.content);
            v.nodes.Clear();
            v.hits.Clear();
            const g::Graph& graph = g::TheGraph();
            for (const g::Node& n : graph.nodes)
                BuildNode(v, tree, graph, n);
            if (tree.IsAlive(v.menu))
                BuildMenu(v, tree);
        }

        void ApplyTransform(View& v)
        {
            Style(v.content, Format("translate: {}px {}px; scale: {} {};", v.pan.x, v.pan.y, v.zoom, v.zoom).View());
        }

        // ------------------------------------------------------------------------------------------- the panels around

        ElementHandle FindNamed(const ElementTree& tree, ElementHandle from, Atom name)
        {
            if (name == ui::kNoAtom)
                return ElementHandle();
            ElementHandle root = from;
            while (tree.Parent(root).IsValid())
                root = tree.Parent(root);
            ElementHandle found;
            tree.ForEachInSubtree(root, [&](ElementHandle e, u32) {
                if (!found.IsValid() && tree.Name(e) == name)
                    found = e;
            });
            return found;
        }

        void Row(ElementTree& tree, ElementHandle parent, StringView name, StringView value, bool dropdown = false)
        {
            ElementHandle row = Make(tree, parent, "inspector-row");
            Make(tree, row, "inspector-row__name", name);
            ElementHandle field = Make(tree, row, dropdown ? StringView("inspector-row__value inspector-row__value--dropdown") : StringView("inspector-row__value"));
            Make(tree, field, "inspector-row__text", value);
            if (dropdown)
                Make(tree, field, "inspector-row__arrow");
        }

        void Heading(ElementTree& tree, ElementHandle parent, StringView text, bool foldout = true)
        {
            ElementHandle h = Make(tree, parent, foldout ? StringView("inspector-heading inspector-heading--foldout") : StringView("inspector-heading"));
            if (foldout)
                Make(tree, h, "inspector-heading__arrow");
            Make(tree, h, "inspector-heading__text", text);
        }

        void UpdatePanels(View& v, ElementTree& tree)
        {
            const g::Graph& graph = g::TheGraph();
            if (ElementHandle status = FindNamed(tree, v.element, v.status); status.IsValid())
            {
                String sel = v.selected.Size() == 1 && graph.Find(v.selected[0]) ? String(g::NodeKindAt(graph.Find(v.selected[0])->kind).name)
                                                                                    : Format("{} selected", u32(v.selected.Size()));
                tree.SetText(status, Format("{} nodes   {} edges   Zoom {}%   {}", u32(graph.nodes.Size()), u32(graph.edges.Size()),
                                            i32(std::lround(v.zoom * 100)), sel.View())
                                         .View());
            }
            const g::Node* only = v.selected.Size() == 1 ? graph.Find(v.selected[0]) : nullptr;
            for (u32 k = 0; k < 2; k++)
                if (ElementHandle tab = FindNamed(tree, v.element, k == 0 ? v.nodeTab : v.graphTab); tab.IsValid())
                {
                    if ((k == 0) == (only != nullptr))
                        tree.AddClass(tab, Intern("tabs__tab--active"));
                    else
                        tree.RemoveClass(tab, Intern("tabs__tab--active"));
                }
            ElementHandle inspector = FindNamed(tree, v.element, v.inspector);
            if (!inspector.IsValid())
                return;
            Clear(tree, inspector);
            const g::Node* node = v.selected.Size() == 1 ? graph.Find(v.selected[0]) : nullptr;
            if (!node)
            {
                Row(tree, inspector, "Precision", "Single", true);
                Heading(tree, inspector, "Target Settings", false);
                Heading(tree, inspector, "Standard");
                Row(tree, inspector, "Material", "Lit", true);
                Row(tree, inspector, "Workflow Mode", "Metallic", true);
                Row(tree, inspector, "Surface Type", "Opaque", true);
                Row(tree, inspector, "Render Face", "Front", true);
                Row(tree, inspector, "Depth Write", "Auto", true);
                Row(tree, inspector, "Depth Test", "LEqual", true);
                Row(tree, inspector, "Alpha Clipping", "Off");
                Row(tree, inspector, "Cast Shadows", "On");
                Heading(tree, inspector, "Graph");
                Row(tree, inspector, "Nodes", Format("{}", u32(graph.nodes.Size())).View());
                Row(tree, inspector, "Edges", Format("{}", u32(graph.edges.Size())).View());
                return;
            }
            const g::NodeKind& kind = g::NodeKindAt(node->kind);
            Heading(tree, inspector, node->title.Empty() ? kind.name : node->title.View(), false);
            Make(tree, inspector, "inspector-description", kind.description);
            Row(tree, inspector, "Precision", "Inherit", true);
            Row(tree, inspector, "Preview", kind.preview ? (node->previewOpen ? StringView("Inherit") : StringView("Hidden")) : StringView("None"), true);
            if (!kind.category.Empty())
                Row(tree, inspector, "Category", kind.category);
            if (!node->title.Empty())
            {
                g::Value p = node->property;
                Row(tree, inspector, "Default", kind.outputs[0].type == g::FindPortType("Float") ? Number(p.x).View()
                                                                                                  : Hex(Color(p.x, p.y, p.z, 1)).View());
                Row(tree, inspector, "Exposed", "Yes");
            }
            if (!kind.inputs.Empty())
                Heading(tree, inspector, "Inputs");
            for (u32 i = 0; i < kind.inputs.Size(); i++)
            {
                const g::PortDef& def = kind.inputs[i];
                String value;
                if (const g::Edge* e = graph.Into(node->id, i))
                {
                    const g::Node* from = graph.Find(e->fromNode);
                    const g::NodeKind& fk = g::NodeKindAt(from->kind);
                    value = String(from->title.Empty() ? fk.name : from->title.View());
                }
                else if (def.inlineKind == g::InlineKind::Channel)
                    value = String(def.name == StringView("UV") ? StringView("UV0") : StringView("Object Space"));
                else if (def.inlineKind == g::InlineKind::Color)
                {
                    g::Value c = node->inputValues[i];
                    value = Hex(Color(c.x, c.y, c.z, 1));
                }
                else
                {
                    g::Value c = node->inputValues[i];
                    u32 n = g::PortTypeAt(def.type).components;
                    n = n == 0 ? 1 : n;
                    f32 x[4] = {c.x, c.y, c.z, c.w};
                    for (u32 k = 0; k < n; k++)
                    {
                        if (k)
                            value += "  ";
                        value += Number(x[k]).View();
                    }
                }
                Row(tree, inspector, def.name, value.View());
            }
        }

        // ------------------------------------------------------------------------------------------- the menu

        // The create node menu's top level: the categories' first parts, in the kinds' order.
        Array<StringView> Categories()
        {
            Array<StringView> out;
            for (u32 i = 0; i < g::NodeKindCount(); i++)
            {
                StringView c = g::NodeKindAt(i).category;
                if (c.Empty())
                    continue;
                usize slash = c.Find('/');
                StringView top = slash == StringView::npos ? c : c.Substr(0, slash);
                bool known = false;
                for (StringView k : out)
                    known = known || k == top;
                if (!known)
                    out.Push(top);
            }
            return out;
        }

        void BuildMenu(View& v, ElementTree& tree)
        {
            Clear(tree, v.menu);
            for (usize i = 0; i < v.hits.Size();)
                if (v.hits[i].role == Role::Menu || v.hits[i].role == Role::MenuItem)
                    v.hits.RemoveAt(i);
                else
                    i++;
            AddHit(v, v.menu, Role::Menu);
            Array<StringView> categories = Categories();
            Make(tree, v.menu, "graph-menu__title", v.menuCategory < 0 ? StringView("Create Node") : categories[usize(v.menuCategory)]);
            ElementHandle search = Make(tree, v.menu, "graph-menu__search");
            Make(tree, search, "graph-menu__search-icon");
            Make(tree, search, "graph-menu__search-text", "Search");
            ElementHandle list = Make(tree, v.menu, "graph-menu__list");
            // Placed at the pointer, inside the view with all of its items.
            u32 items = v.menuCategory < 0 ? u32(categories.Size()) : 1;
            if (v.menuCategory >= 0)
                for (u32 i = 0; i < g::NodeKindCount(); i++)
                    items += g::NodeKindAt(i).category.StartsWith(categories[usize(v.menuCategory)]) ? 1 : 0;
            Rect r = RectOf(v.element);
            f32 height = 64 + f32(items) * 22;
            Style(v.menu, Format("left: {}px; top: {}px;", Clamp(v.menuLocal.x, 4, r.width - 244), Clamp(v.menuLocal.y, 4, std::fmax(4.0f, r.height - height - 4))).View());
            if (v.menuCategory < 0)
            {
                for (usize i = 0; i < categories.Size(); i++)
                {
                    ElementHandle item = Make(tree, list, "graph-menu__item");
                    Make(tree, item, "graph-menu__item-icon graph-menu__item-icon--folder");
                    Make(tree, item, "graph-menu__item-label", categories[i]);
                    Make(tree, item, "graph-menu__item-arrow");
                    AddHit(v, item, Role::MenuItem, 0, 0, false, 0, i32(i));
                }
                return;
            }
            ElementHandle back = Make(tree, list, "graph-menu__item graph-menu__item--back");
            Make(tree, back, "graph-menu__item-back");
            Make(tree, back, "graph-menu__item-label", "Back");
            AddHit(v, back, Role::MenuItem, 0, 0, false, 0, -2);
            StringView top = categories[usize(v.menuCategory)];
            for (u32 i = 0; i < g::NodeKindCount(); i++)
            {
                const g::NodeKind& k = g::NodeKindAt(i);
                if (!k.category.StartsWith(top))
                    continue;
                ElementHandle item = Make(tree, list, "graph-menu__item");
                Make(tree, item, "graph-menu__item-icon");
                Make(tree, item, "graph-menu__item-label", k.name);
                usize slash = k.category.Find('/');
                if (slash != StringView::npos)
                    Make(tree, item, "graph-menu__item-hint", k.category.Substr(slash + 1));
                AddHit(v, item, Role::MenuItem, 0, 0, false, 0, i32(1000 + i));
            }
        }

        void OpenMenu(View& v, ElementTree& tree, Vector2 local)
        {
            if (!tree.IsAlive(v.menu))
            {
                v.menu = Make(tree, v.element, "graph-menu");
            }
            v.menuAt = ToGraph(v, local);
            v.menuLocal = local;
            v.menuCategory = -1;
            BuildMenu(v, tree);
        }

        void CloseMenu(View& v, ElementTree& tree)
        {
            if (tree.IsAlive(v.menu))
                tree.Destroy(v.menu);
            v.menu = ElementHandle();
            for (usize i = 0; i < v.hits.Size();)
                if (v.hits[i].role == Role::Menu || v.hits[i].role == Role::MenuItem)
                    v.hits.RemoveAt(i);
                else
                    i++;
        }

        // ------------------------------------------------------------------------------------------- framing

        void Frame(View& v, bool all)
        {
            if (v.nodes.Empty())
                return;
            Rect content = RectOf(v.content), full = RectOf(v.element);
            // The room between the panels over the view.
            Rect view(full.x + v.insets[0], full.y + v.insets[1], full.width - v.insets[0] - v.insets[2], full.height - v.insets[1] - v.insets[3]);
            f32 x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
            for (const NodeElements& n : v.nodes)
            {
                Rect r = RectOf(n.root);
                // An inline value reaches left of its node.
                x0 = std::fmin(x0, r.x - content.x - 150);
                y0 = std::fmin(y0, r.y - content.y);
                x1 = std::fmax(x1, r.x - content.x + r.width);
                y1 = std::fmax(y1, r.y - content.y + r.height);
            }
            f32 w = x1 - x0, h = y1 - y0;
            if (w <= 0 || h <= 0 || view.width <= 0 || view.height <= 0)
                return;
            f32 fit = std::fmin((view.width - 40) / w, (view.height - 40) / h);
            v.zoom = Clamp(fit, all ? 0.2f : 0.72f, 1.0f);
            v.pan = Vector2(v.insets[0] + (view.width - w * v.zoom) * 0.5f - x0 * v.zoom, v.insets[1] + (view.height - h * v.zoom) * 0.5f - y0 * v.zoom);
            // Too wide to be read whole: its end (the contexts) stays in view, its start goes out on the left.
            if (!all && w * v.zoom > view.width - 40)
                v.pan.x = v.insets[0] + view.width - 24 - x1 * v.zoom;
            ApplyTransform(v);
        }

        // ------------------------------------------------------------------------------------------- edges

        void EdgeCurve(Vector2 a, Vector2 b, f32 zoom, Vector2& c1, Vector2& c2)
        {
            f32 dx = std::fmax(36 * zoom, std::fabs(b.x - a.x) * 0.5f);
            c1 = a + Vector2(dx, 0);
            c2 = b - Vector2(dx, 0);
        }

        Vector2 Bezier(Vector2 a, Vector2 b, Vector2 c, Vector2 d, f32 t)
        {
            f32 u = 1 - t;
            return a * (u * u * u) + b * (3 * u * u * t) + c * (3 * u * t * t) + d * (t * t * t);
        }

        bool EdgeEnds(const View& v, const g::Edge& e, Vector2& a, Vector2& b)
        {
            const NodeElements* from = ElementsOf(v, e.fromNode);
            const NodeElements* to = ElementsOf(v, e.toNode);
            if (!from || !to || e.fromPort >= from->outputs.Size() || e.toPort >= to->inputs.Size())
                return false;
            a = ConnectorAt(v, from->outputs[e.fromPort]);
            b = ConnectorAt(v, to->inputs[e.toPort]);
            return true;
        }

        i32 EdgeAt(const View& v, Vector2 local)
        {
            const g::Graph& graph = g::TheGraph();
            for (usize i = 0; i < graph.edges.Size(); i++)
            {
                Vector2 a, b, c1, c2;
                if (!EdgeEnds(v, graph.edges[i], a, b))
                    continue;
                EdgeCurve(a, b, v.zoom, c1, c2);
                Vector2 last = a;
                for (u32 k = 1; k <= 32; k++)
                {
                    Vector2 p = Bezier(a, c1, c2, b, f32(k) / 32);
                    Vector2 d = p - last, w = local - last;
                    f32 len = d.x * d.x + d.y * d.y;
                    f32 t = len > 0 ? Clamp((w.x * d.x + w.y * d.y) / len, 0, 1) : 0;
                    Vector2 q = last + d * t - local;
                    if (q.x * q.x + q.y * q.y < 36)
                        return i32(i);
                    last = p;
                }
            }
            return -1;
        }

        void StrokeEdge(Painter& p, Vector2 a, Vector2 b, f32 zoom, Color from, Color to, f32 width)
        {
            Vector2 c1, c2;
            EdgeCurve(a, b, zoom, c1, c2);
            p.lineWidth = width;
            p.lineCap = ui::LineCap::Round;
            p.strokeGradient.Clear();
            if (!(from.r == to.r && from.g == to.g && from.b == to.b))
            {
                p.strokeGradient.Push(ui::GradientKey{0, from});
                p.strokeGradient.Push(ui::GradientKey{1, to});
            }
            p.strokeColor = from;
            p.BeginPath();
            p.MoveTo(a);
            p.BezierCurveTo(c1, c2, b);
            p.Stroke();
            p.strokeGradient.Clear();
        }

        // ------------------------------------------------------------------------------------------- painting

        f32 PreviewTime()
        {
            // Batch runs (the goldens) draw one moment; a window runs the clock.
            if (engine::Application::isBatchMode())
                return 2.0f;
            return f32(engine::Time::realtimeSinceStartupAsDouble());
        }

        Color Saturated(g::Value v, u32 components)
        {
            auto c = [](f32 x) { return Clamp(x, 0, 1); };
            if (components == 1 || components == 0)
                return Color(c(v.x), c(v.x), c(v.x), 1);
            if (components == 2)
                return Color(c(v.x), c(v.y), 0, 1);
            return Color(c(v.x), c(v.y), c(v.z), 1);
        }

        Color32 ToColor32(f32 r, f32 g, f32 b)
        {
            auto c = [](f32 x) { return u8(std::lround(Clamp(x, 0, 1) * 255)); };
            return Color32(c(r), c(g), c(b), 255);
        }

        // Every node's preview in one pass: the whole graph once a point, each preview's first output from it.
        void WriteNodePreviews(f32 time)
        {
            const g::Graph& graph = g::TheGraph();
            g::Evaluator ev;
            ev.PrepareAll(graph);
            struct Target
            {
                PreviewOf* entry;
                i32 slot;
                u32 components;
            };
            Array<Target> targets;
            for (PreviewOf& p : Previews())
                if (!p.main && p.texture.Get() && graph.Find(p.node))
                    targets.Push(Target{&p, ev.Slot(p.node, 0), g::PortTypeAt(g::ResolvedType(graph, p.node, true, 0)).components});
            if (targets.Empty())
                return;
            const i32 n = kNodePreviewSize;
            for (i32 y = 0; y < n; y++)
                for (i32 x = 0; x < n; x++)
                {
                    ev.Run(Vector2((f32(x) + 0.5f) / f32(n), (f32(y) + 0.5f) / f32(n)), time);
                    for (const Target& t : targets)
                    {
                        Color c = Saturated(t.slot >= 0 ? ev.ValueAt(t.slot) : g::Value(0, 0, 0, 1), t.components);
                        t.entry->pixels[usize(y) * n + usize(x)] = ToColor32(c.r, c.g, c.b);
                    }
                }
            for (const Target& t : targets)
            {
                t.entry->texture.Get()->SetPixels32(Span<const Color32>(t.entry->pixels.Data(), t.entry->pixels.Size()));
                t.entry->texture.Get()->Apply(false);
            }
        }

        // The graph's result on a lit sphere over a backdrop: base color, smoothness and emission of the Fragment
        // context.
        void WriteMainPreview(PreviewOf& entry, f32 time)
        {
            const g::Graph& graph = g::TheGraph();
            const g::Node* fragment = nullptr;
            for (const g::Node& n : graph.nodes)
                if (g::NodeKindAt(n.kind).context && g::NodeKindAt(n.kind).name == StringView("Fragment"))
                    fragment = &n;
            g::Evaluator base, smooth, emission;
            if (fragment)
            {
                base.Prepare(graph, fragment->id, -1, 0);
                smooth.Prepare(graph, fragment->id, -1, 3);
                emission.Prepare(graph, fragment->id, -1, 4);
            }
            const i32 w = kMainPreviewWidth, h = kMainPreviewHeight;
            const f32 radius = f32(h) * 0.42f, cx = f32(w) * 0.5f, cy = f32(h) * 0.48f;
            const f32 lx = -0.45f, ly = 0.6f, lz = 0.66f, ll = std::sqrt(lx * lx + ly * ly + lz * lz);
            for (i32 y = 0; y < h; y++)
                for (i32 x = 0; x < w; x++)
                {
                    // Rows go up: y from the bottom.
                    f32 v = (f32(y) + 0.5f) / f32(h);
                    Color back(0.13f + 0.07f * v, 0.135f + 0.075f * v, 0.15f + 0.08f * v, 1);
                    f32 sx = (f32(x) + 0.5f - cx) / radius, sy = (f32(y) + 0.5f - cy) / radius;
                    f32 d = std::sqrt(sx * sx + sy * sy);
                    // The edge's coverage over a pixel.
                    f32 cover = Clamp((1 - d) * radius + 0.5f, 0, 1);
                    Color c = back;
                    if (cover > 0 && fragment)
                    {
                        f32 nx = sx / std::fmax(d, 1.0f), ny = sy / std::fmax(d, 1.0f);
                        f32 nz = std::sqrt(std::fmax(0.0f, 1 - nx * nx - ny * ny));
                        Vector2 uv(0.5f + std::atan2(nx, nz) * 0.15915494f, 0.5f + std::asin(Clamp(ny, -1, 1)) * 0.31830989f);
                        g::Value b = base.At(uv, time), e = emission.At(uv, time);
                        f32 s2 = Clamp(smooth.At(uv, time).x, 0, 1);
                        f32 ndl = std::fmax(0.0f, (nx * lx + ny * ly + nz * lz) / ll);
                        f32 hx = lx / ll, hy = ly / ll, hz = lz / ll + 1;
                        f32 hl = std::sqrt(hx * hx + hy * hy + hz * hz);
                        f32 ndh = std::fmax(0.0f, (nx * hx + ny * hy + nz * hz) / hl);
                        f32 spec = std::pow(ndh, 4 + s2 * s2 * 180) * (0.04f + s2 * 0.7f) * ndl * 1.6f;
                        f32 sky = 0.18f + 0.12f * ny;
                        f32 rim = (1 - nz) * (1 - nz) * (1 - nz) * 0.25f * s2;
                        auto shade = [&](f32 albedo, f32 emit, f32 tint) { return Clamp(albedo * (ndl * 0.92f + sky) + spec + rim * tint + emit, 0, 1); };
                        Color lit(shade(b.x, e.x, 0.6f), shade(b.y, e.y, 0.75f), shade(b.z, e.z, 1.0f), 1);
                        c = Color(back.r + (lit.r - back.r) * cover, back.g + (lit.g - back.g) * cover, back.b + (lit.b - back.b) * cover, 1);
                    }
                    entry.pixels[usize(y) * w + usize(x)] = ToColor32(c.r, c.g, c.b);
                }
        }

        // Each preview's image, written again when the graph or its time moved on; the painter draws nothing (the
        // image is the element's background).
        void PaintPreview(Painter&, const ElementTree&, ElementHandle e, f32, f32)
        {
            PreviewOf* entry = PreviewEntry(e);
            if (!entry)
                return;
            if (!entry->texture.Get())
                BindPreview(*entry);
            engine::Texture2D* t = entry->texture.Get();
            if (!t)
                return;
            f32 time = PreviewTime();
            u32 version = g::TheGraph().version;
            if (!entry->main)
            {
                // The first node preview painted in a frame writes them all.
                NodePreviewState& state = NodePreviews();
                if (state.version == version && std::fabs(time - state.time) < 1.0f / 30.0f && entry->version == version)
                    return;
                state.version = version;
                state.time = time;
                for (PreviewOf& p : Previews())
                    if (!p.main)
                        p.version = version;
                WriteNodePreviews(time);
                return;
            }
            if (entry->version == version && std::fabs(time - entry->time) < 1.0f / 30.0f)
                return;
            entry->version = version;
            entry->time = time;
            WriteMainPreview(*entry, time);
            t->SetPixels32(Span<const Color32>(entry->pixels.Data(), entry->pixels.Size()));
            t->Apply(false);
        }

        // The grid and the edges, under the nodes.
        void PaintGraph(Painter& p, const ElementTree&, ElementHandle e, f32 w, f32 h)
        {
            View* v = ViewOf(e);
            if (!v)
                return;
            p.strokeGradient.Clear();
            p.lineCap = ui::LineCap::Butt;
            // Lines every 20 units (when they are far enough apart), stronger every 100.
            for (u32 pass = 0; pass < 2; pass++)
            {
                f32 step = (pass == 0 ? 20.0f : 100.0f) * v->zoom;
                if (step < 7)
                    continue;
                p.lineWidth = 1;
                p.strokeColor = pass == 0 ? Color(1, 1, 1, 0.012f) : Color(1, 1, 1, 0.03f);
                f32 x = std::fmod(v->pan.x, step);
                x = x < 0 ? x + step : x;
                for (; x < w; x += step)
                {
                    p.BeginPath();
                    p.MoveTo(Vector2(std::floor(x) + 0.5f, 0));
                    p.LineTo(Vector2(std::floor(x) + 0.5f, h));
                    p.Stroke();
                }
                f32 y = std::fmod(v->pan.y, step);
                y = y < 0 ? y + step : y;
                for (; y < h; y += step)
                {
                    p.BeginPath();
                    p.MoveTo(Vector2(0, std::floor(y) + 0.5f));
                    p.LineTo(Vector2(w, std::floor(y) + 0.5f));
                    p.Stroke();
                }
            }
            const g::Graph& graph = g::TheGraph();
            f32 width = Clamp(2.2f * v->zoom, 1.4f, 3.5f);
            for (usize i = 0; i < graph.edges.Size(); i++)
            {
                const g::Edge& edge = graph.edges[i];
                Vector2 a, b;
                if (!EdgeEnds(*v, edge, a, b))
                    continue;
                Color from = g::PortTypeAt(g::ResolvedType(graph, edge.fromNode, true, edge.fromPort)).color;
                Color to = g::PortTypeAt(g::ResolvedType(graph, edge.toNode, false, edge.toPort)).color;
                bool lit = i32(i) == v->selectedEdge || IsSelected(*v, edge.fromNode) || IsSelected(*v, edge.toNode);
                if (i32(i) == v->selectedEdge)
                    StrokeEdge(p, a, b, v->zoom, Color(kSelection.r, kSelection.g, kSelection.b, 0.35f), Color(kSelection.r, kSelection.g, kSelection.b, 0.35f), width + 4);
                StrokeEdge(p, a, b, v->zoom, lit ? from : Color(from.r * 0.85f, from.g * 0.85f, from.b * 0.85f, 1),
                           lit ? to : Color(to.r * 0.85f, to.g * 0.85f, to.b * 0.85f, 1), width);
            }
        }

        // Over the nodes: the band of a selection and the edge being drawn.
        void PaintOverlay(Painter& p, const ElementTree&, ElementHandle e, f32, f32)
        {
            View* v = ViewOf(e);
            if (!v)
                return;
            if (v->drag == Drag::Band)
            {
                f32 x0 = std::fmin(v->pressAt.x, v->at.x), y0 = std::fmin(v->pressAt.y, v->at.y);
                f32 x1 = std::fmax(v->pressAt.x, v->at.x), y1 = std::fmax(v->pressAt.y, v->at.y);
                p.fillColor = Color(kSelection.r, kSelection.g, kSelection.b, 0.08f);
                p.strokeColor = Color(kSelection.r, kSelection.g, kSelection.b, 0.8f);
                p.lineWidth = 1;
                p.BeginPath();
                p.MoveTo(Vector2(x0, y0));
                p.LineTo(Vector2(x1, y0));
                p.LineTo(Vector2(x1, y1));
                p.LineTo(Vector2(x0, y1));
                p.ClosePath();
                p.Fill();
                p.Stroke();
            }
            if (v->drag == Drag::Edge)
            {
                const NodeElements* n = ElementsOf(*v, v->edgeNode);
                if (!n)
                    return;
                const Array<ElementHandle>& ports = v->edgeFromOutput ? n->outputs : n->inputs;
                if (v->edgePort >= ports.Size())
                    return;
                Vector2 a = ConnectorAt(*v, ports[v->edgePort]);
                Color c = g::PortTypeAt(g::ResolvedType(g::TheGraph(), v->edgeNode, v->edgeFromOutput, v->edgePort)).color;
                if (v->edgeFromOutput)
                    StrokeEdge(p, a, v->at, v->zoom, c, c, Clamp(2.2f * v->zoom, 1.4f, 3.5f));
                else
                    StrokeEdge(p, v->at, a, v->zoom, c, c, Clamp(2.2f * v->zoom, 1.4f, 3.5f));
            }
        }

        // ------------------------------------------------------------------------------------------- events

        void Select(View& v, ElementTree& tree, u32 node, bool add)
        {
            if (!add)
                v.selected.Clear();
            if (node && !IsSelected(v, node))
                v.selected.Push(node);
            else if (node && add)
                for (usize i = 0; i < v.selected.Size(); i++)
                    if (v.selected[i] == node && v.selected.Size() > 1)
                    {
                        v.selected.RemoveAt(i);
                        break;
                    }
            for (const NodeElements& n : v.nodes)
            {
                if (IsSelected(v, n.id))
                    tree.AddClass(n.root, Intern("graph-node--selected"));
                else
                    tree.RemoveClass(n.root, Intern("graph-node--selected"));
            }
        }

        void DeleteSelection(View& v, ElementTree& tree)
        {
            g::Graph& graph = g::TheGraph();
            if (v.selectedEdge >= 0 && usize(v.selectedEdge) < graph.edges.Size())
            {
                const g::Edge e = graph.edges[usize(v.selectedEdge)];
                graph.Disconnect(e.toNode, e.toPort);
            }
            for (u32 id : v.selected)
                if (const g::Node* n = graph.Find(id); n && !g::NodeKindAt(n->kind).context)
                    graph.Remove(id);
            v.selected.Clear();
            v.selectedEdge = -1;
            Rebuild(v, tree);
            UpdatePanels(v, tree);
        }

        // The port under a point (an input for an edge from an output and back), with its node.
        bool PortUnder(View& v, EventSystem& events, Vector2 panel, u32& node, u32& port)
        {
            ElementHandle under = events.Pick(panel);
            const Hit* hit = HitOf(v, events.tree(), under);
            if (!hit)
                return false;
            if (hit->role == Role::Port && hit->output != v.edgeFromOutput && hit->node != v.edgeNode)
            {
                node = hit->node;
                port = hit->port;
                return true;
            }
            // Over a node: its first port of the other side.
            if (hit->role == Role::Node && hit->node != v.edgeNode)
            {
                const NodeElements* n = ElementsOf(v, hit->node);
                const Array<ElementHandle>& list = v.edgeFromOutput ? n->inputs : n->outputs;
                f32 best = 1e9f;
                Vector2 at = LocalToContent(v, LocalOf(v, panel));
                for (u32 i = 0; i < list.Size(); i++)
                {
                    Vector2 c = Center(RectOf(list[i]));
                    f32 d = std::fabs(c.y - at.y);
                    if (d < best)
                    {
                        best = d;
                        node = hit->node;
                        port = i;
                    }
                }
                return best < 1e8f;
            }
            return false;
        }

        void OnPointerDown(void*, EventSystem& events, UIEvent& e)
        {
            View* v = ViewOf(e.currentTarget);
            if (!v)
                return;
            ElementTree& tree = events.tree();
            events.Focus(v->element);
            Vector2 local = LocalOf(*v, e.position);
            const Hit* hit = HitOf(*v, tree, e.target);
            bool inMenu = hit && (hit->role == Role::Menu || hit->role == Role::MenuItem);
            if (tree.IsAlive(v->menu) && !inMenu)
            {
                CloseMenu(*v, tree);
                if (e.button != 1)
                    return;
            }
            v->pressAt = v->at = local;
            v->moved = false;
            if (e.button == 2 || (e.button == 0 && (e.modifiers & kAlt)))
            {
                v->drag = Drag::Pan;
                events.CapturePointer(v->element, e.pointerId);
                e.StopPropagation();
                return;
            }
            if (e.button == 1)
            {
                OpenMenu(*v, tree, local);
                e.StopPropagation();
                return;
            }
            if (e.button != 0)
                return;
            g::Graph& graph = g::TheGraph();
            if (hit && hit->role == Role::MenuItem)
            {
                if (hit->item == -2)
                    v->menuCategory = -1;
                else if (hit->item >= 1000)
                {
                    u32 kind = u32(hit->item - 1000);
                    u32 id = graph.Add(kind, v->menuAt);
                    if (g::NodeKindAt(kind).outputs.Size() == 1 && g::NodeKindAt(kind).outputs[0].name.Empty())
                        graph.Find(id)->title = String(g::NodeKindAt(kind).name);
                    CloseMenu(*v, tree);
                    v->selected.Clear();
                    v->selected.Push(id);
                    Rebuild(*v, tree);
                    UpdatePanels(*v, tree);
                    e.StopPropagation();
                    return;
                }
                else
                    v->menuCategory = hit->item;
                BuildMenu(*v, tree);
                e.StopPropagation();
                return;
            }
            if (inMenu)
                return;
            if (hit && hit->role == Role::PreviewToggle)
            {
                if (g::Node* n = graph.Find(hit->node))
                    n->previewOpen = !n->previewOpen;
                Rebuild(*v, tree);
                UpdatePanels(*v, tree);
                e.StopPropagation();
                return;
            }
            if (hit && hit->role == Role::Port)
            {
                v->drag = Drag::Edge;
                v->edgeNode = hit->node;
                v->edgePort = hit->port;
                v->edgeFromOutput = hit->output;
                // A connected input: its edge comes off and follows the pointer from its output.
                if (!hit->output)
                    if (const g::Edge* edge = graph.Into(hit->node, hit->port))
                    {
                        v->edgeNode = edge->fromNode;
                        v->edgePort = edge->fromPort;
                        v->edgeFromOutput = true;
                        graph.Disconnect(hit->node, hit->port);
                        Rebuild(*v, tree);
                    }
                events.CapturePointer(v->element, e.pointerId);
                e.StopPropagation();
                return;
            }
            if (hit && hit->role == Role::Inline)
            {
                v->drag = Drag::Scrub;
                v->scrubNode = hit->node;
                v->scrubInput = hit->port;
                v->scrubComponent = hit->component;
                v->scrubElement = hit->element;
                g::Value value = graph.Find(hit->node)->inputValues[hit->port];
                f32 c[4] = {value.x, value.y, value.z, value.w};
                v->scrubStart = c[hit->component & 3];
                events.CapturePointer(v->element, e.pointerId);
                e.StopPropagation();
                return;
            }
            if (hit && hit->role == Role::Node)
            {
                if (!IsSelected(*v, hit->node) || (e.modifiers & kShift))
                    Select(*v, tree, hit->node, (e.modifiers & (kShift | kControl)) != 0);
                v->selectedEdge = -1;
                v->drag = Drag::Move;
                v->moveStart.Clear();
                for (u32 id : v->selected)
                    v->moveStart.Push(graph.Find(id) ? graph.Find(id)->position : Vector2(0, 0));
                events.CapturePointer(v->element, e.pointerId);
                UpdatePanels(*v, tree);
                e.StopPropagation();
                return;
            }
            // The background: an edge under the pointer is selected, else a band starts.
            i32 edge = EdgeAt(*v, local);
            if (!(e.modifiers & kShift))
                Select(*v, tree, 0, false);
            v->selectedEdge = edge;
            if (edge < 0)
            {
                v->drag = Drag::Band;
                events.CapturePointer(v->element, e.pointerId);
            }
            UpdatePanels(*v, tree);
            e.StopPropagation();
        }

        void OnPointerMove(void*, EventSystem& events, UIEvent& e)
        {
            View* v = ViewOf(e.currentTarget);
            if (!v || v->drag == Drag::None)
                return;
            Vector2 local = LocalOf(*v, e.position);
            Vector2 delta = local - v->at;
            v->at = local;
            v->moved = true;
            g::Graph& graph = g::TheGraph();
            switch (v->drag)
            {
            case Drag::Pan:
                v->pan += delta;
                ApplyTransform(*v);
                break;
            case Drag::Move:
            {
                Vector2 by = (local - v->pressAt) / v->zoom;
                for (usize i = 0; i < v->selected.Size(); i++)
                    if (g::Node* n = graph.Find(v->selected[i]))
                    {
                        // On the grid's small steps.
                        Vector2 p = v->moveStart[i] + by;
                        n->position = Vector2(std::round(p.x / 10) * 10, std::round(p.y / 10) * 10);
                        if (const NodeElements* ne = ElementsOf(*v, n->id))
                            Style(ne->root, Format("left: {}px; top: {}px;", n->position.x, n->position.y).View());
                    }
                break;
            }
            case Drag::Scrub:
                if (g::Node* n = graph.Find(v->scrubNode))
                {
                    f32 value = v->scrubStart + (local.x - v->pressAt.x) * 0.02f;
                    value = std::round(value * 100) / 100;
                    g::Value& x = n->inputValues[v->scrubInput];
                    f32* c[4] = {&x.x, &x.y, &x.z, &x.w};
                    *c[v->scrubComponent & 3] = value;
                    graph.version++;
                    ElementHandle text = events.tree().LastChild(v->scrubElement);
                    events.tree().SetText(text, Number(value).View());
                }
                break;
            default:
                break;
            }
        }

        void OnPointerUp(void*, EventSystem& events, UIEvent& e)
        {
            View* v = ViewOf(e.currentTarget);
            if (!v || v->drag == Drag::None)
                return;
            ElementTree& tree = events.tree();
            g::Graph& graph = g::TheGraph();
            Drag drag = v->drag;
            v->drag = Drag::None;
            if (events.HasPointerCapture(v->element, e.pointerId))
                events.ReleasePointer(v->element, e.pointerId);
            if (drag == Drag::Edge)
            {
                u32 node = 0, port = 0;
                if (PortUnder(*v, events, e.position, node, port))
                {
                    if (v->edgeFromOutput)
                        graph.Connect(v->edgeNode, v->edgePort, node, port);
                    else
                        graph.Connect(node, port, v->edgeNode, v->edgePort);
                }
                Rebuild(*v, tree);
            }
            else if (drag == Drag::Band)
            {
                Vector2 a = ToGraph(*v, v->pressAt), b = ToGraph(*v, v->at);
                f32 x0 = std::fmin(a.x, b.x), y0 = std::fmin(a.y, b.y), x1 = std::fmax(a.x, b.x), y1 = std::fmax(a.y, b.y);
                Rect content = RectOf(v->content);
                for (const NodeElements& n : v->nodes)
                {
                    Rect r = RectOf(n.root);
                    f32 nx = r.x - content.x, ny = r.y - content.y;
                    if (nx < x1 && nx + r.width > x0 && ny < y1 && ny + r.height > y0 && !IsSelected(*v, n.id))
                        v->selected.Push(n.id);
                }
                Select(*v, tree, 0, true);
            }
            else if (drag == Drag::Scrub)
                Rebuild(*v, tree);
            UpdatePanels(*v, tree);
        }

        void OnWheel(void*, EventSystem& events, UIEvent& e)
        {
            View* v = ViewOf(e.currentTarget);
            if (!v || std::fabs(e.delta.y) < 1e-4f)
                return;
            Vector2 local = LocalOf(*v, e.position);
            Vector2 at = ToGraph(*v, local);
            v->zoom = Clamp(v->zoom * std::pow(1.05f, -e.delta.y), 0.2f, 2.0f);
            v->pan = local - at * v->zoom;
            ApplyTransform(*v);
            UpdatePanels(*v, events.tree());
            e.StopPropagation();
        }

        void OnKeyDown(void*, EventSystem& events, UIEvent& e)
        {
            View* v = ViewOf(e.currentTarget);
            if (!v)
                return;
            ElementTree& tree = events.tree();
            switch (e.keyCode)
            {
            case kKeyDelete:
            case kKeyBackspace:
                DeleteSelection(*v, tree);
                break;
            case kKeyF:
            case kKeyA:
                Frame(*v, true);
                UpdatePanels(*v, tree);
                break;
            case kKeySpace:
                OpenMenu(*v, tree, LocalOf(*v, events.PointerPosition(ui::PointerId::mouse)));
                break;
            case kKeyEscape:
                CloseMenu(*v, tree);
                break;
            default:
                return;
            }
            e.StopPropagation();
        }

        void OnGeometry(void*, EventSystem& events, UIEvent& e)
        {
            View* v = ViewOf(e.currentTarget);
            if (!v || v->framed || e.newRect.width <= 0)
                return;
            v->framed = true;
            Frame(*v, false);
            UpdatePanels(*v, events.tree());
        }

        // ------------------------------------------------------------------------------------------- types

        // The markup's attributes of graphs, by element (read when they are attached).
        struct GraphNames
        {
            ElementHandle element;
            Atom inspector = ui::kNoAtom, status = ui::kNoAtom, nodeTab = ui::kNoAtom, graphTab = ui::kNoAtom;
            String select;
            f32 insets[4] = {0, 0, 0, 0};
        };

        Array<GraphNames>& Names()
        {
            static Array<GraphNames>* names = new Array<GraphNames>();
            return *names;
        }

        bool GraphAttributes(ElementTree& tree, ElementHandle e, StringView name, StringView value)
        {
            (void)tree;
            GraphNames* entry = nullptr;
            for (GraphNames& n : Names())
                if (n.element == e)
                    entry = &n;
            if (!entry)
            {
                GraphNames fresh;
                fresh.element = e;
                Names().Push(Move(fresh));
                entry = &Names().Back();
            }
            if (name == StringView("inspector"))
                entry->inspector = Intern(value);
            else if (name == StringView("status"))
                entry->status = Intern(value);
            else if (name == StringView("frame-insets"))
            {
                String text(value);
                const char* at = text.CStr();
                for (u32 k = 0; k < 4; k++)
                {
                    char* end = nullptr;
                    entry->insets[k] = std::strtof(at, &end);
                    if (end == at)
                        return false;
                    at = end;
                }
            }
            else if (name == StringView("select"))
                entry->select = String(value);
            else if (name == StringView("node-tab"))
                entry->nodeTab = Intern(value);
            else if (name == StringView("graph-tab"))
                entry->graphTab = Intern(value);
            else
                return false;
            return true;
        }

        void AttachGraph(EventSystem& events, ElementHandle e)
        {
            ElementTree& tree = events.tree();
            if (&tree != &World().tree())
                return;
            g::RegisterBuiltins();
            for (usize i = 0; i < Views().Size();)
                if (!tree.IsAlive(Views()[i]->element))
                {
                    delete Views()[i];
                    Views().RemoveAt(i);
                }
                else
                    i++;
            auto* v = new View();
            v->element = e;
            for (const GraphNames& n : Names())
                if (n.element == e)
                {
                    v->inspector = n.inspector;
                    v->status = n.status;
                    v->nodeTab = n.nodeTab;
                    v->graphTab = n.graphTab;
                    for (u32 k = 0; k < 4; k++)
                        v->insets[k] = n.insets[k];
                    // The node of that kind selected at the start.
                    for (const g::Node& node : g::TheGraph().nodes)
                        if (!n.select.Empty() && g::NodeKindAt(node.kind).name == n.select.View())
                            v->selected.Push(node.id);
                }
            Views().Push(v);
            tree.SetFocusable(e, true);
            tree.AddClass(e, Intern("graph-view"));
            v->content = Make(tree, e, "graph-content");
            v->overlay = tree.Create(Intern("SampleGraphOverlay"));
            tree.AddClass(v->overlay, Intern("graph-overlay"));
            tree.SetPicking(v->overlay, ui::PickingMode::Ignore);
            tree.Append(e, v->overlay);
            ApplyTransform(*v);
            Rebuild(*v, tree);
            UpdatePanels(*v, tree);
            const ui::EventTypes& t = ui::Events();
            events.RegisterCallback(e, t.pointerDown, &OnPointerDown, nullptr);
            events.RegisterCallback(e, t.pointerMove, &OnPointerMove, nullptr);
            events.RegisterCallback(e, t.pointerUp, &OnPointerUp, nullptr);
            events.RegisterCallback(e, t.wheel, &OnWheel, nullptr);
            events.RegisterCallback(e, t.keyDown, &OnKeyDown, nullptr);
            events.RegisterCallback(e, t.geometryChanged, &OnGeometry, nullptr);
        }

        bool PreviewAttributes(ElementTree&, ElementHandle e, StringView name, StringView value)
        {
            if (name != StringView("node"))
                return false;
            if (value == StringView("main") && !PreviewEntry(e))
            {
                PreviewOf entry;
                entry.element = e;
                entry.main = true;
                Previews().Push(Move(entry));
            }
            return true;
        }

        void AttachPreview(EventSystem& events, ElementHandle e)
        {
            if (&events.tree() != &World().tree())
                return;
            if (PreviewOf* entry = PreviewEntry(e); entry && !entry->texture.Get())
                BindPreview(*entry);
        }
    }

    void RegisterSampleGraph()
    {
        g::RegisterBuiltins();
        ui::ElementType graph;
        graph.name = Intern("SampleGraph");
        graph.attributes = &GraphAttributes;
        graph.paint = &PaintGraph;
        graph.attach = &AttachGraph;
        ui::RegisterElementType(graph);
        ui::RegisterElementType("SampleGraphOverlay", nullptr, &PaintOverlay);
        ui::RegisterElementType("SampleNodePreview", &PreviewAttributes, &PaintPreview, &AttachPreview);
    }
}
