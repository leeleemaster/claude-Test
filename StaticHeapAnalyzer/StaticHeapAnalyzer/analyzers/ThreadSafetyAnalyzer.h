#ifndef THREAD_SAFETY_ANALYZER_H
#define THREAD_SAFETY_ANALYZER_H

#include "../RuleEngine.h"

class ThreadSafetyAnalyzer {
public:
    static void Analyze(const std::string& file, const std::vector<TokenLine>& tokens, std::vector<Finding>& findings);
};

#endif
