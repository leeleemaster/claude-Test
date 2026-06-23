#include "TokenScanner.h"

#include <fstream>
#include <sstream>

namespace {

enum State { NORMAL, LINE_C, BLOCK_C, STR, CH };

// 주석/문자열/문자상수 구간을 공백으로 치환한다.
// - 줄바꿈('\n')은 항상 보존하여 원본과 줄번호가 1:1 대응되게 한다.
// - 문자열/문자상수의 양끝 구분자(" ')는 유지하고 내부만 공백 처리한다.
// - 결과 문자열 길이는 입력과 동일하게 유지한다(열 위치 보존).
std::string StripCommentsAndStrings(const std::string& src) {
    std::string out;
    out.reserve(src.size());

    State state = NORMAL;
    const size_t n = src.size();

    for (size_t i = 0; i < n; ++i) {
        const char c = src[i];
        const char next = (i + 1 < n) ? src[i + 1] : '\0';

        switch (state) {
        case NORMAL:
            if (c == '/' && next == '/') {
                state = LINE_C;
                out += ' ';
                out += ' ';
                ++i;
            } else if (c == '/' && next == '*') {
                state = BLOCK_C;
                out += ' ';
                out += ' ';
                ++i;
            } else if (c == '"') {
                state = STR;
                out += '"';
            } else if (c == '\'') {
                state = CH;
                out += '\'';
            } else {
                out += c;
            }
            break;

        case LINE_C:
            if (c == '\n') {
                state = NORMAL;
                out += '\n';
            } else {
                out += ' ';
            }
            break;

        case BLOCK_C:
            if (c == '*' && next == '/') {
                state = NORMAL;
                out += ' ';
                out += ' ';
                ++i;
            } else if (c == '\n') {
                out += '\n';
            } else {
                out += ' ';
            }
            break;

        case STR:
            if (c == '\\') {
                // 이스케이프: 2글자 모두 공백 처리
                out += ' ';
                if (i + 1 < n && src[i + 1] != '\n') {
                    out += ' ';
                    ++i;
                }
            } else if (c == '"') {
                state = NORMAL;
                out += '"';
            } else if (c == '\n') {
                out += '\n';
            } else {
                out += ' ';
            }
            break;

        case CH:
            if (c == '\\') {
                out += ' ';
                if (i + 1 < n && src[i + 1] != '\n') {
                    out += ' ';
                    ++i;
                }
            } else if (c == '\'') {
                state = NORMAL;
                out += '\'';
            } else if (c == '\n') {
                out += '\n';
            } else {
                out += ' ';
            }
            break;
        }
    }

    return out;
}

void SplitLines(const std::string& s, std::vector<std::string>& lines) {
    lines.clear();
    std::string cur;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\n') {
            if (!cur.empty() && cur[cur.size() - 1] == '\r') {
                cur.erase(cur.size() - 1);
            }
            lines.push_back(cur);
            cur.clear();
        } else {
            cur += s[i];
        }
    }
    // 마지막 줄(개행 없이 끝나는 경우)
    if (!cur.empty()) {
        if (cur[cur.size() - 1] == '\r') {
            cur.erase(cur.size() - 1);
        }
        lines.push_back(cur);
    }
}

}  // namespace

bool TokenScanner::ScanFile(const std::string& path, std::vector<TokenLine>& outTokens, std::string& error) const {
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input.is_open()) {
        error = "cannot open file: " + path;
        return false;
    }

    std::ostringstream ss;
    ss << input.rdbuf();
    std::string content = ss.str();

    // UTF-8 BOM 제거
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        content.erase(0, 3);
    }

    const std::string cleaned = StripCommentsAndStrings(content);

    std::vector<std::string> rawLines;
    std::vector<std::string> cleanLines;
    SplitLines(content, rawLines);
    SplitLines(cleaned, cleanLines);

    outTokens.clear();
    for (size_t i = 0; i < rawLines.size(); ++i) {
        TokenLine token;
        token.line = static_cast<int>(i) + 1;
        token.text = rawLines[i];
        token.clean = (i < cleanLines.size()) ? cleanLines[i] : rawLines[i];
        outTokens.push_back(token);
    }

    return true;
}
