#ifndef BUFFER_OVERRUN_ANALYZER_H
#define BUFFER_OVERRUN_ANALYZER_H

#include "../RuleEngine.h"

class BufferOverrunAnalyzer {
public:
    static void Analyze(const std::string& file, const std::vector<TokenLine>& tokens, std::vector<Finding>& findings);
};

#endif
