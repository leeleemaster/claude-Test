#ifndef CTOR_INIT_ANALYZER_H
#define CTOR_INIT_ANALYZER_H

#include "../RuleEngine.h"

class CtorInitAnalyzer {
public:
    static void Analyze(const std::string& file, const std::vector<TokenLine>& tokens, std::vector<Finding>& findings);
};

#endif
