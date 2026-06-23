#ifndef RULE_ENGINE_H
#define RULE_ENGINE_H

#include "TokenScanner.h"
#include "report/Severity.h"

#include <set>
#include <string>
#include <vector>

struct Finding {
    Severity severity;
    std::string file;
    int line;
    std::string rule;     // 규칙 식별자 (예: R2-new-delete-mismatch)
    std::string message;  // 원인 설명
    std::string fix;      // 수정 제안
    std::string code;     // 원본 코드 라인
};

class RuleEngine {
public:
    RuleEngine();
    void SetEnabledRules(const std::set<std::string>& rules);
    std::vector<Finding> AnalyzeFile(const std::string& file, const std::vector<TokenLine>& tokens) const;

private:
    // 규칙 활성 여부. enabled 집합이 비어 있으면 전체 활성.
    // 항목이 ruleId 와 정확히 일치하거나 "<item>-" 접두로 시작하면 활성으로 본다.
    bool IsRuleEnabled(const std::string& ruleId) const;

    std::set<std::string> m_enabledRules;
};

#endif
