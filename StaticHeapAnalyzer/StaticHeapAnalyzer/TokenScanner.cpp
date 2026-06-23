#include "TokenScanner.h"

#include <fstream>

bool TokenScanner::ScanFile(const std::string& path, std::vector<TokenLine>& outTokens, std::string& error) const {
    std::ifstream input(path.c_str());
    if (!input.is_open()) {
        error = "cannot open file: " + path;
        return false;
    }

    outTokens.clear();
    std::string line;
    int lineNo = 0;
    while (std::getline(input, line)) {
        ++lineNo;
        TokenLine token;
        token.line = lineNo;
        token.text = line;
        outTokens.push_back(token);
    }

    return true;
}
