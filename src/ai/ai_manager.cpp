#include "ai_manager.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib> // for std::getenv
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// libcurl 콜백 (그대로 사용)
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// 1. 생성자
AIManager::AIManager() {
    const char* env_key = std::getenv("GEMINI_API_KEY");
    if (env_key) {
        api_key_ = env_key;
    } else {
        api_key_ = "";  // 빈 문자열
        std::cerr << "⚠️  GEMINI_API_KEY not set! Set it with:\n"
                  << "    export GEMINI_API_KEY='your-key-here'\n";
    }
}

std::string AIManager::get_ai_suggestion(const std::string& current_input) {
    std::string prompt = 
        "You are a Linux shell autocomplete engine. "
        "The user typed: '" + current_input + "'. "
        "Complete this command. "
        "IMPORTANT: Your output MUST start with '" + current_input + "'. "
        "Output ONLY the full command line. No explanations.";
    
    // 진짜 API 호출!
    std::string result = call_gemini_api(prompt);

    // ✅ 변경 사항 시작: 응답이 입력과 같으면 강제로 변경
    if (result == current_input) {
        result = current_input + " --help";
    }
    // ✅ 변경 사항 끝

    return result;
}
// 3. 헬퍼 함수: libcurl 통신 로직
std::string AIManager::call_gemini_api(const std::string& prompt) {
    if (api_key_.empty()) {
        return ""; 
    }

    // [사용자 요청 유지] gemini-2.5-flash 모델 URL 그대로 사용
    std::string api_url = "https://generativelanguage.googleapis.com/v1/models/gemini-2.5-flash:generateContent?key=" + api_key_;

    // JSON 생성
    json request_data;
    request_data["contents"] = json::array({
        {{ "parts", json::array({{{"text", prompt}}}) }}
    });
    std::string data_to_send = request_data.dump();

    CURL *curl;
    CURLcode res;
    std::string response_string;

    curl = curl_easy_init();
    if(curl) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data_to_send.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

        // 요청 실행
        res = curl_easy_perform(curl);
        
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);

        if(res != CURLE_OK) {
            return ""; // 통신 실패
        }
    } else {
        return ""; // CURL 초기화 실패
    }

    // 결과 파싱
    try {
        json response_json = json::parse(response_string);
        
        // 에러 체크
        if (response_json.contains("error")) {
            return ""; 
        }

        // 답변 추출
        if (response_json.contains("candidates") && !response_json["candidates"].empty()) {
             return response_json["candidates"][0]["content"]["parts"][0]["text"];
        }
    } catch (...) {
        return ""; // 파싱 실패
    }

    return "";
}