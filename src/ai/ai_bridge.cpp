#include "ai_manager.h"
#include <cstring> // for strdup, malloc
#include <iostream>

// Rust는 C++ 클래스를 직접 못 쓰므로, 
// C 언어 스타일(extern "C")의 함수로 감싸서 내보냅니다.
extern "C" {

    // 1. Rust가 호출할 함수: AI 제안 가져오기
    char* get_ai_suggestion_from_cpp(const char* input) {
        // 입력값이 없으면 NULL 반환
        if (input == nullptr) {
            return nullptr;
        }

        // 정적(static) 객체로 관리하여 매번 키 설정을 다시 하지 않도록 함
        static AIManager manager; 

        // C++의 AI 엔진 호출!
        std::string suggestion = manager.get_ai_suggestion(input);

        // 결과가 비어있으면 NULL 반환
        if (suggestion.empty()) {
            return nullptr;
        }

        // [중요] C++ std::string을 C 스타일 문자열(char*)로 변환하여 복사
        // Rust가 가져다 쓸 수 있도록 메모리를 새로 할당합니다.
        char* result = strdup(suggestion.c_str());
        return result;
    }

    // 2. Rust가 호출할 함수: 사용 끝난 메모리 해제
    // Rust가 가져간 문자열은 C++이 할당한 것이므로, 해제도 C++이 해야 안전합니다.
    void free_ai_suggestion(char* ptr) {
        if (ptr != nullptr) {
            free(ptr);
        }
    }
}