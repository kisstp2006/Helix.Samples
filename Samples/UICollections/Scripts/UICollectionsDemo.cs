using System;
using System.Collections.Generic;
using System.Linq;
using HelixEngine;
using HelixEngine.UI;
using HelixEngine.UI.Experimental;

namespace HelixSamples
{
    // The collections sample's script: a list of many items (only the rows in view made), a tree, a sortable table,
    // menus (a dropdown menu, a contextual menu on the list's rows), animations, and a chart painted by
    // generateVisualContent with a vector image beside it.
    public class UICollectionsDemo : GameBehaviour
    {
        sealed class Asset
        {
            public string name;
            public string kind;
            public int size;
        }

        readonly List<string> m_Items = new List<string>();
        readonly List<Asset> m_Assets = new List<Asset>();
        float[] m_Values = { 0.35f, 0.8f, 0.55f, 0.95f, 0.4f, 0.7f, 0.6f };
        float m_Grow = 1;
        int m_Shuffles;
        Label m_Status;
        ListView m_List;
        Element m_Chart, m_Donut;
        int m_Hovered = -1;
        VectorImage m_Badge;

        void Start()
        {
            Element root = GetComponent<UIView>().rootElement;
            m_Status = root.Find<Label>("status");

            // A list of 500 items: rows made for the ones in view and bound as it scrolls.
            for (int i = 0; i < 500; i++)
                m_Items.Add($"Item {i + 1:000}");
            m_List = root.Find<ListView>("inventory");
            if (m_List != null)
            {
                m_List.makeItem = () =>
                {
                    var label = new Label();
                    label.AddManipulator(new ContextualMenuManipulator(e =>
                    {
                        e.menu.AppendAction("Rename", _ => Status($"Rename {label.text}"));
                        e.menu.AppendAction("Duplicate", _ => Status($"Duplicate {label.text}"));
                        e.menu.AppendSeparator();
                        e.menu.AppendAction("Delete", _ => Status($"Delete {label.text}"), DropdownMenuAction.Status.Disabled);
                    }));
                    return label;
                };
                m_List.bindItem = (e, i) => ((Label)e).text = m_Items[i] ?? "(new)";
                m_List.itemsSource = m_Items;
                m_List.selectionChanged += items => Status($"Selected: {string.Join(", ", items.Take(4))}{(items.Count() > 4 ? "..." : "")}");
                m_List.itemsChosen += items => Status($"Chosen: {items.FirstOrDefault()}");
                m_List.itemIndexChanged += (from, to) => Status($"Moved row {from + 1} to {to + 1}");
            }

            // A tree of a scene's objects, its roots expanded.
            var tree = root.Find<TreeView>("scene");
            if (tree != null)
            {
                int id = 0;
                TreeViewItemData<string> Node(string name, params TreeViewItemData<string>[] children) =>
                    new TreeViewItemData<string>(++id, name, children.Length > 0 ? children.ToList() : null);
                var items = new List<TreeViewItemData<string>>
                {
                    Node("Main Camera"),
                    Node("Environment", Node("Terrain"), Node("Trees", Node("Oak"), Node("Pine"), Node("Birch")), Node("Water")),
                    Node("Player", Node("Model"), Node("Weapon"), Node("Camera Rig", Node("Follow Target"))),
                    Node("UI"),
                };
                tree.makeItem = () => new Label();
                tree.bindItem = (e, i) => ((Label)e).text = tree.GetItemDataForIndex<string>(i);
                tree.SetRootItems(items);
                tree.ExpandItem(2);
                tree.ExpandItem(7);
                tree.selectionChanged += s => Status($"Tree: {s.FirstOrDefault()}");
            }

            // A table of assets: three columns, sorted by size.
            string[] kinds = { "Texture", "Mesh", "Material", "Audio", "Script" };
            var random = new System.Random(7);
            for (int i = 0; i < 40; i++)
                m_Assets.Add(new Asset { name = $"{kinds[i % kinds.Length].ToLowerInvariant()}_{i:00}", kind = kinds[i % kinds.Length], size = random.Next(4, 4096) });
            var table = root.Find<MultiColumnListView>("assets");
            if (table != null)
            {
                table.columns.Add(new Column { name = "name", title = "Name", width = 130, stretchable = true,
                                               bindCell = (e, i) => ((Label)e).text = m_Assets[i].name,
                                               comparison = (a, b) => string.CompareOrdinal(m_Assets[a].name, m_Assets[b].name) });
                table.columns.Add(new Column { name = "kind", title = "Kind", width = 80,
                                               bindCell = (e, i) => ((Label)e).text = m_Assets[i].kind,
                                               comparison = (a, b) => string.CompareOrdinal(m_Assets[a].kind, m_Assets[b].kind) });
                table.columns.Add(new Column { name = "size", title = "Size", width = 70, optional = true,
                                               bindCell = (e, i) => ((Label)e).text = $"{m_Assets[i].size} KB",
                                               comparison = (a, b) => m_Assets[a].size.CompareTo(m_Assets[b].size) });
                table.itemsSource = m_Assets;
                table.sortColumnDescriptions.Add(new SortColumnDescription("size", SortDirection.Descending));
                table.columnSortingChanged += () => Status($"Sorted by {string.Join(", ", table.sortedColumns.Select(c => $"{c.columnName} {c.direction}"))}");
            }

            // A dropdown menu of actions under its button.
            var menuButton = root.Find<Button>("menu");
            if (menuButton != null)
                menuButton.clicked += () =>
                {
                    var menu = new GenericDropdownMenu();
                    menu.AddItem("Select all", false, () => m_List?.SetSelection(Enumerable.Range(0, m_Items.Count)));
                    menu.AddItem("Clear selection", false, () => m_List?.ClearSelection());
                    menu.AddSeparator("");
                    menu.AddItem("Scroll to the end", false, () => m_List?.ScrollToItem(m_Items.Count - 1));
                    menu.AddDisabledItem("Export", false);
                    menu.DropDown(menuButton.worldBound, menuButton, DropdownMenuSizeMode.Auto);
                };

            // The chart: its bars grow in with an animation, a button runs it again, another changes the values.
            m_Chart = root.Find("chart");
            m_Badge = ScriptableObject.CreateInstance<VectorImage>();
            using (var painter = new Painter2D())
            {
                painter.fillColor = new Color(1f, 0.78f, 0.3f);
                painter.BeginPath();
                for (int i = 0; i < 10; i++)
                {
                    float angle = Mathf.PI * 2 * i / 10 - Mathf.PI / 2;
                    float r = i % 2 == 0 ? 14 : 6;
                    var p = new Vector2(14 + Mathf.Cos(angle) * r, 14 + Mathf.Sin(angle) * r);
                    if (i == 0)
                        painter.MoveTo(p);
                    else
                        painter.LineTo(p);
                }
                painter.ClosePath();
                painter.Fill();
                painter.SaveToVectorImage(m_Badge);
            }
            if (m_Chart != null)
            {
                m_Chart.generateVisualContent = Paint;
                // The bar under the pointer is lit and shows its value.
                m_Chart.RegisterCallback<PointerMoveEvent>(e =>
                {
                    Rect r = m_Chart.contentRect;
                    float x = e.localPosition.x - 26, w = r.width - 10 - 26;
                    int hovered = x >= 0 && x < w ? (int)(x / (w / m_Values.Length)) : -1;
                    if (hovered != m_Hovered)
                    {
                        m_Hovered = hovered;
                        m_Chart.MarkDirtyRepaint();
                    }
                });
                m_Chart.RegisterCallback<PointerLeaveEvent>(_ =>
                {
                    m_Hovered = -1;
                    m_Chart.MarkDirtyRepaint();
                });
            }
            m_Donut = root.Find("donut");
            if (m_Donut != null)
                m_Donut.generateVisualContent = PaintDonut;
            root.Find<Button>("animate")?.RegisterCallback<ClickEvent>(_ => Grow());
            root.Find<Button>("shuffle")?.RegisterCallback<ClickEvent>(_ =>
            {
                var r = new System.Random(++m_Shuffles);
                m_Values = m_Values.Select(_ => 0.2f + (float)r.NextDouble() * 0.8f).ToArray();
                Grow();
            });

            // The meter's width follows the animation of style values.
            var fill = root.Find("meter-fill");
            var meterValue = root.Find<Label>("meter-value");
            root.Find<Button>("animate")?.RegisterCallback<ClickEvent>(_ =>
                fill?.experimental.animation.Start(new StyleValues { width = 0 }, new StyleValues { width = 177 }, 900).Ease(Easing.OutBounce)
                    .OnCompleted(() => { if (meterValue != null) meterValue.text = "72 %"; }));
        }

