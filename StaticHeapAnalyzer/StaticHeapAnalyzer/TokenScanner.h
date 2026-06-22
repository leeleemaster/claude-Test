#ifndef TOKEN_SCANNER_H
#define TOKEN_SCANNER_H

#include <string>
#include <vector>

struct TokenLine {
    int line;
    std::string text;
};

class TokenScanner {
public:
    bool ScanFile(const std::string& path, std::vector<TokenLine>& outTokens, std::string& error) const;
};

#endif
