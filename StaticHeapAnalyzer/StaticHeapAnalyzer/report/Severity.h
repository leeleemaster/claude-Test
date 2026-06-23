#ifndef SEVERITY_H
#define SEVERITY_H

enum class Severity {
    CRITICAL,
    HIGH,
    MEDIUM,
    INFO,
    LOW
};

inline const char* SeverityToString(Severity severity) {
    switch (severity) {
    case Severity::CRITICAL: return "CRITICAL";
    case Severity::HIGH: return "HIGH";
    case Severity::MEDIUM: return "MEDIUM";
    case Severity::INFO: return "INFO";
    default: return "LOW";
    }
}

inline const char* SeverityToHtmlColor(Severity severity) {
    switch (severity) {
    case Severity::CRITICAL: return "#ff4d4f";
    case Severity::HIGH: return "#fa8c16";
    case Severity::MEDIUM: return "#fadb14";
    case Severity::INFO: return "#d9f7be";
    default: return "#91d5ff";
    }
}

// 정렬용 우선순위 (작을수록 심각)
inline int SeverityRank(Severity severity) {
    switch (severity) {
    case Severity::CRITICAL: return 0;
    case Severity::HIGH: return 1;
    case Severity::MEDIUM: return 2;
    case Severity::INFO: return 3;
    default: return 4;
    }
}

#endif