        void Grow()
        {
            m_Chart?.experimental.animation.Start(0f, 1f, 700, (e, v) =>
            {
                m_Grow = v;
                e.MarkDirtyRepaint();
                m_Donut?.MarkDirtyRepaint();
            }).Ease(Easing.OutCubic).OnCompleted(() => Status("The animation ended"));
        }

        static readonly string[] s_Stages = { "Cull", "Shadow", "Depth", "Opaque", "Sky", "Alpha", "Post" };
        static readonly Color s_Text = new Color(0.6f, 0.62f, 0.68f), s_Bright = new Color(0.88f, 0.9f, 0.94f);
        static readonly Color s_Line = new Color(1f, 0.78f, 0.3f);
        const float kScale = 4;                       // a value of 1 is 4 ms

        static Color BarColor(int i, int count) => Color.Lerp(new Color(0.33f, 0.6f, 1f), new Color(0.38f, 0.88f, 0.62f), i / (float)Mathf.Max(1, count - 1));

        static Color32 C32(Color c, float alpha) => new Color32((byte)(c.r * 255), (byte)(c.g * 255), (byte)(c.b * 255), (byte)(Mathf.Clamp01(alpha) * 255));

        // A rough width of a text in the panel's font (no measuring during the paint): for centering labels.
        static float TextWidth(string text, float size) => text.Length * size * 0.52f;

