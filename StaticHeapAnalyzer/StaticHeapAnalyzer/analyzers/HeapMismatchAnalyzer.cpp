#include "HeapMismatchAnalyzer.h"

#include <regex>

void HeapMismatchAnalyzer::Analyze(const std::string& file, const std::vector<TokenLine>& tokens, SymbolTable& symbolTable, std::vector<Finding>& findings) {
    std::regex newArrayRegex("([A-Za-z_][A-Za-z0-9_:<>]*)\\s*\\*\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*new\\s+[A-Za-z_][A-Za-z0-9_:<>]*\\s*\\[");
    std::regex deleteRegex("\\bdelete\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*;");
    std::regex deleteArrayRegex("\\bdelete\\s*\\[\\s*\\]\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*;");
    std::regex functionRegex("([A-Za-z_][A-Za-z0-9_:<>~]*)\\s*\\([^;]*\\)\\s*\\{");

    std::string scope = "global";
    int braceDepth = 0;

    for (std::vector<TokenLine>::const_iterator it = tokens.begin(); it != tokens.end(); ++it) {
        std::smatch fnMatch;
        if (std::regex_search(it->text, fnMatch, functionRegex)) {
            const std::string candidate = fnMatch[1].str();
            if (candidate != "if" && candidate != "for" && candidate != "while" && candidate != "switch") {
                scope = candidate;
            }
        }

        for (size_t i = 0; i < it->text.size(); ++i) {
            if (it->text[i] == '{') {
                ++braceDepth;
            } else if (it->text[i] == '}') {
                --braceDepth;
                if (braceDepth <= 0) {
                    braceDepth = 0;
                    scope = "global";
                }
            }
        }

        std::smatch match;
        if (std::regex_search(it->text, match, newArrayRegex)) {
            symbolTable.RecordNew(match[2].str(), match[1].str(), true, it->line, scope);
        }

        if (std::regex_search(it->text, match, deleteArrayRegex)) {
            symbolTable.MarkDelete(match[1].str(), scope, it->line);
        } else if (std::regex_search(it->text, match, deleteRegex)) {
            std::string name = match[1].str();
            const SymbolEntry* entry = symbolTable.Find(name, scope);
            if (entry != 0 && entry->isArray) {
                Finding finding;
                finding.severity = Severity::CRITICAL;
                finding.file = file;
                finding.line = it->line;
                finding.rule = "HEAP-001";
                finding.message = "new[] 할당 변수에 delete 사용 (delete[] 필요)";
                findings.push_back(finding);
            }
            symbolTable.MarkDelete(name, scope, it->line);
        }
    }
}
