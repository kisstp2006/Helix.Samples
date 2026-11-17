#include <Helix/Samples/Graph/GraphModel.h>

#include <cmath>
#include <initializer_list>

namespace helix::samples::graph
{
    namespace
    {
        Array<PortType>& Types()
        {
            static Array<PortType>* types = new Array<PortType>();
            return *types;
        }

        Array<NodeKind>& Kinds()
        {
            static Array<NodeKind>* kinds = new Array<NodeKind>();
            return *kinds;
        }

        f32 Fract(f32 x) { return x - std::floor(x); }
        f32 Clamp01(f32 x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
        f32 Lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }

        // Per component helpers over a Value.
        template <class F>
        Value Map(Value a, F f)
        {
            return Value(f(a.x), f(a.y), f(a.z), f(a.w));
        }

        template <class F>
        Value Map2(Value a, Value b, F f)
        {
            return Value(f(a.x, b.x), f(a.y, b.y), f(a.z, b.z), f(a.w, b.w));
        }

        // A hash of a cell to 0..1 (two values): integer mixing of the cell's coordinates.
        Vector2 Hash2(f32 x, f32 y)
        {
            u32 h = u32(i32(x)) * 0x8da6b343u ^ u32(i32(y)) * 0xd8163841u;
            h ^= h >> 15;
            h *= 0x2c1b3c6du;
            h ^= h >> 12;
            h *= 0x297a2d39u;
            h ^= h >> 15;
            u32 k = h * 0x9e3779b1u;
            k ^= k >> 16;
            return Vector2(f32(h & 0xffffu) / 65535.0f, f32(k & 0xffffu) / 65535.0f);
        }

        // The reference's gradient noise: a lattice of random gradients, smoothly blended (about 0..1).
        f32 GradientNoise(Vector2 p)
        {
            f32 ix = std::floor(p.x), iy = std::floor(p.y);
            f32 fx = p.x - ix, fy = p.y - iy;
            auto grad = [&](f32 cx, f32 cy, f32 dx, f32 dy) {
                Vector2 h = Hash2(cx, cy);
                f32 angle = h.x * 6.2831853f;
                return std::cos(angle) * dx + std::sin(angle) * dy;
            };
            f32 ux = fx * fx * fx * (fx * (fx * 6 - 15) + 10), uy = fy * fy * fy * (fy * (fy * 6 - 15) + 10);
            f32 a = grad(ix, iy, fx, fy), b = grad(ix + 1, iy, fx - 1, fy);
            f32 c = grad(ix, iy + 1, fx, fy - 1), d = grad(ix + 1, iy + 1, fx - 1, fy - 1);
            return Lerp(Lerp(a, b, ux), Lerp(c, d, ux), uy) + 0.5f;
        }

        // Voronoi as the reference's node: the distance to the nearest moving cell point and that cell's value.
        void Voronoi(Vector2 uv, f32 angleOffset, f32 density, f32& distance, f32& cell)
        {
            Vector2 p = uv * density;
            f32 ix = std::floor(p.x), iy = std::floor(p.y);
            distance = 8;
            cell = 0;
            for (i32 y = -1; y <= 1; y++)
                for (i32 x = -1; x <= 1; x++)
                {
                    f32 cx = ix + f32(x), cy = iy + f32(y);
                    Vector2 h = Hash2(cx, cy);
                    Vector2 offset(std::sin(h.y * angleOffset) * 0.5f + 0.5f, std::cos(h.x * angleOffset) * 0.5f + 0.5f);
                    f32 dx = cx + offset.x - p.x, dy = cy + offset.y - p.y;
                    f32 d = std::sqrt(dx * dx + dy * dy);
                    if (d < distance)
                    {
                        distance = d;
                        cell = h.x;
                    }
                }
        }

        Value V(f32 x, f32 y = 0, f32 z = 0, f32 w = 0) { return Value(x, y, z, w); }
        Value Rgb(f32 r, f32 g, f32 b) { return Value(r, g, b, 1); }

        PortDef In(StringView name, u32 type, Value value = Value(0, 0, 0, 0), InlineKind kind = InlineKind::Values)
        {
            PortDef p;
            p.name = name;
            p.type = type;
            p.value = value;
            p.inlineKind = kind;
            return p;
        }

