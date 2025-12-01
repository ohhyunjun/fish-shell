#ifndef FISH_AI_MANAGER_H
#define FISH_AI_MANAGER_H

#include <string>

// AI 엔진의 기능을 정의하는 클래스
class AIManager {
public:
    // 생성자
    AIManager();

    // AI에게 자동 완성을 요청하는 메인 함수
    std::string get_ai_suggestion(const std::string& current_input);

private:
    std::string api_key_;

    // libcurl과 nlohmann/json을 사용하는 헬퍼 함수
    std::string call_gemini_api(const std::string& prompt);
};

#endif // FISH_AI_MANAGER_H