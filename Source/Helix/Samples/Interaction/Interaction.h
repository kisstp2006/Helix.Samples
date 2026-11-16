#pragma once

// The samples' interactive element types, built on a panel's events the way controls are:
//   <SampleButton text="..."/>   a press captures the pointer and sets :active until the release; a click or a submit
//                                (Enter) counts as a press of it
//   <SampleTextBox/>             focusable; typed characters go in, Backspace takes the last one out, the text an input
//                                method composes shows underlined at the end, the input method stays on while it is
//                                focused; the left and right arrows stay in it
//   <SampleDragItem text="..."/> a press starts dragging it
//   <SampleDropList/>            takes dragged items (they move into it), with the class drop-target while one is over it
//   <SampleProbe/>               logs the events that reach it (from its descendants too) in their order
//   <SampleEventLog/>            shows the last lines of that log
namespace helix::samples
{
    // Registers the element types (once; again replaces them).
    void RegisterSampleInteraction();
}