        PortDef Out(StringView name, u32 type) { return In(name, type, Value(0, 0, 0, 0), InlineKind::None); }
    }

    // ------------------------------------------------------------------------------------------------- types

    u32 RegisterPortType(const PortType& type)
    {
        for (u32 i = 0; i < Types().Size(); i++)
            if (Types()[i].name == type.name)
            {
                Types()[i] = type;
                return i;
            }
        Types().Push(type);
        return u32(Types().Size() - 1);
    }

    const PortType& PortTypeAt(u32 index) { return Types()[index < Types().Size() ? index : 0]; }

    u32 FindPortType(StringView name)
    {
        for (u32 i = 0; i < Types().Size(); i++)
            if (Types()[i].name == name)
                return i;
        return 0;
    }

    Value Convert(Value v, u32 from, u32 to)
    {
        u32 f = PortTypeAt(from).components, t = PortTypeAt(to).components;
        if (f == t)
            return v;
        if (f == 1)
            return Value(v.x, v.x, v.x, v.x);
        if (f == 0 || t == 0)
            return v;
        f32 c[4] = {v.x, v.y, v.z, v.w};
        for (u32 i = f; i < 4; i++)
            c[i] = i == 3 ? 1.0f : 0.0f;
        return Value(c[0], t > 1 ? c[1] : 0, t > 2 ? c[2] : 0, t > 3 ? c[3] : 0);
    }

    // ------------------------------------------------------------------------------------------------- kinds

    namespace
    {
        struct CategoryEntry
        {
            String name;
            Color color;
        };

        Array<CategoryEntry>& CategoryColors()
        {
            static Array<CategoryEntry>* list = new Array<CategoryEntry>();
            return *list;
        }

        StringView TopOf(StringView category)
        {
            usize slash = category.Find('/');
            return slash == StringView::npos ? category : category.Substr(0, slash);
        }
    }

    void RegisterCategoryColor(StringView category, Color color)
    {
        for (CategoryEntry& e : CategoryColors())
            if (e.name.View() == category)
            {
                e.color = color;
                return;
            }
        CategoryColors().Push(CategoryEntry{String(category), color});
    }

    bool CategoryColor(StringView category, Color& color)
    {
        StringView top = TopOf(category);
        for (const CategoryEntry& e : CategoryColors())
            if (e.name.View() == top)
            {
                color = e.color;
                return true;
            }
        return false;
    }

    u32 RegisterNodeKind(NodeKind kind)
    {
        for (u32 i = 0; i < Kinds().Size(); i++)
            if (Kinds()[i].name == kind.name)
            {
                Kinds()[i] = Move(kind);
                return i;
            }
        Kinds().Push(Move(kind));
        return u32(Kinds().Size() - 1);
    }

    u32 NodeKindCount() { return u32(Kinds().Size()); }
    const NodeKind& NodeKindAt(u32 index) { return Kinds()[index]; }

    u32 FindNodeKind(StringView name)
    {
        for (u32 i = 0; i < Kinds().Size(); i++)
            if (Kinds()[i].name == name)
                return i;
        return ~0u;
    }

    // ------------------------------------------------------------------------------------------------- graph

    u32 Graph::Add(u32 kind, Vector2 position)
    {
        Node n;
        n.id = nextId++;
        n.kind = kind;
        n.position = position;
        for (const PortDef& p : NodeKindAt(kind).inputs)
            n.inputValues.Push(p.value);
        nodes.Push(Move(n));
        version++;
        return nodes.Back().id;
    }

    i32 Graph::IndexOf(u32 id) const
    {
        for (usize i = 0; i < nodes.Size(); i++)
            if (nodes[i].id == id)
                return i32(i);
        return -1;
    }

    Node* Graph::Find(u32 id)
    {
        i32 i = IndexOf(id);
        return i < 0 ? nullptr : &nodes[usize(i)];
    }

    const Node* Graph::Find(u32 id) const
    {
        i32 i = IndexOf(id);
        return i < 0 ? nullptr : &nodes[usize(i)];
    }

