#pragma once

#include <Helix/Core/Containers/Array.h>
#include <Helix/Core/Strings/String.h>
#include <Helix/Math/Color.h>
#include <Helix/Math/Vector.h>

// The node graph sample's model: port types and node kinds in registries, a graph of nodes and edges, evaluated per
// pixel on the CPU (the nodes' previews, the main preview). Every value is a Vector4; a port type says how many of its
// components count, and a value crossing from one type to another is converted as the reference's graphs do (a
// float fills every component, a vector gives its first ones).
namespace helix::samples::graph
{
    using Value = Vector4;

    // A port type: its name, the color of its ports and edges, the components it has.
    struct PortType
    {
        StringView name;
        Color color;
        u32 components = 1;
    };

    // The registered port types (Float, Vector2, Vector3, Vector4, Color: the defaults).
    u32 RegisterPortType(const PortType& type);
    const PortType& PortTypeAt(u32 index);
    u32 FindPortType(StringView name);
    Value Convert(Value v, u32 from, u32 to);

    // How an unconnected input shows its value beside the node.
    enum class InlineKind : u8
    {
        None,
        Values,
        Color,
        Channel
    };

    struct PortDef
    {
        StringView name;
        u32 type = 0;
        Value value;
        InlineKind inlineKind = InlineKind::Values;
    };

    struct Node;

    // What a node's evaluation sees: the point (uv in 0 to 1, y up), the time, its inputs (converted to its ports'
    // types), the node; it writes its outputs.
    struct EvalContext
    {
        Vector2 uv;
        f32 time = 0;
        const Value* inputs = nullptr;
        Value* outputs = nullptr;
        const Node* node = nullptr;
    };
    using NodeEval = void (*)(EvalContext& c);

    // A kind of node: its name and category (the create menu's), what it does, its ports, whether it shows a
    // preview, whether it is a context (the stack of blocks the graph ends in).
    struct NodeKind
    {
        StringView name, category, description;
        Array<PortDef> inputs, outputs;
        NodeEval eval = nullptr;
        bool preview = true;
        bool context = false;
    };

    // The color of a category in the editor's category color mode (by the category's first part; none: no color).
    void RegisterCategoryColor(StringView category, Color color);
    bool CategoryColor(StringView category, Color& color);

    u32 RegisterNodeKind(NodeKind kind);
    u32 NodeKindCount();
    const NodeKind& NodeKindAt(u32 index);
    u32 FindNodeKind(StringView name);

    struct Node
    {
        u32 id = 0;
        u32 kind = 0;
        Vector2 position;
        // A property node's name (shown as its title) and value.
        String title;
        Value property;
        Array<Value> inputValues;
        bool previewOpen = true;
    };

    struct Edge
    {
        u32 fromNode = 0, fromPort = 0, toNode = 0, toPort = 0;
    };

    struct Graph
    {
        Array<Node> nodes;
        Array<Edge> edges;
        u32 nextId = 1;
        // Changes since it was made (the views redraw the previews when it moves on).
        u32 version = 0;

        u32 Add(u32 kind, Vector2 position);
        Node* Find(u32 id);
        const Node* Find(u32 id) const;
        i32 IndexOf(u32 id) const;
        // The edge into an input (null when it has none).
        const Edge* Into(u32 node, u32 port) const;
        // Connects an output to an input (an input takes one edge: the old one goes); false when it would make a
        // cycle or the nodes are one.
        bool Connect(u32 fromNode, u32 fromPort, u32 toNode, u32 toPort);
        void Disconnect(u32 toNode, u32 toPort);
        void Remove(u32 node);
        bool Reaches(u32 from, u32 to) const;
    };

    // A port's type with its dynamic vectors resolved (the widest vector its node's dynamic inputs get).
    u32 ResolvedType(const Graph& graph, u32 node, bool output, u32 port);

    // Evaluates graphs for one output at many points: the nodes it needs in their order, evaluated once a point.
    class Evaluator
    {
    public:
        // The output of a node (or the input of a context block, input >= 0) to evaluate.
        void Prepare(const Graph& graph, u32 node, i32 output, i32 contextInput = -1);
        Value At(Vector2 uv, f32 time);
        // Every node at once (the nodes' previews): Run evaluates the graph at a point, Output reads a node's output
        // (Slot: where it is, for ValueAt).
        void PrepareAll(const Graph& graph);
        void Run(Vector2 uv, f32 time);
        Value Output(u32 node, u32 port) const;
        i32 Slot(u32 node, u32 port) const;
        Value ValueAt(i32 slot) const { return values_[usize(slot)]; }

    private:
        struct Step
        {
            const Node* node = nullptr;
            u32 firstValue = 0;
            Array<i32> inputSources;
            Array<u32> sourceTypes;
        };
        const Graph* graph_ = nullptr;
        Array<Step> steps_;
        Array<Value> values_;
        Array<Value> inputs_;
        i32 result_ = -1;
        u32 resultType_ = 0, wantedType_ = 0;
        Value constant_;
        bool constantResult_ = false;
    };

    // The kinds and types every graph of the sample knows (once).
    void RegisterBuiltins();
    // The sample's graph: animated water caustics into the Fragment context.
    void MakeSampleGraph(Graph& graph);
    // The graph the views show.
    Graph& TheGraph();
}
