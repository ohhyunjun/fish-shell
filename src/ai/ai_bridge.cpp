#include "ai_manager.h"
#include <cstring>

static AIManager g_manager;

extern "C" {

    // 새로운 제안 생성
    void generate_ai_suggestions_from_cpp(const char* input) {
        if (input == nullptr) return;
        g_manager.generate_suggestions(input);
    }
    
    // 다음 제안으로 순환
    void next_ai_suggestion_from_cpp() {
        g_manager.next_suggestion();
    }
    
    // 현재 제안 (명령어 + 설명)
    char* get_ai_suggestion_with_description_from_cpp() {
        std::string suggestion = g_manager.get_current_suggestion_with_description();
        if (suggestion.empty()) return nullptr;
        return strdup(suggestion.c_str());
    }
    
    // 현재 제안 (명령어만)
    char* get_ai_command_only_from_cpp() {
        std::string command = g_manager.get_current_command_only();
        if (command.empty()) return nullptr;
        return strdup(command.c_str());
    }
    
    // 제안이 있는지 확인
    bool has_ai_suggestions_from_cpp() {
        return g_manager.has_suggestions();
    }
    
    // 제안 초기화
    void clear_ai_suggestions_from_cpp() {
        g_manager.clear_suggestions();
    }
    
    // 입력이 같은지 확인
    bool is_same_input_from_cpp(const char* input) {
        if (input == nullptr) return false;
        return g_manager.is_same_input(input);
    }

    // 히스토리 관리
    void add_command_history_from_cpp(const char* command) {
        if (command == nullptr) return;
        g_manager.add_command_to_history(command);
    }

    void clear_command_history_from_cpp() {
        g_manager.clear_history();
    }

    // 메모리 해제
    void free_ai_suggestion(char* ptr) {
        if (ptr != nullptr) free(ptr);
    }
}