    const Edge* Graph::Into(u32 node, u32 port) const
    {
        for (const Edge& e : edges)
            if (e.toNode == node && e.toPort == port)
                return &e;
        return nullptr;
    }

    bool Graph::Reaches(u32 from, u32 to) const
    {
        if (from == to)
            return true;
        for (const Edge& e : edges)
            if (e.fromNode == from && Reaches(e.toNode, to))
                return true;
        return false;
    }

    bool Graph::Connect(u32 fromNode, u32 fromPort, u32 toNode, u32 toPort)
    {
        if (fromNode == toNode || !Find(fromNode) || !Find(toNode) || Reaches(toNode, fromNode))
            return false;
        Disconnect(toNode, toPort);
        edges.Push(Edge{fromNode, fromPort, toNode, toPort});
        version++;
        return true;
    }

    void Graph::Disconnect(u32 toNode, u32 toPort)
    {
        for (usize i = 0; i < edges.Size(); i++)
            if (edges[i].toNode == toNode && edges[i].toPort == toPort)
            {
                edges.RemoveAt(i);
                version++;
                return;
            }
    }

    void Graph::Remove(u32 node)
    {
        for (usize i = 0; i < edges.Size();)
            if (edges[i].fromNode == node || edges[i].toNode == node)
                edges.RemoveAt(i);
            else
                i++;
        i32 at = IndexOf(node);
        if (at >= 0)
            nodes.RemoveAt(usize(at));
        version++;
    }

    u32 ResolvedType(const Graph& graph, u32 node, bool output, u32 port)
    {
        const Node* n = graph.Find(node);
        if (!n)
            return 0;
        const NodeKind& kind = NodeKindAt(n->kind);
        const Array<PortDef>& ports = output ? kind.outputs : kind.inputs;
        if (port >= ports.Size())
            return 0;
        u32 type = ports[port].type;
        if (PortTypeAt(type).components != 0)
            return type;
        // A dynamic port: the widest vector its dynamic inputs get (a float without any).
        u32 best = 0, components = 1;
        for (u32 k = 0; k < kind.inputs.Size(); k++)
        {
            if (PortTypeAt(kind.inputs[k].type).components != 0)
                continue;
            if (const Edge* e = graph.Into(node, k))
            {
                u32 t = ResolvedType(graph, e->fromNode, true, e->fromPort);
                if (PortTypeAt(t).components > components)
                {
                    components = PortTypeAt(t).components;
                    best = t;
                }
            }
        }
        return best;
    }

    // ------------------------------------------------------------------------------------------------- evaluator

    void Evaluator::Prepare(const Graph& graph, u32 node, i32 output, i32 contextInput)
    {
        graph_ = &graph;
        steps_.Clear();
        values_.Clear();
        result_ = -1;
        constantResult_ = false;
        // The nodes the result needs, each after its inputs' nodes.
        Array<u32> order;
        auto visit = [&](auto& self, u32 id) -> void {
            for (u32 o : order)
                if (o == id)
                    return;
            for (const Edge& e : graph.edges)
                if (e.toNode == id)
                    self(self, e.fromNode);
            order.Push(id);
        };
        u32 target = node;
        i32 targetOutput = output;
        const Node* n = graph.Find(node);
        if (!n)
            return;
        if (contextInput >= 0)
        {
            // A context block: what feeds it, else its own value.
            wantedType_ = NodeKindAt(n->kind).inputs[usize(contextInput)].type;
            const Edge* e = graph.Into(node, u32(contextInput));
            if (!e)
            {
                constant_ = n->inputValues[usize(contextInput)];
                constantResult_ = true;
                return;
            }
            target = e->fromNode;
            targetOutput = i32(e->fromPort);
        }
        else
            wantedType_ = 0;
        visit(visit, target);
        // Each node's outputs in values_, its inputs' sources found.
        Array<u32> first;
        for (u32 id : order)
        {
            const Node* x = graph.Find(id);
            first.Push(u32(values_.Size()));
            for (usize k = 0; k < NodeKindAt(x->kind).outputs.Size(); k++)
                values_.Push(Value(0, 0, 0, 0));
        }
        for (usize i = 0; i < order.Size(); i++)
        {
            Step s;
            s.node = graph.Find(order[i]);
            s.firstValue = first[i];
            const NodeKind& kind = NodeKindAt(s.node->kind);
            for (usize k = 0; k < kind.inputs.Size(); k++)
            {
                i32 source = -1;
                u32 type = kind.inputs[k].type;
                if (const Edge* e = graph.Into(order[i], u32(k)))
                    for (usize j = 0; j < order.Size(); j++)
                        if (order[j] == e->fromNode)
                        {
                            source = i32(first[j] + e->fromPort);
                            type = NodeKindAt(graph.Find(e->fromNode)->kind).outputs[e->fromPort].type;
                        }
                s.inputSources.Push(source);
                s.sourceTypes.Push(type);
            }
            steps_.Push(Move(s));
            if (order[i] == target)
            {
                result_ = i32(first[i]) + targetOutput;
                resultType_ = NodeKindAt(graph.Find(target)->kind).outputs[usize(targetOutput)].type;
            }
        }
        if (contextInput < 0)
            wantedType_ = resultType_;
    }

