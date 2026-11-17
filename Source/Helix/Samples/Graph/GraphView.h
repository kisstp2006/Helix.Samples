#pragma once

// The node graph sample's element types, built on a panel's events the way controls are:
//   <SampleGraph inspector="name" status="name"/>
//       the graph of graph::TheGraph(): a grid, its nodes (elements styled by the sheet's graph- classes), bezier
//       edges in their port types' colors. The middle button or Alt with the left one pans, the wheel zooms around
//       the pointer, a click selects (Shift adds), a drag on the background selects with a band, a drag on a node
//       moves the selected nodes, a drag from a port connects it (from a connected input: takes the edge off), a
//       drag on an inline value changes it, Delete removes, F or A frames the graph, a right click or Space opens the
//       create node menu. The elements named by inspector and status show the selection and the view.
//   <SampleNodePreview node="main"/>
//       the graph's result on a lit sphere (the nodes' own previews are made by the graph).
namespace helix::samples
{
    // Registers the element types (once; again replaces them).
    void RegisterSampleGraph();
}
