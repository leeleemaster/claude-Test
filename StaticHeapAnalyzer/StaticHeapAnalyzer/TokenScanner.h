#ifndef TOKEN_SCANNER_H
#define TOKEN_SCANNER_H

#include <string>
#include <vector>

struct TokenLine {
    int line;
    std::string text;   // 원본 소스 라인 (보고용)
    std::string clean;  // 주석/문자열/문자상수를 공백으로 치환한 라인 (매칭용)
};

class TokenScanner {
public:
    // 파일을 줄 단위로 읽고, 주석/문자열 제거본(clean)을 함께 채운다.
    // 줄번호는 원본 기준으로 1:1 보존된다.
    bool ScanFile(const std::string& path, std::vector<TokenLine>& outTokens, std::string& error) const;
};

#endif