    void Evaluator::PrepareAll(const Graph& graph)
    {
        graph_ = &graph;
        steps_.Clear();
        values_.Clear();
        constantResult_ = false;
        result_ = -1;
        // Every node, each after its inputs' nodes.
        Array<u32> order;
        auto visit = [&](auto& self, u32 id) -> void {
            for (u32 o : order)
                if (o == id)
                    return;
            for (const Edge& e : graph.edges)
                if (e.toNode == id)
                    self(self, e.fromNode);
            order.Push(id);
        };
        for (const Node& n : graph.nodes)
            visit(visit, n.id);
        Array<u32> first;
        for (u32 id : order)
        {
            first.Push(u32(values_.Size()));
            for (usize k = 0; k < NodeKindAt(graph.Find(id)->kind).outputs.Size(); k++)
                values_.Push(Value(0, 0, 0, 0));
        }
        for (usize i = 0; i < order.Size(); i++)
        {
            Step s;
            s.node = graph.Find(order[i]);
            s.firstValue = first[i];
            const NodeKind& kind = NodeKindAt(s.node->kind);
            for (usize k = 0; k < kind.inputs.Size(); k++)
            {
                i32 source = -1;
                u32 type = kind.inputs[k].type;
                if (const Edge* e = graph.Into(order[i], u32(k)))
                    for (usize j = 0; j < order.Size(); j++)
                        if (order[j] == e->fromNode)
                        {
                            source = i32(first[j] + e->fromPort);
                            type = NodeKindAt(graph.Find(e->fromNode)->kind).outputs[e->fromPort].type;
                        }
                s.inputSources.Push(source);
                s.sourceTypes.Push(type);
            }
            steps_.Push(Move(s));
        }
    }

    void Evaluator::Run(Vector2 uv, f32 time)
    {
        for (Step& s : steps_)
        {
            const NodeKind& kind = NodeKindAt(s.node->kind);
            if (!kind.eval)
                continue;
            inputs_.Clear();
            for (usize k = 0; k < kind.inputs.Size(); k++)
            {
                const PortDef& port = kind.inputs[k];
                if (s.inputSources[k] >= 0)
                    inputs_.Push(Convert(values_[usize(s.inputSources[k])], s.sourceTypes[k], port.type));
                else if (port.inlineKind == InlineKind::Channel)
                    inputs_.Push(Value(uv.x, uv.y, 0, 1));
                else
                    inputs_.Push(s.node->inputValues[k]);
            }
            EvalContext c;
            c.uv = uv;
            c.time = time;
            c.inputs = inputs_.Data();
            c.outputs = values_.Data() + s.firstValue;
            c.node = s.node;
            kind.eval(c);
        }
    }

    Value Evaluator::Output(u32 node, u32 port) const
    {
        for (const Step& s : steps_)
            if (s.node->id == node)
                return values_[usize(s.firstValue + port)];
        return Value(0, 0, 0, 1);
    }

    i32 Evaluator::Slot(u32 node, u32 port) const
    {
        for (const Step& s : steps_)
            if (s.node->id == node)
                return i32(s.firstValue + port);
        return -1;
    }