        // The chart: a grid with its milliseconds, bars of a vertical gradient (a mesh of their own), a smooth line
        // through the bars' tops over a faint area, its points, the stages' names, the hovered bar's value.
        void Paint(MeshGenerationContext ctx)
        {
            Rect box = ctx.element.contentRect;
            Painter2D p = ctx.painter2D;
            float left = 26, right = box.width - 10, top = 42, bottom = box.height - 22;
            float width = right - left, height = bottom - top;
            int n = m_Values.Length;
            float step = width / n;
            float total = m_Values.Sum() * kScale;

            // The grid: four lines, 0 to 4 ms.
            p.lineWidth = 1;
            for (int i = 0; i <= 4; i++)
            {
                float y = Mathf.Round(bottom - height * i / 4) + 0.5f;
                p.strokeColor = new Color(1, 1, 1, i == 0 ? 0.22f : 0.07f);
                p.BeginPath();
                p.MoveTo(new Vector2(left, y));
                p.LineTo(new Vector2(right, y));
                p.Stroke();
            }

            // The bars: a quad each, bright at the top, fading to the floor.
            var tops = new List<Vector2>();
            for (int i = 0; i < n; i++)
            {
                float h = height * m_Values[i] * m_Grow;
                float w = step * 0.56f, x = left + step * i + (step - w) / 2, y = bottom - h;
                Color c = BarColor(i, n);
                bool hovered = i == m_Hovered;
                float glow = hovered ? 1f : 0.85f;
                MeshWriteData mesh = ctx.Allocate(4, 6);
                mesh.SetNextVertex(new Vertex { position = new Vector3(x, y, Vertex.nearZ), tint = C32(c * glow, 1) });
                mesh.SetNextVertex(new Vertex { position = new Vector3(x + w, y, Vertex.nearZ), tint = C32(c * glow, 1) });
                mesh.SetNextVertex(new Vertex { position = new Vector3(x + w, bottom, Vertex.nearZ), tint = C32(c * glow, 0.18f) });
                mesh.SetNextVertex(new Vertex { position = new Vector3(x, bottom, Vertex.nearZ), tint = C32(c * glow, 0.18f) });
                foreach (ushort k in new ushort[] { 0, 1, 2, 0, 2, 3 })
                    mesh.SetNextIndex(k);
                tops.Add(new Vector2(x + w / 2, y));
            }

            // The texts (they also put the bars down, under what follows).
            ctx.DrawText("Frame time (ms)", new Vector2(10, 10), 12, s_Bright);
            string sum = $"{total:0.0} ms";
            ctx.DrawText(sum, new Vector2(right - 30 - TextWidth(sum, 12), 10), 12, s_Line);
            for (int i = 0; i <= 4; i++)
                ctx.DrawText($"{i}", new Vector2(left - 12, bottom - height * i / 4 - 7), 10, s_Text);
            for (int i = 0; i < n; i++)
            {
                string name = s_Stages[i % s_Stages.Length];
                float x = left + step * i + step / 2;
                ctx.DrawText(name, new Vector2(x - TextWidth(name, 8.5f) / 2, bottom + 5), 8.5f, i == m_Hovered ? s_Bright : s_Text);
            }

            // Each bar's top: a bright cap.
            p.lineCap = LineCap.Round;
            p.lineWidth = 2;
            for (int i = 0; i < n; i++)
            {
                float w = step * 0.56f;
                if (bottom - tops[i].y < 3)
                    continue;
                p.strokeColor = Color.Lerp(BarColor(i, n), Color.white, 0.35f);
                p.BeginPath();
                p.MoveTo(new Vector2(tops[i].x - w / 2 + 1, tops[i].y + 1));
                p.LineTo(new Vector2(tops[i].x + w / 2 - 1, tops[i].y + 1));
                p.Stroke();
            }

            // The line: Catmull-Rom through the tops as Bezier curves, a faint area under it.
            var points = tops.Select(t => new Vector2(t.x, t.y - 10 * m_Grow)).ToList();
            void Curve()
            {
                p.MoveTo(points[0]);
                for (int i = 0; i + 1 < points.Count; i++)
                {
                    Vector2 a = points[Mathf.Max(0, i - 1)], b = points[i], c = points[i + 1], d = points[Mathf.Min(points.Count - 1, i + 2)];
                    p.BezierCurveTo(b + (c - a) / 6, c - (d - b) / 6, c);
                }
            }
            p.fillColor = new Color(s_Line.r, s_Line.g, s_Line.b, 0.10f);
            p.BeginPath();
            Curve();
            p.LineTo(new Vector2(points[points.Count - 1].x, bottom));
            p.LineTo(new Vector2(points[0].x, bottom));
            p.ClosePath();
            p.Fill();
            p.strokeColor = s_Line;
            p.lineWidth = 2.5f;
            p.lineJoin = LineJoin.Round;
            p.BeginPath();
            Curve();
            p.Stroke();
            for (int i = 0; i < points.Count; i++)
            {
                float r = i == m_Hovered ? 5 : 3.5f;
                p.fillColor = new Color(0.08f, 0.09f, 0.11f);
                p.strokeColor = s_Line;
                p.lineWidth = 2;
                p.BeginPath();
                p.Arc(points[i], r, 0, 360);
                p.ClosePath();
                p.Fill();
                p.Stroke();
            }

            // The hovered bar's value in a bubble above it.
            if (m_Hovered >= 0 && m_Hovered < n)
            {
                string value = $"{m_Values[m_Hovered] * kScale:0.00} ms";
                float bw = TextWidth(value, 11) + 12, bx = Mathf.Clamp(points[m_Hovered].x - bw / 2, left, right - bw), by = points[m_Hovered].y - 28;
                if (by < top - 6)
                    by = points[m_Hovered].y + 10;
                p.fillColor = new Color(0.2f, 0.22f, 0.27f, 0.96f);
                p.BeginPath();
                p.MoveTo(new Vector2(bx + 4, by));
                p.ArcTo(new Vector2(bx + bw, by), new Vector2(bx + bw, by + 20), 4);
                p.ArcTo(new Vector2(bx + bw, by + 20), new Vector2(bx, by + 20), 4);
                p.ArcTo(new Vector2(bx, by + 20), new Vector2(bx, by), 4);
                p.ArcTo(new Vector2(bx, by), new Vector2(bx + bw, by), 4);
                p.ClosePath();
                p.Fill();
                ctx.DrawText(value, new Vector2(bx + 6, by + 3), 11, s_Bright);
            }
            if (m_Badge != null)
                ctx.DrawVectorImage(m_Badge, new Vector2(right - 22, 6), 0, new Vector2(0.75f, 0.75f));
        }

