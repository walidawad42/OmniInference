#include "rag_integration.h"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

RAGIntegration::RAGIntegration(const RAGConfig& config) : config_(config) {
    curl_handle_ = curl_easy_init();
}

RAGIntegration::~RAGIntegration() {
    StopServers();
    if (curl_handle_) {
        curl_easy_cleanup(curl_handle_);
    }
}

bool RAGIntegration::StartUltraRAGServer() {
    std::cout << "[UltraRAG] Starting server at " << config_.ultrarag_server_url << std::endl;

#ifdef _WIN32
    int result = system("start /B python -m ultrarag.server --port 8000");
#else
    int result = system("python -m ultrarag.server --port 8000 &");
#endif

    ultrarag_running_ = (result == 0);
    return ultrarag_running_;
}

bool RAGIntegration::StartMiniCPMServer() {
    std::cout << "[MiniCPM] Starting server at " << config_.minicpm_server_url << std::endl;

#ifdef _WIN32
    int result = system("start /B python -m minicpm.server --port 8001 --gpu");
#else
    int result = system("python -m minicpm.server --port 8001 --gpu &");
#endif

    minicpm_running_ = (result == 0);
    return minicpm_running_;
}

bool RAGIntegration::AddDocumentsFromGit(const GitSource& git_source) {
    json payload{};
    payload["repo_url"] = git_source.repo_url;
    payload["branch"] = git_source.branch;
    payload["include_history"] = git_source.include_history;

    std::string response = MakeRequest(config_.ultrarag_server_url, "/ingest/git", "POST", payload);

    try {
        json data = json::parse(response);
        return data.value("success", false);
    } catch (...) {
        return false;
    }
}

bool RAGIntegration::AddDocumentsFromGitingest(const std::string& gitingest_url) {
    // GitIngest converts GitHub repos to single-file digests
    // We parse the gitingest output and add it to RAG

    json payload{};
    payload["gitingest_url"] = gitingest_url;

    std::string response = MakeRequest(config_.ultrarag_server_url, "/ingest/gitingest", "POST", payload);

    try {
        json data = json::parse(response);
        return data.value("success", false);
    } catch (...) {
        return false;
    }
}

RAGResult RAGIntegration::QueryRAG(const RAGQuery& query) {
    RAGResult result{};
    result.query_id = query.query_text;
    result.original_query = query.query_text;

    // Get embeddings
    std::vector<float> query_embedding = GetEmbedding(query.query_text);

    // Retrieve from UltraRAG
    json payload{};
    payload["query"] = query.query_text;
    payload["top_k"] = query.top_k;
    payload["threshold"] = query.similarity_threshold;
    payload["embedding"] = query_embedding;

    std::string response = MakeRequest(config_.ultrarag_server_url, "/search", "POST", payload);

    try {
        json data = json::parse(response);

        for (const auto& chunk_data : data.value("results", json::array())) {
            DocumentChunk chunk{};
            chunk.chunk_id = chunk_data.value("id", "");
            chunk.source_id = chunk_data.value("source_id", "");
            chunk.content = chunk_data.value("content", "");
            chunk.metadata = chunk_data.value("metadata", json());
            result.retrieved_chunks.push_back(chunk);
        }

        // Synthesize answer with MiniCPM
        if (!result.retrieved_chunks.empty()) {
            result.synthesized_answer = SynthesizeAnswer(query.query_text, result.retrieved_chunks);
        }
    } catch (const std::exception& e) {
        std::cerr << "[RAG] Query error: " << e.what() << std::endl;
    }

    return result;
}

std::string RAGIntegration::SynthesizeAnswer(
    const std::string& query,
    const std::vector<DocumentChunk>& context_chunks) {

    // Build context string
    std::string context;
    for (const auto& chunk : context_chunks) {
        context += "Document: " + chunk.source_id + "\n";
        context += chunk.content + "\n\n";
    }

    json payload{};
    payload["query"] = query;
    payload["context"] = context;

    std::string response = MakeRequest(config_.minicpm_server_url, "/synthesize", "POST", payload);

    try {
        json data = json::parse(response);
        return data.value("answer", "");
    } catch (...) {
        return "";
    }
}

std::string RAGIntegration::ExtractContextFromImages(const std::vector<std::string>& image_paths) {
    json payload{};
    payload["images"] = image_paths;

    std::string response = MakeRequest(config_.minicpm_server_url, "/extract_text", "POST", payload);

    try {
        json data = json::parse(response);
        return data.value("extracted_text", "");
    } catch (...) {
        return "";
    }
}

std::vector<float> RAGIntegration::GetEmbedding(const std::string& text) {
    json payload{};
    payload["text"] = text;
    payload["model"] = config_.embedding_model;

    std::string response = MakeRequest(config_.ultrarag_server_url, "/embeddings", "POST", payload);

    std::vector<float> embedding;

    try {
        json data = json::parse(response);
        if (data.contains("embedding")) {
            embedding = data["embedding"].get<std::vector<float>>();
        }
    } catch (const std::exception& e) {
        std::cerr << "[RAG] Embedding error: " << e.what() << std::endl;
    }

    return embedding;
}

std::string RAGIntegration::MakeRequest(const std::string& server_url, const std::string& endpoint, const std::string& method, const json& data) {
    std::string url = server_url + endpoint;

    curl_easy_setopt(curl_handle_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_CUSTOMREQUEST, method.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_TIMEOUT, 30L);

    std::string response;
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, [](void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    });
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response);

    if (method != "GET" && !data.is_null()) {
        std::string json_data = data.dump();
        curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS, json_data.c_str());

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, headers);
    }

    CURLcode res = curl_easy_perform(curl_handle_);

    if (res != CURLE_OK) {
        std::cerr << "[RAG] Request error: " << curl_easy_strerror(res) << std::endl;
    }

    return response;
}

bool RAGIntegration::StopServers() {
#ifdef _WIN32
    system("taskkill /F /IM ultrarag.exe");
    system("taskkill /F /IM minicpm.exe");
#else
    system("pkill -f 'ultrarag.server'");
    system("pkill -f 'minicpm.server'");
#endif
    ultrarag_running_ = false;
    minicpm_running_ = false;
    return true;
}

bool RAGIntegration::BuildIndex() {
    std::cout << "[RAG] Building index..." << std::endl;

    json payload{};
    std::string response = MakeRequest(config_.ultrarag_server_url, "/index/build", "POST", payload);

    try {
        json data = json::parse(response);
        return data.value("success", false);
    } catch (...) {
        return false;
    }
}

std::vector<DocumentSource> RAGIntegration::GetAllSources() {
    std::vector<DocumentSource> sources;

    std::string response = MakeRequest(config_.ultrarag_server_url, "/sources", "GET");

    try {
        json data = json::parse(response);
        if (data.contains("sources")) {
            for (const auto& src : data["sources"]) {
                DocumentSource source{};
                source.source_id = src.value("id", "");
                source.source_name = src.value("name", "");
                source.source_type = src.value("type", "");
                source.location = src.value("location", "");
                sources.push_back(source);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[RAG] Sources error: " << e.what() << std::endl;
    }

    return sources;
}