#ifndef STACK_HEAP_ANALYZER_H
#define STACK_HEAP_ANALYZER_H

#include "../RuleEngine.h"

class StackHeapAnalyzer {
public:
    static void Analyze(const std::string& file, const std::vector<TokenLine>& tokens, std::vector<Finding>& findings);
};

#endif
