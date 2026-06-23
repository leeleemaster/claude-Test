#include "RuleEngine.h"

#include "SymbolTable.h"
#include "analyzers/AllocatorAnalyzer.h"
#include "analyzers/BufferOverrunAnalyzer.h"
#include "analyzers/CtorInitAnalyzer.h"
#include "analyzers/HeapMismatchAnalyzer.h"
#include "analyzers/StackHeapAnalyzer.h"
#include "analyzers/ThreadSafetyAnalyzer.h"

RuleEngine::RuleEngine() {
}

void RuleEngine::SetEnabledRules(const std::set<std::string>& rules) {
    m_enabledRules = rules;
}

bool RuleEngine::IsRuleEnabled(const std::string& ruleId) const {
    return m_enabledRules.empty() || m_enabledRules.find(ruleId) != m_enabledRules.end();
}

std::vector<Finding> RuleEngine::AnalyzeFile(const std::string& file, const std::vector<TokenLine>& tokens) const {
    std::vector<Finding> findings;
    SymbolTable symbolTable;

    if (IsRuleEnabled("HEAP-001")) {
        HeapMismatchAnalyzer::Analyze(file, tokens, symbolTable, findings);
    }
    if (IsRuleEnabled("HEAP-002")) {
        BufferOverrunAnalyzer::Analyze(file, tokens, findings);
    }
    if (IsRuleEnabled("CTOR-001")) {
        CtorInitAnalyzer::Analyze(file, tokens, findings);
    }
    if (IsRuleEnabled("THREAD-001")) {
        ThreadSafetyAnalyzer::Analyze(file, tokens, findings);
    }
    if (IsRuleEnabled("ALLOC-001")) {
        AllocatorAnalyzer::Analyze(file, tokens, findings);
    }
    if (IsRuleEnabled("STACK-001")) {
        StackHeapAnalyzer::Analyze(file, tokens, findings);
    }

    return findings;
}