    Value Evaluator::At(Vector2 uv, f32 time)
    {
        if (constantResult_)
            return constant_;
        if (result_ < 0)
            return Value(0, 0, 0, 1);
        for (Step& s : steps_)
        {
            const NodeKind& kind = NodeKindAt(s.node->kind);
            inputs_.Clear();
            for (usize k = 0; k < kind.inputs.Size(); k++)
            {
                const PortDef& port = kind.inputs[k];
                if (s.inputSources[k] >= 0)
                    inputs_.Push(Convert(values_[usize(s.inputSources[k])], s.sourceTypes[k], port.type));
                else if (port.inlineKind == InlineKind::Channel)
                    inputs_.Push(Value(uv.x, uv.y, 0, 1));
                else
                    inputs_.Push(s.node->inputValues[k]);
            }
            EvalContext c;
            c.uv = uv;
            c.time = time;
            c.inputs = inputs_.Data();
            c.outputs = values_.Data() + s.firstValue;
            c.node = s.node;
            if (kind.eval)
                kind.eval(c);
        }
        return Convert(values_[usize(result_)], resultType_, wantedType_);
    }

    // ------------------------------------------------------------------------------------------------- builtins

    void RegisterBuiltins()
    {
        static bool done = false;
        if (done)
            return;
        done = true;
        u32 f1 = RegisterPortType({"Float", Color(0.518f, 0.894f, 0.906f, 1), 1});
        u32 f2 = RegisterPortType({"Vector2", Color(0.604f, 0.937f, 0.573f, 1), 2});
        u32 f3 = RegisterPortType({"Vector3", Color(0.965f, 1.0f, 0.604f, 1), 3});
        u32 f4 = RegisterPortType({"Vector4", Color(0.984f, 0.796f, 0.957f, 1), 4});
        // Dynamic vectors take any vector; their values stay as they come (four components).
        u32 dv = RegisterPortType({"Dynamic", Color(0.518f, 0.894f, 0.906f, 1), 0});

        auto kind = [](StringView name, StringView category, StringView description, std::initializer_list<PortDef> inputs,
                       std::initializer_list<PortDef> outputs, NodeEval eval, bool preview = true) {
            NodeKind k;
            k.name = name;
            k.category = category;
            k.description = description;
            for (const PortDef& p : inputs)
                k.inputs.Push(p);
            for (const PortDef& p : outputs)
                k.outputs.Push(p);
            k.eval = eval;
            k.preview = preview;
            RegisterNodeKind(Move(k));
        };

        // The category color mode's colors.
        RegisterCategoryColor("Input", Color(0.796f, 0.200f, 0.169f, 1));
        RegisterCategoryColor("Math", Color(0.259f, 0.522f, 0.957f, 1));
        RegisterCategoryColor("Procedural", Color(0.612f, 0.310f, 1.0f, 1));
        RegisterCategoryColor("UV", Color(0.031f, 0.843f, 0.545f, 1));
        RegisterCategoryColor("Channel", Color(0.592f, 0.820f, 0.239f, 1));
        RegisterCategoryColor("Artistic", Color(0.957f, 0.494f, 0.196f, 1));
        RegisterCategoryColor("Utility", Color(0.420f, 0.420f, 0.420f, 1));

        // Input.
        kind("UV", "Input/Geometry", "The mesh's texture coordinates.", {}, {Out("Out", f4)},
             [](EvalContext& c) { c.outputs[0] = V(c.uv.x, c.uv.y, 0, 1); });
        kind("Time", "Input/Basic", "Time in seconds, and its sine and cosine.", {},
             {Out("Time", f1), Out("Sine Time", f1), Out("Cosine Time", f1)}, [](EvalContext& c) {
                 c.outputs[0] = V(c.time);
                 c.outputs[1] = V(std::sin(c.time));
                 c.outputs[2] = V(std::cos(c.time));
             }, false);
        kind("Float", "Input/Basic", "A constant value.", {In("X", f1, V(1))}, {Out("Out", f1)},
             [](EvalContext& c) { c.outputs[0] = c.inputs[0]; }, false);
        kind("Color", "Input/Basic", "A constant color.", {In("", f4, Rgb(1, 1, 1), InlineKind::Color)}, {Out("Out", f4)},
             [](EvalContext& c) { c.outputs[0] = c.inputs[0]; }, false);
        kind("Float Property", "Properties", "A property of the graph's material.", {}, {Out("", f1)},
             [](EvalContext& c) { c.outputs[0] = c.node->property; }, false);
        kind("Color Property", "Properties", "A property of the graph's material.", {}, {Out("", f4)},
             [](EvalContext& c) { c.outputs[0] = c.node->property; }, false);

        // Math.
        kind("Add", "Math/Basic", "A plus B.", {In("A", dv), In("B", dv, V(1, 1, 1, 1))}, {Out("Out", dv)},
             [](EvalContext& c) { c.outputs[0] = c.inputs[0] + c.inputs[1]; });
        kind("Subtract", "Math/Basic", "A minus B.", {In("A", dv, V(1, 1, 1, 1)), In("B", dv, V(1, 1, 1, 1))}, {Out("Out", dv)},
             [](EvalContext& c) { c.outputs[0] = c.inputs[0] - c.inputs[1]; });
        kind("Multiply", "Math/Basic", "A times B.", {In("A", dv), In("B", dv, V(2, 2, 2, 2))}, {Out("Out", dv)},
             [](EvalContext& c) { c.outputs[0] = Map2(c.inputs[0], c.inputs[1], [](f32 a, f32 b) { return a * b; }); });
        kind("Divide", "Math/Basic", "A over B.", {In("A", dv), In("B", dv, V(2, 2, 2, 2))}, {Out("Out", dv)},
             [](EvalContext& c) { c.outputs[0] = Map2(c.inputs[0], c.inputs[1], [](f32 a, f32 b) { return b != 0 ? a / b : 0; }); });
        kind("Power", "Math/Basic", "A to the power of B.", {In("A", dv), In("B", dv, V(2, 2, 2, 2))}, {Out("Out", dv)},
             [](EvalContext& c) { c.outputs[0] = Map2(c.inputs[0], c.inputs[1], [](f32 a, f32 b) { return std::pow(a < 0 ? 0 : a, b); }); });
        kind("Sine", "Math/Trigonometry", "The sine of the input.", {In("In", dv)}, {Out("Out", dv)},
             [](EvalContext& c) { c.outputs[0] = Map(c.inputs[0], [](f32 a) { return std::sin(a); }); });
        kind("One Minus", "Math/Range", "One minus the input.", {In("In", dv)}, {Out("Out", dv)},
             [](EvalContext& c) { c.outputs[0] = Map(c.inputs[0], [](f32 a) { return 1 - a; }); });
        kind("Saturate", "Math/Range", "The input clamped to 0..1.", {In("In", dv)}, {Out("Out", dv)},
             [](EvalContext& c) { c.outputs[0] = Map(c.inputs[0], Clamp01); });
        kind("Fraction", "Math/Range", "The input's fractional part.", {In("In", dv)}, {Out("Out", dv)},
             [](EvalContext& c) { c.outputs[0] = Map(c.inputs[0], Fract); });
        kind("Lerp", "Math/Interpolation", "From A to B by T.", {In("A", dv), In("B", dv, V(1, 1, 1, 1)), In("T", dv, V(0.5f, 0.5f, 0.5f, 0.5f))},
             {Out("Out", dv)}, [](EvalContext& c) {
                 Value t = c.inputs[2];
                 c.outputs[0] = Value(Lerp(c.inputs[0].x, c.inputs[1].x, t.x), Lerp(c.inputs[0].y, c.inputs[1].y, t.y),
                                      Lerp(c.inputs[0].z, c.inputs[1].z, t.z), Lerp(c.inputs[0].w, c.inputs[1].w, t.w));
             });
        kind("Smoothstep", "Math/Interpolation", "A smooth step of In between the edges.",
             {In("Edge1", dv), In("Edge2", dv, V(1, 1, 1, 1)), In("In", dv)}, {Out("Out", dv)}, [](EvalContext& c) {
                 auto s = [](f32 a, f32 b, f32 x) {
                     f32 t = Clamp01(b != a ? (x - a) / (b - a) : 0);
                     return t * t * (3 - 2 * t);
                 };
                 Value a = c.inputs[0], b = c.inputs[1], x = c.inputs[2];
                 c.outputs[0] = Value(s(a.x, b.x, x.x), s(a.y, b.y, x.y), s(a.z, b.z, x.z), s(a.w, b.w, x.w));
             });
        kind("Distance", "Math/Vector", "The distance between A and B.", {In("A", f2), In("B", f2, V(0.5f, 0.5f))}, {Out("Out", f1)},
             [](EvalContext& c) {
                 f32 dx = c.inputs[0].x - c.inputs[1].x, dy = c.inputs[0].y - c.inputs[1].y;
                 c.outputs[0] = V(std::sqrt(dx * dx + dy * dy));
             });

        // Procedural.
        kind("Gradient Noise", "Procedural/Noise", "Smooth noise of random gradients.", {In("UV", f2, V(0, 0), InlineKind::Channel), In("Scale", f1, V(10))},
             {Out("Out", f1)}, [](EvalContext& c) {
                 c.outputs[0] = V(GradientNoise(Vector2(c.inputs[0].x, c.inputs[0].y) * c.inputs[1].x));
             });
        kind("Voronoi", "Procedural/Noise", "Distances to moving cell points.",
             {In("UV", f2, V(0, 0), InlineKind::Channel), In("Angle Offset", f1, V(2)), In("Cell Density", f1, V(5))},
             {Out("Out", f1), Out("Cells", f1)}, [](EvalContext& c) {
                 f32 d = 0, cell = 0;
                 Voronoi(Vector2(c.inputs[0].x, c.inputs[0].y), c.inputs[1].x, c.inputs[2].x, d, cell);
                 c.outputs[0] = V(d);
                 c.outputs[1] = V(cell);
             });

        // UV.
        kind("Tiling And Offset", "UV", "The UV scaled by Tiling and moved by Offset.",
             {In("UV", f2, V(0, 0), InlineKind::Channel), In("Tiling", f2, V(1, 1)), In("Offset", f2, V(0, 0))}, {Out("Out", f2)},
             [](EvalContext& c) {
                 c.outputs[0] = V(c.inputs[0].x * c.inputs[1].x + c.inputs[2].x, c.inputs[0].y * c.inputs[1].y + c.inputs[2].y);
             });
        kind("Polar Coordinates", "UV", "The UV as distance and angle around a center.",
             {In("UV", f2, V(0, 0), InlineKind::Channel), In("Center", f2, V(0.5f, 0.5f)), In("Radial Scale", f1, V(1)), In("Length Scale", f1, V(1))},
             {Out("Out", f2)}, [](EvalContext& c) {
                 f32 dx = c.inputs[0].x - c.inputs[1].x, dy = c.inputs[0].y - c.inputs[1].y;
                 f32 r = std::sqrt(dx * dx + dy * dy) * 2 * c.inputs[2].x;
                 f32 a = std::atan2(dx, dy) * 0.15915494f * c.inputs[3].x;
                 c.outputs[0] = V(r, a);
             });

        // Channel.
        kind("Split", "Channel", "The input's components.", {In("In", f4)}, {Out("R", f1), Out("G", f1), Out("B", f1), Out("A", f1)},
             [](EvalContext& c) {
                 c.outputs[0] = V(c.inputs[0].x);
                 c.outputs[1] = V(c.inputs[0].y);
                 c.outputs[2] = V(c.inputs[0].z);
                 c.outputs[3] = V(c.inputs[0].w);
             }, false);
        kind("Combine", "Channel", "Components into vectors.", {In("R", f1), In("G", f1), In("B", f1), In("A", f1)},
             {Out("RGBA", f4), Out("RGB", f3), Out("RG", f2)}, [](EvalContext& c) {
                 Value v(c.inputs[0].x, c.inputs[1].x, c.inputs[2].x, c.inputs[3].x);
                 c.outputs[0] = v;
                 c.outputs[1] = V(v.x, v.y, v.z);
                 c.outputs[2] = V(v.x, v.y);
             });

        // The contexts the graph ends in: their blocks are inputs.
        NodeKind vertex;
        vertex.name = "Vertex";
        vertex.category = "";
        vertex.description = "What the vertex stage writes.";
        vertex.inputs.Push(In("Position", f3, V(0, 0, 0), InlineKind::Channel));
        vertex.inputs.Push(In("Normal", f3, V(0, 0, 1), InlineKind::Channel));
        vertex.inputs.Push(In("Tangent", f3, V(1, 0, 0), InlineKind::Channel));
        vertex.preview = false;
        vertex.context = true;
        RegisterNodeKind(Move(vertex));
        NodeKind fragment;
        fragment.name = "Fragment";
        fragment.category = "";
        fragment.description = "What the fragment stage writes.";
        fragment.inputs.Push(In("Base Color", f3, Rgb(0.5f, 0.5f, 0.5f), InlineKind::Color));
        fragment.inputs.Push(In("Normal (Tangent Space)", f3, V(0, 0, 1), InlineKind::Channel));
        fragment.inputs.Push(In("Metallic", f1, V(0)));
        fragment.inputs.Push(In("Smoothness", f1, V(0.5f)));
        fragment.inputs.Push(In("Emission", f3, Rgb(0, 0, 0), InlineKind::Color));
        fragment.inputs.Push(In("Alpha", f1, V(1)));
        fragment.preview = false;
        fragment.context = true;
        RegisterNodeKind(Move(fragment));
    }

