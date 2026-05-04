#include "gui_node_editor.h"
#include <iostream>

GUINodeEditor::GUINodeEditor() = default;

GUINodeEditor::~GUINodeEditor() {
    if (editor_context_) {
        ed::DestroyEditor(editor_context_);
    }
}

void GUINodeEditor::Initialize() {
    ed::Config config;
    config.SettingsFile = "omni_node_editor.json";
    editor_context_ = ed::CreateEditor(&config);
}

void GUINodeEditor::Render() {
    if (!editor_context_) return;

    ed::SetCurrentEditor(editor_context_);
    ed::Begin("Pipeline Editor", ImVec2(0, 0));

    // Create node popup
    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::MenuItem("Add Model Node")) AddNode("model", ImGui::GetMousePosOnOpeningCurrentPopup());
        if (ImGui::MenuItem("Add Quantize Node")) AddNode("quantize", ImGui::GetMousePosOnOpeningCurrentPopup());
        if (ImGui::MenuItem("Add Generation Node")) AddNode("generation", ImGui::GetMousePosOnOpeningCurrentPopup());
        if (ImGui::MenuItem("Add Output Node")) AddNode("output", ImGui::GetMousePosOnOpeningCurrentPopup());
        ImGui::EndPopup();
    }

    // Render all nodes
    for (auto& node : nodes_) {
        RenderNode(node);
    }

    // Render all links
    for (const auto& link : links_) {
        RenderLink(link);
    }

    // Handle link creation
    if (ed::BeginCreate()) {
        ed::PinId input_pin_id, output_pin_id;
        if (ed::QueryNewLink(&input_pin_id, &output_pin_id)) {
            if (input_pin_id && output_pin_id) {
                CreateLink(output_pin_id, input_pin_id);
            }
        }
    }
    ed::EndCreate();

    // Handle deletion
    if (ed::BeginDelete()) {
        ed::NodeId node_id = 0;
        while (ed::GetSelectedNodes(&node_id, 1)) {
            DeleteNode(node_id);
            node_id = 0;
        }

        ed::LinkId link_id = 0;
        while (ed::GetSelectedLinks(&link_id, 1)) {
            DeleteLink(link_id);
            link_id = 0;
        }
    }
    ed::EndDelete();

    ed::End();
    ed::SetCurrentEditor(nullptr);
}

void GUINodeEditor::RenderNode(NodeData& node) {
    ed::BeginNode(node.id);

    ImGui::TextUnformatted(node.title.c_str());
    ImGui::Separator();

    // Render node-specific UI
    if (node.type == "model") {
        ImGui::Text("Model Configuration");
        ImGui::SliderInt("##layers", reinterpret_cast<int*>(&node.parameters["n_layer"]), 1, 128);
        ImGui::SliderInt("##embd", reinterpret_cast<int*>(&node.parameters["n_embd"]), 256, 16384);
    }
    else if (node.type == "quantize") {
        ImGui::Text("Quantization");
        ImGui::SliderInt("##key_bits", reinterpret_cast<int*>(&node.parameters["key_bits"]), 2, 8);
        ImGui::SliderInt("##val_bits", reinterpret_cast<int*>(&node.parameters["value_bits"]), 2, 8);
        ImGui::Checkbox("Polar Transform", reinterpret_cast<bool*>(&node.parameters["use_polar"]));
    }
    else if (node.type == "generation") {
        ImGui::Text("Generation Config");
        ImGui::SliderFloat("##temp", &node.parameters["temperature"], 0.0f, 2.0f);
        ImGui::SliderFloat("##top_p", &node.parameters["top_p"], 0.0f, 1.0f);
        ImGui::SliderInt("##max_tokens", reinterpret_cast<int*>(&node.parameters["max_tokens"]), 1, 4096);
    }

    // Input/Output pins
    ed::BeginPin(GeneratePinId(), ed::PinKind::Input);
    ImGui::Text("In");
    ed::EndPin();

    ImGui::SameLine();

    ed::BeginPin(GeneratePinId(), ed::PinKind::Output);
    ImGui::Text("Out");
    ed::EndPin();

    ed::EndNode();
}

void GUINodeEditor::AddNode(const std::string& node_type, const ImVec2& position) {
    NodeData node;
    const uintptr_t id_value = next_node_id_++;
    node.id = ed::NodeId(id_value);
    node.type = node_type;
    node.position = position;
    node.title = node_type + " #" + std::to_string(id_value);

    // Initialize default parameters
    if (node_type == "model") {
        node.parameters["n_layer"] = 32;
        node.parameters["n_embd"] = 4096;
    }
    else if (node_type == "quantize") {
        node.parameters["key_bits"] = 4;
        node.parameters["value_bits"] = 2;
        node.parameters["use_polar"] = 1;
    }
    else if (node_type == "generation") {
        node.parameters["temperature"] = 0.7f;
        node.parameters["top_p"] = 0.95f;
        node.parameters["max_tokens"] = 512;
    }

    nodes_.push_back(node);
}

void GUINodeEditor::DeleteNode(ed::NodeId node_id) {
    nodes_.erase(
        std::remove_if(nodes_.begin(), nodes_.end(),
            [node_id](const NodeData& n) { return n.id == node_id; }),
        nodes_.end()
    );
}

void GUINodeEditor::CreateLink(ed::PinId from, ed::PinId to) {
    LinkData link;
    link.id = ed::LinkId(next_link_id_++);
    link.from_pin = from;
    link.to_pin = to;
    links_.push_back(link);
}

void GUINodeEditor::DeleteLink(ed::LinkId link_id) {
    links_.erase(
        std::remove_if(links_.begin(), links_.end(),
            [link_id](const LinkData& l) { return l.id == link_id; }),
        links_.end()
    );
}

ed::PinId GUINodeEditor::GeneratePinId() {
    return ed::PinId(next_pin_id_++);
}

void GUINodeEditor::RenderLink(const LinkData& link) {
    ed::Link(link.id, link.from_pin, link.to_pin);
}

PipelineConfig GUINodeEditor::ExportPipeline() const {
    PipelineConfig config;

    for (const auto& node : nodes_) {
        PipelineNode pnode;
        pnode.node_id = node.title;
        pnode.node_type = node.type;
        pnode.parameters = node.parameters;
        pnode.enabled = node.enabled;
        config.nodes.push_back(pnode);
    }

    return config;
}

void GUINodeEditor::ImportPipeline(const PipelineConfig& config) {
    ClearNodes();

    ImVec2 pos(50, 50);
    for (const auto& pnode : config.nodes) {
        AddNode(pnode.node_type, pos);
        pos.x += 250;
    }
}

void GUINodeEditor::ClearNodes() {
    nodes_.clear();
    links_.clear();
}