        // The donut: each stage's share of the frame as a ring segment, the total inside, a legend beside it.
        void PaintDonut(MeshGenerationContext ctx)
        {
            Rect box = ctx.element.contentRect;
            Painter2D p = ctx.painter2D;
            int n = m_Values.Length;
            float total = m_Values.Sum();
            var center = new Vector2(64, box.height / 2);
            float radius = Mathf.Min(52, box.height / 2 - 10);
            p.lineWidth = 16;
            p.lineCap = LineCap.Butt;
            p.strokeColor = new Color(1, 1, 1, 0.05f);
            p.BeginPath();
            p.Arc(center, radius, 0, 360);
            p.Stroke();
            float angle = -90;
            for (int i = 0; i < n; i++)
            {
                float sweep = 360 * m_Values[i] / total * m_Grow;
                if (sweep > 3)
                {
                    p.strokeColor = BarColor(i, n);
                    p.BeginPath();
                    p.Arc(center, radius, angle + 1.5f, angle + sweep - 1.5f);
                    p.Stroke();
                }
                angle += sweep;
            }
            string ms = $"{total * kScale:0.0}";
            ctx.DrawText(ms, new Vector2(center.x - TextWidth(ms, 18) / 2, center.y - 15), 18, s_Bright);
            ctx.DrawText("ms", new Vector2(center.x - TextWidth("ms", 10) / 2, center.y + 6), 10, s_Text);
            float lx = 136, ly = center.y - n * 8;
            for (int i = 0; i < n; i++)
            {
                float y = ly + i * 16;
                p.fillColor = BarColor(i, n);
                p.BeginPath();
                p.Arc(new Vector2(lx + 4, y + 7), 4, 0, 360);
                p.Fill();
                ctx.DrawText(s_Stages[i % s_Stages.Length], new Vector2(lx + 14, y), 10, s_Text);
                string share = $"{100 * m_Values[i] / total:0}%";
                ctx.DrawText(share, new Vector2(box.width - 8 - TextWidth(share, 10), y), 10, s_Bright);
            }
        }

        void Status(string text)
        {
            if (m_Status != null)
                m_Status.text = text;
        }
    }
}
