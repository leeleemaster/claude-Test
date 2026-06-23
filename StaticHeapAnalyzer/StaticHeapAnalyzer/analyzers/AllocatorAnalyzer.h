#ifndef ALLOCATOR_ANALYZER_H
#define ALLOCATOR_ANALYZER_H

#include "../RuleEngine.h"

class AllocatorAnalyzer {
public:
    static void Analyze(const std::string& file, const std::vector<TokenLine>& tokens, std::vector<Finding>& findings);
};

#endif
