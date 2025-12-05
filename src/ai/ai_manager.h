#ifndef FISH_AI_MANAGER_H
#define FISH_AI_MANAGER_H

#include <string>
#include <vector>
#include <deque>

// ---------------------------------------------------------
// 데이터 구조체 정의
// ---------------------------------------------------------

// 명령어 히스토리 항목
struct CommandHistoryEntry {
    std::string command;
    long long timestamp;  // Unix timestamp
    
    CommandHistoryEntry(const std::string& cmd, long long ts) 
        : command(cmd), timestamp(ts) {}
};

// 단일 AI 제안 (명령어 + 설명)
struct AISuggestion {
    std::string command;      // 예: "ls -la"
    std::string description;  // 예: "모든 파일을 상세히 표시"
    
    AISuggestion(const std::string& cmd, const std::string& desc)
        : command(cmd), description(desc) {}
};

// [핵심] AI 동작 모드 정의
enum class AIMode {
    GENERATION = 1, // 1. 자동 완성 (Alt+W)
    EXPLAIN = 2,    // 2. 설명 (Alt+Q)
    DIAGNOSE = 3    // 3. 진단 (Alt+R)
};

// ---------------------------------------------------------
// AIManager 클래스 정의
// ---------------------------------------------------------

class AIManager {
public:
    AIManager();

    // [수정] 모드(mode)를 인자로 받아 제안 생성
    void generate_suggestions(const std::string& current_input, AIMode mode);
    
    // 다음 제안으로 순환 (자동완성 모드에서 주로 사용)
    void next_suggestion();
    
    // 현재 제안 가져오기: "명령어 (설명)" 형태
    std::string get_current_suggestion_with_description();
    
    // 현재 제안의 명령어만 가져오기
    std::string get_current_command_only();
    
    // 제안이 존재하는지 확인
    bool has_suggestions() const;
    
    // 제안 초기화
    void clear_suggestions();
    
    // 마지막 입력과 동일한지 확인 (중복 요청 방지용)
    bool is_same_input(const std::string& input) const;
    
    // 명령어 히스토리 관리
    void add_command_to_history(const std::string& command);
    void clear_history();

private:
    std::string api_key_;
    
    // 명령어 히스토리 큐
    std::deque<CommandHistoryEntry> command_history_;
    static const size_t MAX_HISTORY_SIZE = 20;
    
    // 생성된 제안 목록
    std::vector<AISuggestion> suggestions_;
    size_t current_index_;
    
    // 상태 추적용 변수
    std::string last_input_;
    AIMode current_mode_;

    // 내부 헬퍼 함수
    std::string call_gemini_api(const std::string& prompt);
    std::string analyze_context();
    std::string detect_workflow_pattern();
    void parse_suggestions(const std::string& response);
};

#endif // FISH_AI_MANAGER_H