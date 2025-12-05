#include "ai_manager.h"
#include <cstring>
#include <cstdlib> // free 사용을 위해 필요

// 전역 AI 매니저 인스턴스 (모든 함수가 이 인스턴스를 공유)
static AIManager g_manager;

extern "C" {

    // [핵심] Rust에서 요청한 모드(int)를 받아서 AI 제안 생성
    // mode_int: 1(자동완성), 2(설명), 3(진단)
    void generate_ai_suggestions_from_cpp(const char* input, int mode_int) {
        if (input == nullptr) return;
        
        // 정수형 모드를 C++ Enum으로 변환
        AIMode mode = AIMode::GENERATION; // 기본값: 1 (자동완성)
        
        if (mode_int == 2) {
            mode = AIMode::EXPLAIN;       // 2: 설명 (Alt+Q)
        } else if (mode_int == 3) {
            mode = AIMode::DIAGNOSE;      // 3: 진단 (Alt+R)
        }
        
        g_manager.generate_suggestions(input, mode);
    }
    
    // 다음 제안으로 이동 (주로 자동완성 모드에서 Tab/순환 시 사용)
    void next_ai_suggestion_from_cpp() {
        g_manager.next_suggestion();
    }
    
    // 현재 제안 가져오기: "명령어 (설명)" 형태
    // 반환된 문자열은 Rust 쪽에서 free_ai_suggestion으로 해제해야 함
    char* get_ai_suggestion_with_description_from_cpp() {
        std::string suggestion = g_manager.get_current_suggestion_with_description();
        if (suggestion.empty()) return nullptr;
        return strdup(suggestion.c_str());
    }
    
    // 현재 제안 중 "명령어" 부분만 가져오기 (실제 입력 적용 시 사용)
    char* get_ai_command_only_from_cpp() {
        std::string command = g_manager.get_current_command_only();
        if (command.empty()) return nullptr;
        return strdup(command.c_str());
    }
    
    // 현재 유효한 제안이 있는지 확인
    bool has_ai_suggestions_from_cpp() {
        return g_manager.has_suggestions();
    }
    
    // 제안 데이터 초기화
    void clear_ai_suggestions_from_cpp() {
        g_manager.clear_suggestions();
    }
    
    // 입력값 변경 여부 확인 (최적화용)
    bool is_same_input_from_cpp(const char* input) {
        if (input == nullptr) return false;
        return g_manager.is_same_input(input);
    }

    // 실행된 명령어를 히스토리에 추가
    void add_command_history_from_cpp(const char* command) {
        if (command == nullptr) return;
        g_manager.add_command_to_history(command);
    }

    // 히스토리 초기화
    void clear_command_history_from_cpp() {
        g_manager.clear_history();
    }

    // C++에서 할당(strdup)한 메모리를 해제
    void free_ai_suggestion(char* ptr) {
        if (ptr != nullptr) free(ptr);
    }
}