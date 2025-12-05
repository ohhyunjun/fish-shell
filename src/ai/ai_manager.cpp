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

// ---------------------------------------------------------
// 헬퍼 함수: 문자열 앞뒤 공백 제거
// ---------------------------------------------------------
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (std::string::npos == first) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

// ---------------------------------------------------------
// 헬퍼 함수: Markdown 코드 블록 및 백틱 제거
// ---------------------------------------------------------
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
    // 인라인 코드 백틱 제거
    text.erase(std::remove(text.begin(), text.end(), '`'), text.end());
    return trim(text);
}

// ---------------------------------------------------------
// libcurl 쓰기 콜백
// ---------------------------------------------------------
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// ---------------------------------------------------------
// AIManager 클래스 구현
// ---------------------------------------------------------

// 생성자
AIManager::AIManager() : current_index_(0), current_mode_(AIMode::GENERATION) {
    const char* env_key = std::getenv("GEMINI_API_KEY");
    api_key_ = env_key ? env_key : "";
}

// 명령어 히스토리 추가
void AIManager::add_command_to_history(const std::string& command) {
    if (command.empty() || command.find_first_not_of(" \t\n\r") == std::string::npos) 
        return;
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();
    
    command_history_.emplace_back(command, timestamp);
    // 최대 히스토리 개수 유지
    if (command_history_.size() > MAX_HISTORY_SIZE) {
        command_history_.pop_front();
    }
}

// 히스토리 초기화
void AIManager::clear_history() {
    command_history_.clear();
}

// 작업 패턴 감지 (컨텍스트 분석용)
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

// 컨텍스트 문자열 생성
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

// [핵심] 모드별 AI 제안 생성
void AIManager::generate_suggestions(const std::string& current_input, AIMode mode) {
    suggestions_.clear();
    current_index_ = 0;
    last_input_ = current_input;
    current_mode_ = mode; // 현재 모드 저장
    
    if (current_input.empty() || api_key_.empty()) return;
    
    std::string context = analyze_context();
    
    // 시스템 프롬프트 설정
    std::string prompt = "You are a Linux shell expert assistant.\n\n";
    
    if (!context.empty()) {
        prompt += "User's recent activity:\n" + context + "\n";
    }
    prompt += "User Input: \"" + current_input + "\"\n\n";

    // --- 모드별 프롬프트 분기 ---
    if (mode == AIMode::GENERATION) {
        // Mode 1: 자동 완성 (Alt+W)
        prompt += "TASK: Provide 3-5 most useful command completion options.\n";
        prompt += "OUTPUT FORMAT: COMMAND | DESCRIPTION\n";
        prompt += "RULES:\n";
        prompt += "- If input is incomplete, complete it.\n";
        prompt += "- If input is a question (Korean/English), convert intent to a command.\n";
        prompt += "- One option per line.\n";
        prompt += "Example:\nls -al | List all files details\n";
    } 
    else if (mode == AIMode::EXPLAIN) {
        // Mode 2: 설명 (Alt+Q)
        prompt += "TASK: Explain exactly what the User Input command does in KOREAN.\n";
        prompt += "RULES:\n";
        prompt += "- Explain flags/options clearly.\n";
        prompt += "- Use friendly tone.\n";
        prompt += "- Keep it under 2 sentences.\n";
        prompt += "OUTPUT FORMAT: ORIGINAL_COMMAND | EXPLANATION_IN_KOREAN\n";
        prompt += "Example:\ntar -czvf a.tar.gz . | 현재 폴더를 gzip으로 압축합니다.\n";
    }
    else if (mode == AIMode::DIAGNOSE) {
        // Mode 3: 진단 (Alt+R)
        prompt += "TASK: Analyze the User Input command for safety risks, efficiency, or improvements.\n";
        prompt += "RULES:\n";
        prompt += "- If dangerous (e.g., rm -rf /), warn strongly.\n";
        prompt += "- If safe but old, suggest modern alternatives.\n";
        prompt += "- If correct, just say 'Safe and correct'.\n";
        prompt += "- Answer in KOREAN.\n";
        prompt += "OUTPUT FORMAT: ORIGINAL_COMMAND | DIAGNOSIS_IN_KOREAN\n";
        prompt += "Example:\nrm -rf / | ⚠️ 루트 디렉토리가 삭제될 수 있어 매우 위험합니다!\n";
    }

    prompt += "\nNO markdown. Output:";
    
    // API 호출
    std::string result = call_gemini_api(prompt);
    result = remove_markdown(result);
    
    if (!result.empty()) {
        parse_suggestions(result);
    }
}

// 결과 파싱
void AIManager::parse_suggestions(const std::string& response) {
    std::istringstream stream(response);
    std::string line;
    
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty()) continue;
        
        // "1. ", "- " 등 불필요한 접두사 제거
        if (line.length() > 2 && line[1] == '.' && isdigit(line[0])) {
            line = line.substr(3);
        }
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
        } else {
            // 구분자가 없는 경우
            if (current_mode_ != AIMode::GENERATION) {
                // 설명/진단 모드: 입력값은 그대로 두고, 응답 전체를 설명으로 처리
                suggestions_.push_back(AISuggestion(last_input_, line));
            } else {
                // 자동 완성 모드: 설명 없이 명령어만 있는 경우
                suggestions_.push_back(AISuggestion(line, ""));
            }
        }
        
        if (suggestions_.size() >= 5) break;
    }
}

// 다음 제안으로 순환
void AIManager::next_suggestion() {
    if (!suggestions_.empty()) {
        current_index_ = (current_index_ + 1) % suggestions_.size();
    }
}

// 현재 제안 가져오기 (UI 표시용: "명령어 (설명)")
std::string AIManager::get_current_suggestion_with_description() {
    if (suggestions_.empty() || current_index_ >= suggestions_.size()) {
        return "";
    }
    
    const auto& suggestion = suggestions_[current_index_];
    
    if (suggestion.description.empty()) {
        return suggestion.command;
    }
    
    // 모드에 따라 괄호 처리 등을 다르게 할 수도 있음
    return suggestion.command + "  (" + suggestion.description + ")";
}

// 현재 제안 명령어만 가져오기 (실제 입력용)
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

// Gemini API 호출
std::string AIManager::call_gemini_api(const std::string& prompt) {
    if (api_key_.empty()) return "";

    std::string api_url = "https://generativelanguage.googleapis.com/v1/models/gemini-2.5-flash:generateContent?key=" + api_key_;

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