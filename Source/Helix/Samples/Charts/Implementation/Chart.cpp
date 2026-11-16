#include <Helix/Samples/Charts/Chart.h>

#include <Helix/Core/Containers/Array.h>
#include <Helix/UI/Markup/Hxml.h>
#include <Helix/UI/Rendering/Painter.h>

#include <cmath>
#include <cstdlib>

namespace helix::samples
{
    namespace
    {
        using ui::ElementHandle;
        using ui::ElementTree;
        using ui::Painter;

        // What the markup gave a chart, by its tree and element.
        struct ChartData
        {
            const ElementTree* tree = nullptr;
            ElementHandle element;
            u32 kind = 0;
            Array<f32> values;
            Array<Color> colors;
        };

        Array<ChartData>& Charts()
        {
            static Array<ChartData>* charts = new Array<ChartData>();
            return *charts;
        }

        const Color kPalette[] = {Color(0.31f, 0.62f, 1.0f, 1), Color(0.25f, 0.82f, 0.66f, 1), Color(1.0f, 0.54f, 0.30f, 1),
                                  Color(1.0f, 0.84f, 0.29f, 1), Color(0.76f, 0.48f, 1.0f, 1)};

        Color ColorOf(const ChartData& c, usize i)
        {
            if (!c.colors.Empty())
                return c.colors[i % c.colors.Size()];
            return kPalette[i % (sizeof(kPalette) / sizeof(kPalette[0]))];
        }

        Color WithAlpha(Color c, f32 a) { return Color(c.r, c.g, c.b, c.a * a); }

        f32 Largest(const ChartData& c)
        {
            f32 m = 0;
            for (f32 v : c.values)
                m = v > m ? v : m;
            return m > 0 ? m : 1;
        }

        // An area under a line, the line with a gradient from the first color to the second, dots on its points.
        void PaintLine(Painter& p, const ChartData& c, f32 w, f32 h)
        {
            usize n = c.values.Size();
            if (n < 2)
                return;
            const f32 pad = 8;
            f32 top = Largest(c) * 1.1f;
            Array<Vector2> points;
            for (usize i = 0; i < n; i++)
                points.Push(Vector2(pad + f32(i) * (w - 2 * pad) / f32(n - 1), h - pad - c.values[i] / top * (h - 2 * pad)));
            // The grid.
            p.lineWidth = 1;
            p.strokeColor = Color(1, 1, 1, 0.08f);
            for (u32 k = 0; k < 4; k++)
            {
                f32 y = pad + f32(k) * (h - 2 * pad) / 3;
                p.BeginPath();
                p.MoveTo(Vector2(pad, y));
                p.LineTo(Vector2(w - pad, y));
                p.Stroke();
            }
            Color from = ColorOf(c, 0), to = ColorOf(c, 1);
            p.BeginPath();
            p.MoveTo(Vector2(points[0].x, h - pad));
            for (const Vector2& q : points)
                p.LineTo(q);
            p.LineTo(Vector2(points[n - 1].x, h - pad));
            p.ClosePath();
            p.fillColor = WithAlpha(from, 0.18f);
            p.Fill();
            p.BeginPath();
            p.MoveTo(points[0]);
            for (usize i = 1; i < n; i++)
                p.LineTo(points[i]);
            p.lineWidth = 3;
            p.lineJoin = ui::LineJoin::Round;
            p.lineCap = ui::LineCap::Round;
            p.strokeGradient.Push({0, from});
            p.strokeGradient.Push({1, to});
            p.Stroke();
            p.strokeGradient.Clear();
            for (usize i = 0; i < n; i++)
            {
                p.BeginPath();
                p.Arc(points[i], 4, 0, 360);
                p.ClosePath();
                p.fillColor = Color(0.12f, 0.13f, 0.17f, 1);
                p.Fill();
                p.BeginPath();
                p.Arc(points[i], 4, 0, 360);
                p.ClosePath();
                p.lineWidth = 2;
                f32 t = f32(i) / f32(n - 1);
                p.strokeColor = Color(from.r + (to.r - from.r) * t, from.g + (to.g - from.g) * t, from.b + (to.b - from.b) * t, 1);
                p.Stroke();
            }
        }

        // Bars with rounded tops, a color each.
        void PaintBars(Painter& p, const ChartData& c, f32 w, f32 h)
        {
            usize n = c.values.Size();
            if (n == 0)
                return;
            const f32 gap = 8;
            f32 bw = (w - gap * f32(n + 1)) / f32(n);
            if (bw <= 0)
                return;
            f32 top = Largest(c);
            f32 r = bw * 0.5f < 6 ? bw * 0.5f : 6;
            for (usize i = 0; i < n; i++)
            {
                f32 x = gap + f32(i) * (bw + gap);
                f32 bh = c.values[i] / top * (h - gap);
                f32 y = h - bh;
                if (bh < r)
                    continue;
                p.BeginPath();
                p.MoveTo(Vector2(x, h));
                p.LineTo(Vector2(x, y + r));
                p.Arc(Vector2(x + r, y + r), r, 180, 270);
                p.LineTo(Vector2(x + bw - r, y));
                p.Arc(Vector2(x + bw - r, y + r), r, 270, 360);
                p.LineTo(Vector2(x + bw, h));
                p.ClosePath();
                p.fillColor = ColorOf(c, i);
                p.Fill();
            }
        }

