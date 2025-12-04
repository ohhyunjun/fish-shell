#ifndef FISH_AI_MANAGER_H
#define FISH_AI_MANAGER_H

#include <string>
#include <vector>
#include <deque>

// 명령어 히스토리 항목
struct CommandHistoryEntry {
    std::string command;
    long long timestamp;
    
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

class AIManager {
public:
    AIManager();

    // 현재 입력에 대한 여러 제안 생성
    void generate_suggestions(const std::string& current_input);
    
    // 다음 제안으로 이동
    void next_suggestion();
    
    // 현재 제안 가져오기 (명령어 + 설명)
    std::string get_current_suggestion_with_description();
    
    // 현재 제안의 명령어만 가져오기
    std::string get_current_command_only();
    
    // 제안이 있는지 확인
    bool has_suggestions() const;
    
    // 제안 초기화 (입력이 바뀌었을 때)
    void clear_suggestions();
    
    // 마지막 입력 확인 (입력이 바뀌었는지 체크)
    bool is_same_input(const std::string& input) const;
    
    // 명령어 히스토리 관리
    void add_command_to_history(const std::string& command);
    void clear_history();

private:
    std::string api_key_;
    std::deque<CommandHistoryEntry> command_history_;
    static const size_t MAX_HISTORY_SIZE = 20;
    
    // 현재 제안 목록
    std::vector<AISuggestion> suggestions_;
    size_t current_index_;
    
    // 마지막 입력 (변경 감지용)
    std::string last_input_;
    
    // 헬퍼 함수
    std::string call_gemini_api(const std::string& prompt);
    std::string analyze_context();
    std::string detect_workflow_pattern();
    void parse_suggestions(const std::string& response);
};

#endif // FISH_AI_MANAGER_H