#pragma once

#include <string>
#include <vector>
#include <json.hpp>
#include <curl/curl.h>

using json = nlohmann::json;

// ============================================================
// RAG SYSTEM INTEGRATION
// UltraRAG 3.0 (OpenBMB) + MiniCPM-O 4.5
// ============================================================

struct RAGConfig {
    std::string ultrarag_server_url = "http://localhost:8000";
    std::string minicpm_server_url = "http://localhost:8001";
    bool use_ultrarag = true;
    bool use_minicpm = true;
    int chunk_size = 512;
    int overlap_size = 100;
    std::string embedding_model = "text-embedding-3-large";
    int top_k_results = 5;
};

struct DocumentSource {
    std::string source_id;
    std::string source_name;
    std::string source_type; // "file", "url", "git", "database", "gitingest"
    std::string location;
    json metadata;
};

struct DocumentChunk {
    std::string chunk_id;
    std::string source_id;
    std::string content;
    int start_position = 0;
    int end_position = 0;
    std::vector<float> embedding;
    json metadata;
};

struct RAGQuery {
    std::string query_text;
    std::vector<DocumentSource> search_sources;
    int top_k = 5;
    float similarity_threshold = 0.7f;
    bool use_reranking = true;
};

struct RAGResult {
    std::string query_id;
    std::string original_query;
    std::vector<DocumentChunk> retrieved_chunks;
    std::string synthesized_answer;
    double total_latency_ms = 0.0;
    json metadata;
};

// ============================================================
// GITREVERSE & GITINGEST INTEGRATION
// ============================================================

struct GitSource {
    std::string repo_url;
    std::string repo_name;
    std::string branch = "main";
    std::vector<std::string> include_paths;
    std::vector<std::string> exclude_paths;
    bool include_history = false;
};

class RAGIntegration {
public:
    RAGIntegration(const RAGConfig& config = RAGConfig());
    ~RAGIntegration();

    // Server Management
    bool StartUltraRAGServer();
    bool StartMiniCPMServer();
    bool StopServers();
    bool IsServerRunning();

    // Document Ingestion
    bool AddDocument(const DocumentSource& source);
    bool AddDocumentsFromGit(const GitSource& git_source);
    bool AddDocumentsFromGitingest(const std::string& gitingest_url);
    bool AddDocumentsFromDirectory(const std::string& directory_path);
    bool RemoveDocument(const std::string& source_id);

    // Indexing
    bool BuildIndex();
    bool RebuildIndex();
    bool ClearIndex();
    int GetIndexStats();

    // RAG Query
    RAGResult QueryRAG(const RAGQuery& query);
    std::vector<RAGResult> BatchQuery(const std::vector<RAGQuery>& queries);

    // Retrieval
    std::vector<DocumentChunk> RetrieveDocuments(const std::string& query_text, int top_k);
    std::vector<DocumentChunk> RetrieveBySource(const std::string& source_id);
    std::vector<DocumentChunk> RetrieveByMetadata(const json& metadata_filter);

    // Synthesis (with MiniCPM-O 4.5)
    std::string SynthesizeAnswer(
        const std::string& query,
        const std::vector<DocumentChunk>& context_chunks
    );

    // Context Extraction
    std::string ExtractContextFromImages(const std::vector<std::string>& image_paths);

    // Document Management
    std::vector<DocumentSource> GetAllSources();
    DocumentSource GetSourceInfo(const std::string& source_id);

private:
    RAGConfig config_;
    CURL* curl_handle_ = nullptr;
    bool ultrarag_running_ = false;
    bool minicpm_running_ = false;

    std::string MakeRequest(const std::string& server_url, const std::string& endpoint, const std::string& method, const json& data = json());
    json ParseResponse(const std::string& response);
    std::vector<float> GetEmbedding(const std::string& text);
};