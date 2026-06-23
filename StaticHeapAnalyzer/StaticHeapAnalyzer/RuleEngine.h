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
    std::string rule;
    std::string message;
};

class RuleEngine {
public:
    RuleEngine();
    void SetEnabledRules(const std::set<std::string>& rules);
    std::vector<Finding> AnalyzeFile(const std::string& file, const std::vector<TokenLine>& tokens) const;

private:
    bool IsRuleEnabled(const std::string& ruleId) const;

    std::set<std::string> m_enabledRules;
};

#endif
