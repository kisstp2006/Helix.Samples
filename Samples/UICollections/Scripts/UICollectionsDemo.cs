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
        Element m_Chart;
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
                m_Chart.generateVisualContent = Paint;
            root.Find<Button>("animate")?.RegisterCallback<ClickEvent>(_ => Grow());
            root.Find<Button>("shuffle")?.RegisterCallback<ClickEvent>(_ =>
            {
                var r = new System.Random(++m_Shuffles);
                m_Values = m_Values.Select(_ => 0.2f + (float)r.NextDouble() * 0.8f).ToArray();
                Grow();
            });

            // The meter's width follows the animation of style values.
            var fill = root.Find("meter-fill");
            root.Find<Button>("animate")?.RegisterCallback<ClickEvent>(_ =>
                fill?.experimental.animation.Start(new StyleValues { width = 20 }, new StyleValues { width = 240 }, 900).Ease(Easing.OutBounce));
        }

        void Grow()
        {
            m_Chart?.experimental.animation.Start(0f, 1f, 700, (e, v) =>
            {
                m_Grow = v;
                e.MarkDirtyRepaint();
            }).Ease(Easing.OutCubic).OnCompleted(() => Status("The animation ended"));
        }

        // Bars of the values (grown by the animation), a line through their tops, the vector image in the corner.
        void Paint(MeshGenerationContext ctx)
        {
            Rect box = ctx.element.contentRect;
            Painter2D p = ctx.painter2D;
            float pad = 14, width = box.width - pad * 2, height = box.height - pad * 2 - 10;
            float step = width / m_Values.Length;
            p.strokeColor = new Color(1, 1, 1, 0.12f);
            p.lineWidth = 1;
            for (int i = 0; i <= 4; i++)
            {
                float y = pad + 10 + height * i / 4;
                p.BeginPath();
                p.MoveTo(new Vector2(pad, y));
                p.LineTo(new Vector2(pad + width, y));
                p.Stroke();
            }
            var tops = new List<Vector2>();
            for (int i = 0; i < m_Values.Length; i++)
            {
                float h = height * m_Values[i] * m_Grow;
                float x = pad + step * i + step * 0.18f, w = step * 0.64f, y = pad + 10 + height - h;
                p.fillColor = Color.Lerp(new Color(0.35f, 0.65f, 1f), new Color(0.45f, 0.9f, 0.6f), i / (float)(m_Values.Length - 1));
                p.BeginPath();
                p.MoveTo(new Vector2(x, y + 3));
                p.ArcTo(new Vector2(x, y), new Vector2(x + 3, y), 3);
                p.LineTo(new Vector2(x + w - 3, y));
                p.ArcTo(new Vector2(x + w, y), new Vector2(x + w, y + 3), 3);
                p.LineTo(new Vector2(x + w, pad + 10 + height));
                p.LineTo(new Vector2(x, pad + 10 + height));
                p.ClosePath();
                p.Fill();
                tops.Add(new Vector2(x + w / 2, y));
            }
            p.strokeColor = new Color(1f, 0.78f, 0.3f);
            p.lineWidth = 2.5f;
            p.lineJoin = LineJoin.Round;
            p.lineCap = LineCap.Round;
            p.BeginPath();
            for (int i = 0; i < tops.Count; i++)
            {
                if (i == 0)
                    p.MoveTo(tops[i]);
                else
                    p.LineTo(tops[i]);
            }
            p.Stroke();
            if (m_Badge != null)
                ctx.DrawVectorImage(m_Badge, new Vector2(box.width - 34, 4), 0, Vector2.one);
            ctx.DrawText("Frames per stage", new Vector2(pad, 4), 11, new Color(0.6f, 0.62f, 0.68f));
        }

        void Status(string text)
        {
            if (m_Status != null)
                m_Status.text = text;
        }
    }
}
