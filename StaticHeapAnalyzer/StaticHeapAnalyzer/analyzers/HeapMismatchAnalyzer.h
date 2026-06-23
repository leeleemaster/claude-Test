#ifndef HEAP_MISMATCH_ANALYZER_H
#define HEAP_MISMATCH_ANALYZER_H

#include "../RuleEngine.h"
#include "../SymbolTable.h"

class HeapMismatchAnalyzer {
public:
    static void Analyze(const std::string& file, const std::vector<TokenLine>& tokens, SymbolTable& symbolTable, std::vector<Finding>& findings);
};

#endif