    void MakeSampleGraph(Graph& g)
    {
        RegisterBuiltins();
        auto add = [&](StringView kind, f32 x, f32 y) { return g.Add(FindNodeKind(kind), Vector2(x, y)); };
        auto property = [&](StringView kind, StringView title, Value value, f32 x, f32 y) {
            u32 id = add(kind, x, y);
            g.Find(id)->title = String(title);
            g.Find(id)->property = value;
            return id;
        };
        auto set = [&](u32 id, u32 input, Value v) { g.Find(id)->inputValues[input] = v; };

        u32 time = add("Time", 0, 0);
        u32 speed = property("Float Property", "Speed", V(0.6f), 34, 110);
        u32 uv = add("UV", 0, 160);
        u32 multiply = add("Multiply", 210, 0);
        u32 tiling = add("Tiling And Offset", 260, 170);
        set(tiling, 1, V(1, 1));
        u32 voronoi = add("Voronoi", 480, 0);
        set(voronoi, 2, V(5));
        u32 noise = add("Gradient Noise", 480, 290);
        set(noise, 1, V(4));
        u32 power = add("Power", 720, 0);
        set(power, 1, V(3, 3, 3, 3));
        u32 add2 = add("Add", 720, 260);
        u32 deep = property("Color Property", "Deep Color", Rgb(0.02f, 0.17f, 0.33f), 730, 520);
        u32 foam = property("Color Property", "Foam Color", Rgb(0.62f, 0.93f, 1.0f), 730, 562);
        u32 lerp = add("Lerp", 960, 200);
        u32 vertex = add("Vertex", 1200, -20);
        u32 fragment = add("Fragment", 1200, 140);
        set(fragment, 3, V(0.85f));
        g.Find(multiply)->previewOpen = false;
        g.Find(tiling)->previewOpen = false;

        g.Connect(time, 0, multiply, 0);
        g.Connect(speed, 0, multiply, 1);
        g.Connect(uv, 0, tiling, 0);
        g.Connect(multiply, 0, tiling, 2);
        g.Connect(tiling, 0, voronoi, 0);
        g.Connect(multiply, 0, voronoi, 1);
        g.Connect(tiling, 0, noise, 0);
        g.Connect(voronoi, 0, power, 0);
        g.Connect(power, 0, add2, 0);
        g.Connect(noise, 0, add2, 1);
        g.Connect(deep, 0, lerp, 0);
        g.Connect(foam, 0, lerp, 1);
        g.Connect(add2, 0, lerp, 2);
        g.Connect(lerp, 0, fragment, 0);
        (void)vertex;
    }

    Graph& TheGraph()
    {
        static Graph* graph = [] {
            auto* g = new Graph();
            MakeSampleGraph(*g);
            return g;
        }();
        return *graph;
    }
}
