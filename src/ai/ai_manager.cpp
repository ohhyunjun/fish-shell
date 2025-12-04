#include "ai_manager.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (std::string::npos == first) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

static std::string remove_markdown(std::string text) {
    size_t start_code = text.find("```");
    if (start_code != std::string::npos) {
        size_t newline = text.find('\n', start_code);
        if (newline != std::string::npos) {
            text = text.substr(newline + 1);
        } else {
            text = text.substr(start_code + 3);
        }
        
        size_t end_code = text.rfind("```");
        if (end_code != std::string::npos) {
            text = text.substr(0, end_code);
        }
    }
    text.erase(std::remove(text.begin(), text.end(), '`'), text.end());
    return trim(text);
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

AIManager::AIManager() : current_index_(0) {
    const char* env_key = std::getenv("GEMINI_API_KEY");
    api_key_ = env_key ? env_key : "";
}

void AIManager::add_command_to_history(const std::string& command) {
    if (command.empty() || command.find_first_not_of(" \t\n\r") == std::string::npos) 
        return;
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();
    
    command_history_.emplace_back(command, timestamp);
    if (command_history_.size() > MAX_HISTORY_SIZE) {
        command_history_.pop_front();
    }
}

void AIManager::clear_history() {
    command_history_.clear();
}

std::string AIManager::detect_workflow_pattern() {
    if (command_history_.empty()) return "";
    
    int git_cnt = 0, build_cnt = 0;
    for (const auto& entry : command_history_) {
        if (entry.command.find("git") == 0) git_cnt++;
        if (entry.command.find("make") == 0 || entry.command.find("npm") == 0) 
            build_cnt++;
    }
    
    std::string context;
    if (git_cnt >= 2) context += "Git Workflow; ";
    if (build_cnt >= 2) context += "Build Workflow; ";
    return context;
}

std::string AIManager::analyze_context() {
    if (command_history_.empty()) return "";
    
    std::stringstream ss;
    std::string ptrn = detect_workflow_pattern();
    if (!ptrn.empty()) ss << "Context: " << ptrn << "\n";
    
    ss << "Recent commands:\n";
    for (const auto& h : command_history_) 
        ss << "- " << h.command << "\n";
    
    return ss.str();
}

void AIManager::generate_suggestions(const std::string& current_input) {
    suggestions_.clear();
    current_index_ = 0;
    last_input_ = current_input;  // 마지막 입력 저장
    
    if (current_input.empty() || api_key_.empty()) return;
    
    std::string context = analyze_context();
    
    std::string prompt = 
        "You are a Linux shell command assistant.\n\n";
    
    if (!context.empty()) {
        prompt += "User's recent activity:\n" + context + "\n";
    }
    
    prompt += "User typed: \"" + current_input + "\"\n\n";
    prompt += "TASK: Provide 3-5 most useful command options.\n\n";
    prompt += "OUTPUT FORMAT (STRICT):\n";
    prompt += "Each line: COMMAND | DESCRIPTION\n";
    prompt += "Example:\n";
    prompt += "ls -la | Show all files with details\n";
    prompt += "ls -lh | Show files with human-readable sizes\n";
    prompt += "ls -lt | Sort by modification time\n\n";
    prompt += "RULES:\n";
    prompt += "- One option per line\n";
    prompt += "- Format: COMMAND | DESCRIPTION\n";
    prompt += "- NO numbering, NO markdown\n";
    prompt += "- Order by usefulness\n";
    prompt += "- 3-5 options\n\n";
    prompt += "Output:";
    
    std::string result = call_gemini_api(prompt);
    result = remove_markdown(result);
    
    if (!result.empty()) {
        parse_suggestions(result);
    }
}

void AIManager::parse_suggestions(const std::string& response) {
    std::istringstream stream(response);
    std::string line;
    
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty()) continue;
        
        // "1. " 같은 번호 제거
        if (line.length() > 2 && line[1] == '.' && isdigit(line[0])) {
            line = line.substr(3);
        }
        
        // "- " 제거
        if (line.find("- ") == 0) {
            line = line.substr(2);
        }
        
        // "|"로 명령어와 설명 분리
        size_t separator = line.find('|');
        
        if (separator != std::string::npos) {
            std::string cmd = trim(line.substr(0, separator));
            std::string desc = trim(line.substr(separator + 1));
            
            if (!cmd.empty()) {
                suggestions_.push_back(AISuggestion(cmd, desc));
            }
        } else if (!line.empty()) {
            suggestions_.push_back(AISuggestion(line, ""));
        }
        
        if (suggestions_.size() >= 5) break;
    }
}

void AIManager::next_suggestion() {
    if (!suggestions_.empty()) {
        current_index_ = (current_index_ + 1) % suggestions_.size();
    }
}

std::string AIManager::get_current_suggestion_with_description() {
    if (suggestions_.empty() || current_index_ >= suggestions_.size()) {
        return "";
    }
    
    const auto& suggestion = suggestions_[current_index_];
    
    if (suggestion.description.empty()) {
        return suggestion.command;
    }
    
    return suggestion.command + "  (" + suggestion.description + ")";
}

std::string AIManager::get_current_command_only() {
    if (suggestions_.empty() || current_index_ >= suggestions_.size()) {
        return "";
    }
    
    return suggestions_[current_index_].command;
}

bool AIManager::has_suggestions() const {
    return !suggestions_.empty();
}

void AIManager::clear_suggestions() {
    suggestions_.clear();
    current_index_ = 0;
    last_input_.clear();
}

bool AIManager::is_same_input(const std::string& input) const {
    return last_input_ == input;
}

std::string AIManager::call_gemini_api(const std::string& prompt) {
    if (api_key_.empty()) return "";

    std::string api_url = 
        "https://generativelanguage.googleapis.com/v1/models/gemini-2.5-flash:generateContent?key=" 
        + api_key_;

    json request_data;
    request_data["contents"] = json::array({
        {{"parts", json::array({{{"text", prompt}}})}}
    });
    std::string data_to_send = request_data.dump();

    CURL *curl = curl_easy_init();
    std::string response_string;

    if (curl) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data_to_send.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        
        if (res != CURLE_OK) return "";
    } else {
        return "";
    }

    try {
        json response_json = json::parse(response_string);
        if (response_json.contains("candidates") && 
            !response_json["candidates"].empty()) {
            return response_json["candidates"][0]["content"]["parts"][0]["text"];
        }
    } catch (...) {
        return "";
    }

    return "";
}