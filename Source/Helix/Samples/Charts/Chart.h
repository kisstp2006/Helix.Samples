#pragma once

// The samples' chart element, drawn with the UI's painter: <SampleChart kind="line" values="3 5 2" colors="#4f9dff"/>.
// Its kinds are in a table (line: an area under a line with a gradient along it and dots on its points; bars: rounded
// bars; donut: a ring of segments); values are numbers apart by spaces or commas, colors #rrggbb or #rrggbbaa (a
// default palette without them).
namespace helix::samples
{
    // Registers the element type (once; again replaces it).
    void RegisterSampleChart();
}
