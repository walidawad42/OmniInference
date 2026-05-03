#pragma once

#include "omni_engine.h"
#include <imgui.h>
#include <imgui_node_editor.h>
#include <vector>
#include <map>
#include <memory>

namespace ed = ax::NodeEditor;

struct NodeData {
    ed::NodeId id;
    std::string title;
    std::string type; // "input", "model", "quantize", "generation", "output"
    ImVec2 position;
    std::map<std::string, float> parameters;
    bool enabled = true;
};

struct LinkData {
    ed::LinkId id;
    ed::PinId from_pin;
    ed::PinId to_pin;
};

class GUINodeEditor {
public:
    GUINodeEditor();
    ~GUINodeEditor();

    void Initialize();
    void Render();
    void Update();

    // Node Management
    void AddNode(const std::string& node_type, const ImVec2& position);
    void DeleteNode(ed::NodeId node_id);
    void ClearNodes();

    // Link Management
    void CreateLink(ed::PinId from, ed::PinId to);
    void DeleteLink(ed::LinkId link_id);

    // Export Pipeline
    PipelineConfig ExportPipeline() const;

    // Import Pipeline
    void ImportPipeline(const PipelineConfig& config);

private:
    ed::EditorContext* editor_context_ = nullptr;
    std::vector<NodeData> nodes_;
    std::vector<LinkData> links_;
    ed::NodeId next_node_id_;
    ed::LinkId next_link_id_;
    ed::PinId next_pin_id_;

    // Rendering
    void RenderNode(NodeData& node);
    void RenderLink(const LinkData& link);
    void RenderNodeCreationPopup();

    // Helper
    ed::PinId GeneratePinId();
};