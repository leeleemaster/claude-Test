#include "RuleEngine.h"

#include "analyzers/AllocatorAnalyzer.h"
#include "analyzers/CtorInitAnalyzer.h"
#include "analyzers/LineRuleAnalyzer.h"
#include "analyzers/ScopeRuleAnalyzer.h"
#include "analyzers/StackHeapAnalyzer.h"
#include "analyzers/ThreadSafetyAnalyzer.h"

RuleEngine::RuleEngine() {
}

void RuleEngine::SetEnabledRules(const std::set<std::string>& rules) {
    m_enabledRules = rules;
}

bool RuleEngine::IsRuleEnabled(const std::string& ruleId) const {
    if (m_enabledRules.empty()) {
        return true;
    }
    for (std::set<std::string>::const_iterator it = m_enabledRules.begin(); it != m_enabledRules.end(); ++it) {
        if (ruleId == *it) {
            return true;
        }
        // "R2" 로 "R2-new-delete-mismatch" 까지 매칭
        const std::string prefix = *it + "-";
        if (ruleId.size() >= prefix.size() && ruleId.compare(0, prefix.size(), prefix) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<Finding> RuleEngine::AnalyzeFile(const std::string& file, const std::vector<TokenLine>& tokens) const {
    std::vector<Finding> raw;

    // 설계 R1~R9
    LineRuleAnalyzer::Analyze(file, tokens, raw);   // R1, R4, R5, R6, R7
    ScopeRuleAnalyzer::Analyze(file, tokens, raw);  // R2, R3, R9

    // 기존 확장 규칙 (설계 비범위지만 유지)
    AllocatorAnalyzer::Analyze(file, tokens, raw);    // ALLOC-001
    CtorInitAnalyzer::Analyze(file, tokens, raw);     // CTOR-001
    StackHeapAnalyzer::Analyze(file, tokens, raw);    // STACK-001
    ThreadSafetyAnalyzer::Analyze(file, tokens, raw); // THREAD-001

    // --rules 필터 적용
    std::vector<Finding> findings;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (IsRuleEnabled(raw[i].rule)) {
            findings.push_back(raw[i]);
        }
    }
    return findings;
}
