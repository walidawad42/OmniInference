// Project-private ImGui compatibility shim.
//
// 1. Enables ImGui's math operators (+, -, *, /) on ImVec2 / ImVec4 so that
//    code like `pos + ImVec2(10, 0)` compiles. Upstream ImGui only defines
//    these when `IMGUI_DEFINE_MATH_OPERATORS` is set before <imgui.h> is
//    included.
//
// 2. Provides a `HelpMarker` helper. The original sample lives in
//    imgui_demo.cpp as a `static` function so it cannot be referenced from
//    other translation units; the project has been calling
//    `ImGui::HelpMarker(...)` at many panel sites, so we expose a tiny inline
//    re-implementation here.
//
// Include this header instead of `<imgui.h>` from every project header / TU
// that needs ImGui.
#pragma once

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <imgui.h>

namespace ImGui {

inline void HelpMarker(const char* desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

} // namespace ImGui