        // A ring of segments from the top, clockwise, apart by a small gap.
        void PaintDonut(Painter& p, const ChartData& c, f32 w, f32 h)
        {
            f32 total = 0;
            for (f32 v : c.values)
                total += v > 0 ? v : 0;
            if (total <= 0)
                return;
            Vector2 center(w * 0.5f, h * 0.5f);
            f32 outer = (w < h ? w : h) * 0.5f - 2, inner = outer * 0.62f;
            if (inner <= 0)
                return;
            const f32 gap = 2;
            f32 angle = -90;
            for (usize i = 0; i < c.values.Size(); i++)
            {
                f32 sweep = (c.values[i] > 0 ? c.values[i] : 0) / total * 360;
                if (sweep > gap)
                {
                    p.BeginPath();
                    p.Arc(center, outer, angle + gap * 0.5f, angle + sweep - gap * 0.5f, ui::ArcDirection::Clockwise);
                    p.Arc(center, inner, angle + sweep - gap * 0.5f, angle + gap * 0.5f, ui::ArcDirection::CounterClockwise);
                    p.ClosePath();
                    p.fillColor = ColorOf(c, i);
                    p.Fill();
                }
                angle += sweep;
            }
        }

        struct KindEntry
        {
            StringView name;
            void (*paint)(Painter&, const ChartData&, f32, f32);
        };

        constexpr KindEntry kKinds[] = {{"line", &PaintLine}, {"bars", &PaintBars}, {"donut", &PaintDonut}};
        constexpr usize kKindCount = sizeof(kKinds) / sizeof(kKinds[0]);

        ChartData& DataOf(const ElementTree& tree, ElementHandle e)
        {
            Array<ChartData>& charts = Charts();
            // Charts of elements that went make room.
            for (usize i = 0; i < charts.Size();)
                if (charts[i].tree == &tree && !tree.IsAlive(charts[i].element))
                    charts.RemoveAt(i);
                else
                    i++;
            for (ChartData& c : charts)
                if (c.tree == &tree && c.element == e)
                    return c;
            ChartData c;
            c.tree = &tree;
            c.element = e;
            charts.Push(Move(c));
            return charts.Back();
        }

        const ChartData* FindData(const ElementTree& tree, ElementHandle e)
        {
            for (const ChartData& c : Charts())
                if (c.tree == &tree && c.element == e)
                    return &c;
            return nullptr;
        }

        bool IsSeparator(char ch) { return ch == ' ' || ch == ',' || ch == '\t' || ch == '\n' || ch == '\r'; }

        // The words of a list apart by spaces or commas.
        template <class F>
        void Words(StringView text, F&& each)
        {
            usize i = 0;
            while (i < text.Size())
            {
                while (i < text.Size() && IsSeparator(text[i]))
                    i++;
                usize start = i;
                while (i < text.Size() && !IsSeparator(text[i]))
                    i++;
                if (i > start)
                    each(text.Substr(start, i - start));
            }
        }

        bool ParseColor(StringView word, Color& out)
        {
            if (word.Size() != 7 && word.Size() != 9)
                return false;
            if (word[0] != '#')
                return false;
            u32 channels[4] = {0, 0, 0, 255};
            for (usize k = 0; k < (word.Size() - 1) / 2; k++)
            {
                char digits[3] = {word[1 + k * 2], word[2 + k * 2], 0};
                char* end = nullptr;
                channels[k] = u32(std::strtoul(digits, &end, 16));
                if (end != digits + 2)
                    return false;
            }
            out = Color(f32(channels[0]) / 255, f32(channels[1]) / 255, f32(channels[2]) / 255, f32(channels[3]) / 255);
            return true;
        }

        bool Attribute(ElementTree& tree, ElementHandle e, StringView name, StringView value)
        {
            if (name == StringView("kind"))
            {
                for (u32 k = 0; k < kKindCount; k++)
                    if (kKinds[k].name == value)
                    {
                        DataOf(tree, e).kind = k;
                        return true;
                    }
                return false;
            }
            if (name == StringView("values"))
            {
                ChartData& c = DataOf(tree, e);
                c.values.Clear();
                bool ok = true;
                Words(value, [&](StringView word) {
                    String text(word);
                    char* end = nullptr;
                    f32 v = std::strtof(text.CStr(), &end);
                    ok = ok && end == text.CStr() + text.Size();
                    c.values.Push(v);
                });
                return ok;
            }
            if (name == StringView("colors"))
            {
                ChartData& c = DataOf(tree, e);
                c.colors.Clear();
                bool ok = true;
                Words(value, [&](StringView word) {
                    Color color;
                    ok = ParseColor(word, color) && ok;
                    c.colors.Push(color);
                });
                return ok;
            }
            return false;
        }

        void Paint(Painter& p, const ElementTree& tree, ElementHandle e, f32 width, f32 height)
        {
            if (const ChartData* c = FindData(tree, e))
                kKinds[c->kind].paint(p, *c, width, height);
        }
    }

    void RegisterSampleChart() { ui::RegisterElementType("SampleChart", &Attribute, &Paint); }
}
